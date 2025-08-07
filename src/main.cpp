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
        //CHECK
        nlohmann::json req = conn.receiveJson();

        if (req.contains("type") && req["type"] == "download_request")
        {
            std::string filename = req["filename"];
            conn.sendFile("shared_files/" + filename);
        }
        else
        {
            conn.receiveFile();
        }
        //
    }
}

void client_mode(const std::string &host, unsigned short port, std::string action, std::string filepath = "")
{
    boost::asio::io_context io_context;
    auto socket = std::make_shared<tcp::socket>(io_context);
    tcp::resolver resolver(io_context);

    boost::asio::connect(*socket, resolver.resolve(host, std::to_string(port)));
    std::cout << "[Client-side] Connected to " << host << ":" << port << "\n";

    p2pfs::Connection conn(socket);

    if (action == "send")
    {
        conn.sendFile(filepath);
    }
    else if (action == "download")
    {
        // CHECK
        conn.sendJson({{"type", "download_request"}, {"filename", filepath}});
        conn.receiveFile();
        //
    }
    else
    {
        std::cerr << "Invalid aciton.\n";
        return;
    }
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

nlohmann::json printFileRecords()
{
    std::ifstream ifs("file_records.json");
    if (!ifs.is_open())
    {
        std::cerr << "Failed to open file_records.json\n";
        return 1;
    }

    nlohmann::json fileRecords;
    try
    {
        ifs >> fileRecords;
    }
    catch (const std::exception &e)
    {
        std::cerr << "JSON parse error: " << e.what() << '\n';
        return 1;
    }

    int counter = 1;

    for (auto entry : fileRecords)
    {
        if (entry.contains("filename") && entry["filename"].is_string())
        {
            std::cout << counter << ". " << entry["filename"] << '\n';
        }
        else
        {
            std::cout << counter << ". [Invalid entry]\n";
        }
        counter++;
    }
    return fileRecords;
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

    // Tracker registration
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

    // Peer logic
    boost::asio::io_context io_context;

    std::thread server_thread([&]()
                              { server_mode(io_context, port); });

    std::string query;
    std::string host, hostPort;
    std::cout << "Select an action:\n1.peers\n2.send\n3.download\n4.quit\n>> ";

    while (std::getline(std::cin, query) && query != "quit")
    {
        if (query == "peers")
        {
            get_peers_from_tracker(tracker_ip, tracker_port, my_address);
            continue;
        }
        else if (query == "send")
        {
            std::cout << "Enter peer to connect: ";
            std::getline(std::cin, host);
            std::cout << "Port: ";
            std::getline(std::cin, hostPort);

            std::cout << "What file would you like to send?\n";
            nlohmann::json fileRecords = printFileRecords();

            std::cout << "\n>> ";
            std::string selected_file;
            std::getline(std::cin, selected_file);
            int file_index = std::stoi(selected_file) - 1;

            client_mode(host, std::stoi(hostPort), "send", fileRecords[file_index]["relative_path"]);

            host = "";
            hostPort = "";
        }
        else if (query == "download")
        {
            std::cout << "Enter peer to connect: ";
            std::getline(std::cin, host);
            std::cout << "Port: ";
            std::getline(std::cin, hostPort);

            std::cout << "What file would you like to download?\n";
            // LIST CONNECTION'S FILES
            std::cout << "\n>> ";
            std::string selected_file;
            std::getline(std::cin, selected_file);
            int file_index = std::stoi(selected_file) - 1;

            client_mode(host, std::stoi(hostPort), "download" /*, CHOOSE HOW TO PASS THE SELECTED FILE*/);

            host = "";
            hostPort = "";
        }
        else
        {
            std::cerr << "Invalid query, please use an action from the menu." << std::endl;
        }

        std::cout << "Select an action:\n1.peers\n2.send\n3.download\n4.quit\n>> ";
    }

    disconnect_from_tracker(tracker_ip, tracker_port, my_address);

    io_context.stop();
    server_thread.join();
    return 0;
}
