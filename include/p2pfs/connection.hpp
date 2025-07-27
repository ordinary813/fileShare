#pragma once

#include <string>
#include <boost/asio.hpp>
#include <memory>

namespace p2pfs {
    class Connection{
        public:
            using Socket = boost::asio::ip::tcp::socket;

            Connection(std::shared_ptr<Socket> socket);

            void sendFile(const std::string& filename);
            void receiveFile(const std::string& output_filename);

        private:
            std::shared_ptr<Socket> socket_;
            static constexpr std::size_t CHUNK_SIZE = 1024;
    };
}