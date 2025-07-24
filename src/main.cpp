#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;

const size_t CHUNK_SIZE = 1024;

void send_file(tcp::socket& socket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }

    char buffer[CHUNK_SIZE];
    while (file.read(buffer, CHUNK_SIZE)) {
        boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount()));
    }
    boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount())); //Send remaining bytes
    file.close();
}

void receive_file(tcp::socket& socket, const std::string& output_filename) {
    std::ofstream file(output_filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file " << output_filename << std::endl;
        return;
    }

    char buffer[CHUNK_SIZE];
    boost::system::error_code error;
    size_t len;
    while ((len = socket.read_some(boost::asio::buffer(buffer), error)) > 0) {
        file.write(buffer, len);
    }
    file.close();
    if (error != boost::asio::error::eof) {
        std::cerr << "Error during receive: " << error.message() << std::endl;
    }
}

// Server mode: Listen for incoming connections
void server_mode(boost::asio::io_context& io_context, unsigned short port, const std::string& filename) {
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        std::cout << "Connected to: " << socket.remote_endpoint().address().to_string() << std::endl;

        send_file(socket, filename);
        socket.close();
    }
}

// Client mode: Connect to a peer and request a file
void client_mode(const std::string& host, unsigned short port, const std::string& output_filename) {
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
    std::cout << "Connected to " << host << ":" << port << std::endl;

    receive_file(socket, output_filename);
    socket.close();
}

int main() {
    boost::asio::io_context io_context;
    unsigned short port = 8080;
    std::string filename = "test.txt";

    // Start the server side thread
    std::thread server_thread([&]() { server_mode(io_context, port, filename); });

    // Main thread is client side
    std::string host, output_filename;
    std::cout << "Enter host to connect to (or 'quit' to exit): ";
    while (std::getline(std::cin, host) && host != "quit") {
        std::cout << "Enter output filename: ";
        std::getline(std::cin, output_filename);
        client_mode(host, 8080, output_filename);
        std::cout << "Enter host to connect to (or 'quit' to exit): ";
    }

    // End of program
    io_context.stop();
    server_thread.join();
    return 0;
}