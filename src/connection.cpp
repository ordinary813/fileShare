#include "p2pfs/connection.hpp"
#include <iostream>
#include <fstream>

namespace p2pfs
{
    Connection::Connection(std::shared_ptr<Socket> socket)
        : socket_(std::move(socket)) {}

    void Connection::sendFile(const std::string &filepath)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "Error: Cannot open file " << filepath << "\n";
            return;
        }

        char buffer[CHUNK_SIZE];
        while (file.read(buffer, CHUNK_SIZE) || file.gcount() > 0)
        {
            boost::asio::write(*socket_, boost::asio::buffer(buffer, file.gcount()));
        }

        std::cout << "Finished sending file: " << filepath << "\n";
    }

    void Connection::receiveFile(const std::string &output_filename)
    {
        std::ofstream file(output_filename, std::ios::binary);
        if (!file)
        {
            std::cerr << "Error: Cannot create file " << output_filename << "\n";
            return;
        }

        char buffer[CHUNK_SIZE];
        boost::system::error_code error;
        std::size_t len;
        while ((len = socket_->read_some(boost::asio::buffer(buffer), error)) > 0)
        {
            file.write(buffer, len);
        }

        if (error != boost::asio::error::eof)
        {
            std::cerr << "Receive error: " << error.message() << "\n";
        }
        else
        {
            std::cout << "File received and saved to: " << output_filename << "\n";
        }
    }

    void Connection::requestFile(const std::string &requestedFile, const std::string &output_filename)
    {
        
    }

    void Connection::sendJson(const nlohmann::json &message)
    {
        std::string msg = message.dump() + "\n"; // newline-delimited protocol
        boost::asio::write(*socket_, boost::asio::buffer(msg));
    }

    nlohmann::json Connection::receiveJson()
    {
        boost::asio::streambuf buffer;
        boost::asio::read_until(*socket_, buffer, "\n");
        std::istream is(&buffer);
        nlohmann::json json_data;
        is >> json_data;
        return json_data;
    }
}