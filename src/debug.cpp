#include "p2pfs/debug.hpp"

namespace p2pfs
{
    bool debug_enabled = true;

    void print_buffer(const char *buf, size_t len)
    {
        const std::string RED = "\033[31m";
        const std::string RESET = "\033[0m";
        if (debug_enabled)
        {
            std::cout << RED << "Buffer content:" << std::endl;
            for (size_t i = 0; i < len; ++i)
            {
                std::cout << buf[i];
            }
            std::cout << std::dec << RESET << std::endl;
        }
    }
}
