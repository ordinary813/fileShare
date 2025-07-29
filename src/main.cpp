#include "p2pfs/connection.hpp"
#include "p2pfs/crypto.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <vector>
#include <fstream>

using boost::asio::ip::tcp;

void server_mode(boost::asio::io_context &io_context, unsigned short port)
{
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Server listening on port " << port << "\n";

    while (true)
    {
        auto socket = std::make_shared<tcp::socket>(io_context);
        acceptor.accept(*socket);
        std::cout << "Client connected: " << socket->remote_endpoint() << "\n";

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
    std::cout << "Connected to " << host << ":" << port << "\n";

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

    unsigned short port;
    std::string filename;

    boost::asio::io_context io_context;

    std::cout << "Enter port to listen on: ";
    std::cin >> port;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // flush newline from input buffer

    // Start server in separate thread
    std::thread server_thread([&]()
                              { server_mode(io_context, port); });

    std::string host, output_filename;
    std::cout << "Enter host to connect to (or 'quit' to exit): ";
    while (std::getline(std::cin, host) && host != "quit")
    {
        std::cout << "Enter output filename: ";
        std::getline(std::cin, output_filename);
        client_mode(host, port);
        std::cout << "Enter host to connect to (or 'quit' to exit): ";
    }

    io_context.stop();
    server_thread.join();
    return 0;
}
