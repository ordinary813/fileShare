#include "p2pfs/connection.hpp"
#include <iostream>
#include <fstream>

namespace p2pfs{
    Connection::Connection(std::shared_ptr<Socket> socket)
    : socket_(std::move(socket)) {}

    void Connection::sendFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Cannot open file " << filename << "\n";
            return;
        }

        char buffer[CHUNK_SIZE];
        while (file.read(buffer, CHUNK_SIZE) || file.gcount() > 0) {
            boost::asio::write(*socket_, boost::asio::buffer(buffer, file.gcount()));
        }

        std::cout << "Finished sending file: " << filename << "\n";
    }

    void Connection::receiveFile(const std::string& output_filename) {
        std::ofstream file(output_filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Cannot create file " << output_filename << "\n";
            return;
        }

        char buffer[CHUNK_SIZE];
        boost::system::error_code error;
        std::size_t len;
        while ((len = socket_->read_some(boost::asio::buffer(buffer), error)) > 0) {
            file.write(buffer, len);
        }

        if (error != boost::asio::error::eof) {
            std::cerr << "Receive error: " << error.message() << "\n";
        } else {
            std::cout << "File received and saved to: " << output_filename << "\n";
        }
    }
}