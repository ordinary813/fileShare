#include "p2pfs/connection.hpp"
#include "p2pfs/crypto.hpp"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <thread>
#include <filesystem>
#include <vector>
#include <fstream>

using boost::asio::ip::tcp;
using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * 
 * @brief Listening loop.
 * 
 * Used asynchronously for listening to incoming connections to our peer.
 * 
 * @param io_context the io context of the current session (local).
 * @param port Listen to incoming connections on this port.
 * 
 */
void server_mode(boost::asio::io_context &io_context, unsigned short port)
{
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    while (true)
    {
        auto socket = std::make_shared<tcp::socket>(io_context);
        acceptor.accept(*socket);
        std::thread([socket]()
                    {
            p2pfs::Connection conn(socket);
            try {
                json req = conn.receiveJson();
                std::string type = req.value("type", "");
                if (type == "list_files") {
                    // send our file_records.json
                    std::ifstream ifs("file_records.json");
                    json records;
                    ifs >> records;
                    conn.sendJson(records);
                } else if (type == "download") {
                    std::string rel = req["relative_path"];
                    std::string path = (fs::path("shared_files") / rel).string();
                    conn.sendFile(path);
                } else {
                    // unknown: ignore
                }
            } catch (...) {} })
            .detach();
    }
}

bool register_with_tracker(const std::string &tracker_ip, int tracker_port, const std::string &my_address)
{
    try
    {
        boost::asio::io_context io;
        auto socket = std::make_shared<tcp::socket>(io);
        tcp::resolver resolver(io);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));
        p2pfs::Connection conn(socket);
        conn.sendJson({{"type", "register"}, {"address", my_address}});
        json resp = conn.receiveJson();
        return resp.value("status", "") == "ok";
    }
    catch (...)
    {
        return false;
    }
}

void disconnect_from_tracker(const std::string &tracker_ip, int tracker_port, const std::string &my_address)
{
    try
    {
        boost::asio::io_context io;
        auto socket = std::make_shared<tcp::socket>(io);
        tcp::resolver resolver(io);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));
        p2pfs::Connection conn(socket);
        conn.sendJson({{"type", "disconnect"}, {"address", my_address}});
    }
    catch (...)
    {
    }
}

std::vector<std::string> get_peers_from_tracker(const std::string &tracker_ip, int tracker_port)
{
    std::vector<std::string> peers;
    try
    {
        boost::asio::io_context io;
        auto socket = std::make_shared<tcp::socket>(io);
        tcp::resolver resolver(io);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));
        p2pfs::Connection conn(socket);
        conn.sendJson({{"type", "get_peers"}});
        json resp = conn.receiveJson();
        if (resp.contains("peers") && resp["peers"].is_array())
        {
            for (auto &p : resp["peers"])
                peers.push_back(p.get<std::string>());
        }
    }
    catch (...)
    {
    }
    return peers;
}

int generateFileRecords(const std::string &sharedDir)
{
    if (!fs::exists(sharedDir) || !fs::is_directory(sharedDir))
        return 1;
    json fileRecords = json::array();
    for (auto &entry : fs::recursive_directory_iterator(sharedDir))
    {
        if (!fs::is_regular_file(entry))
            continue;
        std::string full = entry.path().string();
        std::string rel = fs::relative(entry.path(), sharedDir).string();
        uintmax_t size = fs::file_size(entry);
        std::string sha = p2pfs::calculate_sha256(full);
        fileRecords.push_back({{"filename", entry.path().filename().string()},
                               {"filesize", size},
                               {"relative_path", rel},
                               {"sha256", sha}});
    }
    std::ofstream out("file_records.json");
    out << fileRecords.dump(4) << std::endl;
    return 0;
}

json read_file_records()
{
    std::ifstream ifs("file_records.json");
    json j;
    ifs >> j;
    return j;
}

