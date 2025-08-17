#include "p2pfs/connection.hpp"
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>

namespace p2pfs
{
    Connection::Connection(std::shared_ptr<Socket> socket)
        : socket_(std::move(socket)) {}

    // Writes the json as a string into the socket buffer
    void Connection::sendJson(const json &message)
    {
        std::string msg = message.dump() + "\n";
        boost::asio::write(*socket_, boost::asio::buffer(msg));
    }

    // Reads from the socket buffer until '\n'
    json Connection::receiveJson()
    {
        boost::asio::streambuf buf;
        boost::asio::read_until(*socket_, buf, "\n");
        std::istream is(&buf);
        json j;
        is >> j;
        return j;
    }

    bool Connection::sendFile(const std::string &filepath)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "sendFile: Cannot open " << filepath << "\n";
            return false;
        }
        file.seekg(0, std::ios::end);
        uint64_t filesize = static_cast<uint64_t>(file.tellg());
        file.seekg(0);

        // send header
        json hdr = {
            {"type", "file"},
            {"filename", std::filesystem::path(filepath).filename().string()},
            {"filesize", filesize}};
        sendJson(hdr);

        // send raw bytes
        std::vector<char> buf(CHUNK_SIZE);
        while (file.read(buf.data(), buf.size()) || file.gcount())
        {
            boost::asio::write(*socket_, boost::asio::buffer(buf.data(), file.gcount()));
        }
        return true;
    }

    bool Connection::receiveFile(const std::string &output_dir)
    {
        // first read JSON header
        json hdr = receiveJson();
        if (!hdr.contains("type") || hdr["type"] != "file")
        {
            std::cerr << "receiveFile: expected file header\n";
            return false;
        }
        std::string filename = hdr["filename"];
        uint64_t filesize = hdr["filesize"];

        std::filesystem::create_directories(output_dir);
        std::string outpath = (std::filesystem::path(output_dir) / filename).string();

        std::ofstream out(outpath, std::ios::binary);
        if (!out)
        {
            std::cerr << "receiveFile: cannot open " << outpath << "\n";
            return false;
        }

        uint64_t bytes_read = 0;
        std::vector<char> buf(CHUNK_SIZE);
        boost::system::error_code ec;
        while (bytes_read < filesize)
        {
            size_t toread = static_cast<size_t>(std::min<uint64_t>(CHUNK_SIZE, filesize - bytes_read));
            size_t n = boost::asio::read(*socket_, boost::asio::buffer(buf.data(), toread), ec);
            if (n == 0)
                break;
            out.write(buf.data(), n);
            bytes_read += n;
        }
        out.close();
        if (bytes_read != filesize)
        {
            std::cerr << "receiveFile: expected " << filesize << " got " << bytes_read << "\n";
            return false;
        }
        std::cout << "Saved file to " << outpath << "\n";
        return true;
    }
}
