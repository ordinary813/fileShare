#pragma once
#include <iostream>
#include <nlohmann/json.hpp>

namespace p2pfs
{

    extern bool debug_enabled; // declaration (defined in .cpp)

    void print_buffer(const char *buf, size_t len);
    
    // Base case
    inline void debug_impl()
    {
        std::cout << std::endl;
    }

    // Variadic template
    template <typename T, typename... Args>
    void debug_impl(T &&first, Args &&...rest)
    {
        std::cout << std::forward<T>(first);
        if constexpr (sizeof...(rest) > 0)
        {
            std::cout << " ";
            debug_impl(std::forward<Args>(rest)...);
        }
        else
        {
            std::cout << std::endl;
        }
    }

    // Public debug function
    template <typename... Args>
    void debug(Args &&...args)
    {
        const std::string YELLOW = "\033[33m";
        const std::string RESET = "\033[0m";

        if (debug_enabled)
        {
            std::cout << YELLOW << "[DEBUG] ";
            debug_impl(std::forward<Args>(args)...);
            std::cout << RESET;
        }
    }

    
}
