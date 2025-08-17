#include "p2pfs/peer.hpp"
#include "p2pfs/connection.hpp"

#include <filesystem>
#include <csignal>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;
using boost::asio::ip::tcp;

namespace p2pfs{
    // Constructor
    Peer::Peer(std::string tracker_ip, int tracker_port, unsigned short listen_port)
        : tracker_ip_(std::move(tracker_ip)), tracker_port_(tracker_port),
        port_(listen_port), running_(true) {}

    // Singleton
    Peer* Peer::instance() { return s_instance; }
    void Peer::set_instance(Peer* p) { s_instance = p; }

    void Peer::start() {
        // Register with tracker
        my_address_ = "127.0.0.1:" + std::to_string(port_);
        if (!register_with_tracker()) {
            throw std::runtime_error("Failed to register with tracker");
        }

        // Start server thread
        server_thread_ = std::thread([this]() { server_mode(); });

        // Setup signal handler for graceful shutdown
        std::signal(SIGINT, [](int) {
            if (Peer::instance()) Peer::instance()->stop();
        });

        std::cout << "Peer started on port " << port_ << "\n";
    }

    void Peer::stop() {
        if (!running_) return;
        running_ = false;

        disconnect_from_tracker();

        // Create dummy connection to unblock accept
        try {
            tcp::socket sock(io_);
            tcp::resolver resolver(io_);
            boost::asio::connect(sock, resolver.resolve("127.0.0.1", std::to_string(port_)));
        } catch (...) {}

        io_.stop();
        if (server_thread_.joinable())
            server_thread_.join();

        std::cout << "Peer stopped\n";
        //std::exit(0);
    }

    // Server loop 
    void Peer::server_mode() {
        tcp::acceptor acceptor(io_, tcp::endpoint(tcp::v4(), port_));
        while (running_) {
            auto socket = std::make_shared<tcp::socket>(io_);
            try {
                acceptor.accept(*socket);
            } catch (...) {
                break;
            }

            // Handle connection in detached thread
            std::thread([socket]() {
                p2pfs::Connection conn(socket);
                try {
                    json req = conn.receiveJson();
                    std::string type = req.value("type", "");
                    if (type == "list_files") {
                        std::ifstream ifs("file_records.json");
                        json records;
                        ifs >> records;
                        conn.sendJson(records);
                    } else if (type == "download") {
                        std::string rel = req["relative_path"];
                        std::string path = (fs::path("shared_files") / rel).string();
                        conn.sendFile(path);
                    }
                } catch (...) {}
            }).detach();
        }
    }

    bool Peer::register_with_tracker() {
        try {
            boost::asio::io_context io;
            auto socket = std::make_shared<tcp::socket>(io);
            tcp::resolver resolver(io);
            boost::asio::connect(*socket, resolver.resolve(tracker_ip_, std::to_string(tracker_port_)));

            p2pfs::Connection conn(socket);
            conn.sendJson({{"type", "register"}, {"address", my_address_}});
            json resp = conn.receiveJson();
            return resp.value("status", "") == "ok";
        } catch (...) {
            return false;
        }
    }

    void Peer::disconnect_from_tracker() {
        try {
            boost::asio::io_context io;
            auto socket = std::make_shared<tcp::socket>(io);
            tcp::resolver resolver(io);
            boost::asio::connect(*socket, resolver.resolve(tracker_ip_, std::to_string(tracker_port_)));
            p2pfs::Connection conn(socket);
            conn.sendJson({{"type", "disconnect"}, {"address", my_address_}});
        } catch (...) {}
    }
}