int main()
{
    // ensure directories exist
    fs::create_directories("shared_files");
    fs::create_directories("downloads");

    if (!fs::exists("file_records.json"))
    {
        std::string dir;
        do
        {
            std::cout << "Enter directory to share files from (relative to shared_files/ or absolute): ";
            std::getline(std::cin, dir);
            // allow user to place files in shared_files or point elsewhere
            if (dir.empty())
                dir = "shared_files";
        } while (generateFileRecords(dir));
    }

    const std::string tracker_ip = "127.0.0.1";
    const int tracker_port = 8129;

    unsigned short port;
    std::cout << "Enter port to listen on: ";
    std::cin >> port;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string my_address = "127.0.0.1:" + std::to_string(port);

    if (!register_with_tracker(tracker_ip, tracker_port, my_address))
    {
        std::cerr << "Failed to register with tracker\n";
        return 1;
    }

    boost::asio::io_context io;
    std::thread server_thr([&]()
                           { server_mode(io, port); });

    std::string cmd;
    std::cout << "Commands: peers | list | send | download | quit\n> ";
    while (std::getline(std::cin, cmd))
    {
        if (cmd == "quit")
            break;
        if (cmd == "peers")
        {
            auto peers = get_peers_from_tracker(tracker_ip, tracker_port);
            std::cout << "Peers:\n";
            for (auto &p : peers)
                std::cout << " - " << p << "\n";
        }
        else if (cmd == "list")
        {
            auto recs = read_file_records();
            int i = 1;
            for (auto &e : recs)
            {
                std::cout << i++ << ". " << e["filename"].get<std::string>() << " (" << e["filesize"].get<uint64_t>() << ")\n";
            }
        }
        else if (cmd == "send")
        {
            std::string target, portstr;
            std::cout << "Peer ip: ";
            std::getline(std::cin, target);
            std::cout << "Peer port: ";
            std::getline(std::cin, portstr);
            auto recs = read_file_records();
            std::cout << "Choose file index: ";
            std::string idxs;
            std::getline(std::cin, idxs);
            int idx = std::stoi(idxs) - 1;
            std::string rel = recs[idx]["relative_path"].get<std::string>();
            // connect and upload: we implement a client that sends a "download" request? For upload, just connect and send file (server will call receive)
            try
            {
                boost::asio::io_context ci;
                tcp::resolver resolver(ci);
                tcp::socket sock(ci);
                boost::asio::connect(sock, resolver.resolve(target, portstr));
                auto s = std::make_shared<tcp::socket>(std::move(sock));
                p2pfs::Connection conn(s);
                // send a header to tell peer we will send a file
                // We'll send: {"type":"upload"} then sendFile
                conn.sendJson({{"type", "upload"}, {"relative_path", rel}});
                std::string path = (fs::path("shared_files") / rel).string();
                conn.sendFile(path);
            }
            catch (const std::exception &e)
            {
                std::cerr << "send error: " << e.what() << "\n";
            }
        }
        else if (cmd == "download")
        {
            std::string target, portstr;
            std::cout << "Peer ip: ";
            std::getline(std::cin, target);
            std::cout << "Peer port: ";
            std::getline(std::cin, portstr);
            // ask peer to list files
            try
            {
                boost::asio::io_context ci;
                tcp::resolver resolver(ci);
                tcp::socket sock(ci);
                boost::asio::connect(sock, resolver.resolve(target, portstr));
                auto s = std::make_shared<tcp::socket>(std::move(sock));
                p2pfs::Connection conn(s);
                conn.sendJson({{"type", "list_files"}});
                json remote_files = conn.receiveJson(); // expect array
                int i = 1;
                for (auto &e : remote_files)
                {
                    std::cout << i++ << ". " << e["filename"].get<std::string>() << "\n";
                }
                std::cout << "Choose file index: ";
                std::string idxs;
                std::getline(std::cin, idxs);
                int idx = std::stoi(idxs) - 1;
                std::string rel = remote_files[idx]["relative_path"].get<std::string>();
                // request download
                conn.sendJson({{"type", "download"}, {"relative_path", rel}});
                conn.receiveFile("downloads");
            }
            catch (const std::exception &e)
            {
                std::cerr << "download error: " << e.what() << "\n";
            }
        }
        else
        {
            std::cout << "unknown\n";
        }
        std::cout << "> ";
    }

    disconnect_from_tracker(tracker_ip, tracker_port, my_address);
    io.stop();
    server_thr.join();
    return 0;
}
