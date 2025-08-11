#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

namespace p2pfs
{
    using json = nlohmann::json;
    class Connection
    {
    public:
        using Socket = boost::asio::ip::tcp::socket;
        Connection(std::shared_ptr<Socket> socket);

        void sendJson(const json &message);
        json receiveJson();

        bool sendFile(const std::string &filepath);
        bool receiveFile(const std::string &output_dir);

    private:
        std::shared_ptr<Socket> socket_;
        static constexpr std::size_t CHUNK_SIZE = 16 * 1024;
    };
}
