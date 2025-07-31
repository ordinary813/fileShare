#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <iostream>
#include <fstream>

#define PORT 8129

using boost::asio::ip::tcp;
using json = nlohmann::json;

std::unordered_set<std::string> peer_list;
const std::string peer_file = "peers.json";

void load_peers_from_file()
{
    std::ifstream infile(peer_file);
    if (infile)
    {
        json j;
        infile >> j;
        for (const auto &peer : j["peers"])
        {
            peer_list.insert(peer.get<std::string>());
        }
    }
}

void save_peers_to_file()
{
    json j;
    j["peers"] = json::array();
    for (const auto &peer : peer_list)
    {
        j["peers"].push_back(peer);
    }
    std::ofstream outfile(peer_file);
    outfile << j.dump(4);
}

bool is_peer_alive(const std::string &address)
{
    try
    {
        size_t colon = address.find(':');
        std::string ip = address.substr(0, colon);
        int port = std::stoi(address.substr(colon + 1));

        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);

        boost::asio::connect(socket, resolver.resolve(ip, std::to_string(port)));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void handle_client(tcp::socket socket)
{
    try
    {
        boost::asio::streambuf buf;
        boost::asio::read_until(socket, buf, "\n");

        std::istream is(&buf);
        json request;
        is >> request;

        json response;

        if (request["type"] == "register")
        {
            std::string peer = request["address"];
            peer_list.insert(peer);
            save_peers_to_file();
            std::cout << peer << " connected" << std::endl;
            response["status"] = "ok";
        }
        else if (request["type"] == "get_peers")
        {
            response["peers"] = json::array();
            std::unordered_set<std::string> unreachable;

            for (const auto &peer : peer_list)
            {
                if (is_peer_alive(peer))
                {
                    response["peers"].push_back(peer);
                }
                else
                {
                    unreachable.insert(peer);
                }
            }

            for (const auto &peer : unreachable)
            {
                peer_list.erase(peer);
            }

            save_peers_to_file();
        }
        else if (request["type"] == "disconnect")
        {
            std::string peer = request["address"];
            peer_list.erase(peer);
            save_peers_to_file();
            std::cout << peer << " disconnected" << std::endl;
            response["status"] = "disconnected";
        }
        else
        {
            response["error"] = "Unknown request type";
        }

        std::string resp = response.dump() + "\n";
        boost::asio::write(socket, boost::asio::buffer(resp));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        load_peers_from_file();

        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), PORT));
        std::cout << "Tracker running on port " << PORT << std::endl;

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::thread(handle_client, std::move(socket)).detach(); // concurrent handling
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal tracker error: " << e.what() << std::endl;
    }
}
