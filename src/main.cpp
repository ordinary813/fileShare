#include "p2pfs/connection.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

using boost::asio::ip::tcp;

void server_mode(boost::asio::io_context& io_context, unsigned short port, const std::string& filename) {
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Server listening on port " << port << "\n";

    while (true) {
        auto socket = std::make_shared<tcp::socket>(io_context);
        acceptor.accept(*socket);
        std::cout << "Client connected: " << socket->remote_endpoint() << "\n";

        p2pfs::Connection conn(socket);
        conn.sendFile(filename);
    }
}

void client_mode(const std::string& host, unsigned short port, const std::string& output_filename) {
    boost::asio::io_context io_context;
    auto socket = std::make_shared<tcp::socket>(io_context);
    tcp::resolver resolver(io_context);

    boost::asio::connect(*socket, resolver.resolve(host, std::to_string(port)));
    std::cout << "Connected to " << host << ":" << port << "\n";

    p2pfs::Connection conn(socket);
    conn.receiveFile(output_filename);
}

int main() {
    unsigned short port;
    std::string filename = "test.txt";

    boost::asio::io_context io_context;

    std::cout << "Enter port to listen on: ";
    std::cin >> port;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');     //flush newline from input buffer

    // Start server in separate thread
    std::thread server_thread([&]() {
        server_mode(io_context, port, filename);
    });

    std::string host, output_filename;
    std::cout << "Enter host to connect to (or 'quit' to exit): ";
    while (std::getline(std::cin, host) && host != "quit") {
        std::cout << "Enter output filename: ";
        std::getline(std::cin, output_filename);
        client_mode(host, port, output_filename);
        std::cout << "Enter host to connect to (or 'quit' to exit): ";
    }

    io_context.stop();
    server_thread.join();
    return 0;
}
