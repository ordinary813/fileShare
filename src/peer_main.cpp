#include "p2pfs/connection.hpp"
#include "p2pfs/crypto.hpp"
#include "p2pfs/peer.hpp"

#include "p2pfs/debug.hpp"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>

using boost::asio::ip::tcp;
using json = nlohmann::json;
namespace fs = std::filesystem;

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

void cmd_loop(p2pfs::Peer &peer)
{
    std::string cmd;
    std::cout << "Commands: peers | list | send | download | quit\n> ";
    while (std::getline(std::cin, cmd))
    {
        if (cmd == "quit")
            break;
        if (cmd == "peers")
        {
            auto peers = get_peers_from_tracker(peer.getTrackerIP(), peer.getTrackerPort());
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
            try
            {
                boost::asio::io_context ci;
                tcp::resolver resolver(ci);
                tcp::socket sock(ci);
                boost::asio::connect(sock, resolver.resolve(target, portstr));
                auto s = std::make_shared<tcp::socket>(std::move(sock));
                p2pfs::Connection conn(s);
                p2pfs::debug("(CMD_LOOP) Sending file.");

                conn.sendJson({{"type", "file"}, {"relative_path", rel}});
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
                conn.sendJson(
                    {
                        {"type", "download"}, 
                        {"relative_path", rel}
                    });
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
}

int main()
{
    const std::string tracker_ip = "127.0.0.1";
    const int tracker_port = 8129;

    std::string const shared_dir = "shared_files";
    std::string const download_dir = "downloads";

    // ensure directories exist
    fs::create_directories(shared_dir);
    fs::create_directories(download_dir);

    // Update file records
    if(generateFileRecords(shared_dir))
    {
        std::cout << "File record generation failed!" << std::endl;
        return 1;
    }

    unsigned short port;
    std::cout << "Enter port to listen on: ";
    std::cin >> port;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');


    p2pfs::Peer peer(tracker_ip, tracker_port, port);
    p2pfs::Peer::set_instance(&peer);       // set the singleton for this process


    try {
        peer.start();
        cmd_loop(peer);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    peer.stop();
    return 0;
}
