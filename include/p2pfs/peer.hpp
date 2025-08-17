#pragma once
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <thread>
#include <atomic>
#include <string>

namespace p2pfs
{
    class Peer {
    public:
        Peer(std::string tracker_ip, int tracker_port, unsigned short listen_port);
        
        void start();
        void stop();

        // Access singleton instance (for signal handler)
        static Peer* instance();
        static void set_instance(Peer* p);

    private:
        void server_mode();
        bool register_with_tracker();
        void disconnect_from_tracker();

    private:
        std::string tracker_ip_;
        int tracker_port_;
        unsigned short port_;
        std::string my_address_;

        std::atomic<bool> running_;
        boost::asio::io_context io_;
        std::thread server_thread_;

        static inline Peer* s_instance = nullptr;
    };
}
