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

void server_mode(boost::asio::io_context &io_context, unsigned short port)
{
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    while (true)
    {
        auto socket = std::make_shared<tcp::socket>(io_context);
        acceptor.accept(*socket);
        std::cout << "[Server-side] " << socket->remote_endpoint() << " established connection." << "\n";

        p2pfs::Connection conn(socket);
        // APPROVE FILE REQUEST AND SEND FILE
    }
}

void client_mode(const std::string &host, unsigned short port)
{
    boost::asio::io_context io_context;
    auto socket = std::make_shared<tcp::socket>(io_context);
    tcp::resolver resolver(io_context);

    boost::asio::connect(*socket, resolver.resolve(host, std::to_string(port)));
    std::cout << "[Client-side] Connected to " << host << ":" << port << "\n";

    p2pfs::Connection conn(socket);
    // REQUEST FILE FROM PEER
}

// Generates initial file records
int generateFileRecords(std::string sharedFilesDirPath)
{
    const std::string fileRecordsPath = "file_records.json";
    namespace fs = std::filesystem;
    if (!fs::exists(sharedFilesDirPath) || !fs::is_directory(sharedFilesDirPath))
    {
        std::cerr << "Invalid directory.\n";
        return 1;
    }

    nlohmann::json fileRecords = nlohmann::json::array();

    for (const auto &entry : fs::recursive_directory_iterator(sharedFilesDirPath))
    {
        if (fs::is_regular_file(entry))
        {
            std::string fullPath = entry.path().string();
            std::string relativePath = fs::relative(entry.path(), sharedFilesDirPath).string();
            uintmax_t fileSize = fs::file_size(entry);
            std::string sha256 = p2pfs::calculate_sha256(fullPath);

            fileRecords.push_back({{"filename", entry.path().filename().string()},
                                   {"filesize", fileSize},
                                   {"relative_path", relativePath},
                                   {"sha256", sha256}});
        }
    }

    std::ofstream out(fileRecordsPath);
    out << std::setw(4) << fileRecords << std::endl;

    return 0;
}

bool register_with_tracker(const std::string &tracker_ip, int tracker_port, const std::string &my_address)
{
    try
    {
        boost::asio::io_context io_context;
        auto socket = std::make_shared<tcp::socket>(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));

        p2pfs::Connection conn(socket);

        conn.sendJson({{"type", "register"},
                       {"address", my_address}});

        nlohmann::json response = conn.receiveJson();
        std::cout << "[Tracker] " << response.dump() << "\n";

        return response.contains("status") && response["status"] == "ok";
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Tracker] Register failed: " << e.what() << "\n";
        return false;
    }
}

void disconnect_from_tracker(const std::string &tracker_ip, int tracker_port, const std::string &my_address)
{
    try
    {
        boost::asio::io_context io_context;
        auto socket = std::make_shared<tcp::socket>(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));

        p2pfs::Connection conn(socket);

        conn.sendJson({{"type", "disconnect"},
                       {"address", my_address}});

        std::cout << "[Tracker] Disconnected.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Tracker] Disconnect failed: " << e.what() << "\n";
    }
}

void get_peers_from_tracker(const std::string &tracker_ip, int tracker_port, const std::string &my_address)
{
    std::vector<std::string> peers;

    try
    {
        boost::asio::io_context io_context;
        auto socket = std::make_shared<tcp::socket>(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(*socket, resolver.resolve(tracker_ip, std::to_string(tracker_port)));

        p2pfs::Connection conn(socket);

        conn.sendJson({{"type", "get_peers"}});

        nlohmann::json response = conn.receiveJson();

        if (response.contains("peers") && response["peers"].is_array())
        {
            for (const auto &peer : response["peers"])
            {
                if (peer.is_string())
                {
                    peers.push_back(peer.get<std::string>());
                }
            }

            std::cout << "[Tracker] Active peers:\n";
            for (const auto &peer : peers)
            {
                std::cout << " - " << peer;
                if (peer == my_address)
                    std::cout << " <-- You";
                std::cout << std::endl;
            }
        }
        else
        {
            std::cerr << "[Tracker] Invalid response from tracker.\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Tracker] Disconnect failed: " << e.what() << "\n";
    }
}

int main()
{
    // Initial startup
    const std::string fileRecordsPath = "file_records.json";
    if (!std::filesystem::exists(fileRecordsPath))
    {
        std::string sharedFilesDirPath;
        do
        {
            std::cout << "Enter a directory to share files from: ";
            std::getline(std::cin, sharedFilesDirPath);
        } while (generateFileRecords(sharedFilesDirPath));
    }

    const std::string tracker_ip = "127.0.0.1";
    const int tracker_port = 8129;

    unsigned short port;

    std::cout << "Enter port to listen on: ";
    std::cin >> port;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // flush newline from input buffer

    std::string my_address = "127.0.0.1:" + std::to_string(port);

    if (!register_with_tracker(tracker_ip, tracker_port, my_address))
    {
        std::cerr << "Failed to register with tracker. Exiting...\n";
        return 1;
    }

    boost::asio::io_context io_context;

    // Start server in separate thread
    std::thread server_thread([&]()
                              { server_mode(io_context, port); });

    std::string host;
    std::cout << "Enter host to connect to (or 'quit' to exit):\n>> ";
    while (std::getline(std::cin, host) && host != "quit")
    {
        if (host == "peers")
        {
            get_peers_from_tracker(tracker_ip, tracker_port, my_address);
            continue;
        }
        std::string output_filename;
        std::cout << "Enter output filename: ";
        std::getline(std::cin, output_filename);
        client_mode(host, port);
        std::cout << "Enter host to connect to (or 'quit' to exit):\n>> ";
    }

    disconnect_from_tracker(tracker_ip, tracker_port, my_address);

    io_context.stop();
    server_thread.join();
    return 0;
}
