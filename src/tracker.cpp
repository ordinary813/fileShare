#include "p2pfs/connection.hpp"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

using boost::asio::ip::tcp;
using json = nlohmann::json;

std::unordered_set<std::string> peer_list;
const std::string peer_file = "peers.json";
std::mutex peer_mutex;

void load_peers_from_file()
{
    std::ifstream infile(peer_file);
    if (!infile)
        return;
    json j;
    infile >> j;
    if (j.contains("peers") && j["peers"].is_array())
    {
        std::lock_guard lock(peer_mutex);
        for (auto &p : j["peers"])
            peer_list.insert(p.get<std::string>());
    }
}

void save_peers_to_file()
{
    json j;
    {
        std::lock_guard lock(peer_mutex);
        j["peers"] = json::array();
        for (auto &p : peer_list)
            j["peers"].push_back(p);
    }
    std::ofstream out(peer_file);
    out << j.dump(4) << std::endl;
}

bool is_peer_alive(const std::string &addr)
{
    try
    {
        auto colon = addr.find(':');
        auto ip = addr.substr(0, colon);
        auto port = addr.substr(colon + 1);
        boost::asio::io_context io;
        tcp::socket sock(io);
        tcp::resolver resolver(io);
        boost::asio::connect(sock, resolver.resolve(ip, port));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void handle_client(std::shared_ptr<tcp::socket> socket)
{
    try
    {
        p2pfs::Connection conn(socket);

        json req = conn.receiveJson();
        json resp;

        std::string type = req.value("type", "");
        if (type == "register")
        {
            std::string addr = req["address"];
            {
                std::lock_guard lock(peer_mutex);
                peer_list.insert(addr);
            }
            save_peers_to_file();
            resp["status"] = "ok";
            std::cout << "Registered " << addr << "\n";
        }
        else if (type == "get_peers")
        {
            json arr = json::array();
            std::vector<std::string> to_erase;
            {
                std::lock_guard lock(peer_mutex);
                for (auto &p : peer_list)
                {
                    if (is_peer_alive(p))
                        arr.push_back(p);
                    else
                        to_erase.push_back(p);
                }
                for (auto &e : to_erase)
                    peer_list.erase(e);
            }
            if (!to_erase.empty())
                save_peers_to_file();
            resp["peers"] = arr;
        }
        else if (type == "disconnect")
        {
            std::string addr = req["address"];
            {
                std::lock_guard lock(peer_mutex);
                peer_list.erase(addr);
            }
            save_peers_to_file();
            resp["status"] = "disconnected";
            std::cout << "Disconnected " << addr << "\n";
        }
        else
        {
            resp["error"] = "unknown";
        }
        std::string s = resp.dump() + "\n";
        boost::asio::write(*socket, boost::asio::buffer(s));
    }
    catch (const std::exception &e)
    {
        std::cerr << "tracker client error: " << e.what() << "\n";
    }
}

int main()
{
    try
    {
        load_peers_from_file();
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8129));
        std::cout << "Tracker listening on 8129\n";
        while (true)
        {
            auto socket = std::make_shared<tcp::socket>(io);
            acceptor.accept(*socket);
            std::thread(handle_client, socket).detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "tracker fatal: " << e.what() << "\n";
    }
    return 0;
}
