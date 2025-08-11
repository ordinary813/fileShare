// crypto.cpp
#include "p2pfs/crypto.hpp"
#include <openssl/evp.h>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace p2pfs
{

    std::string calculate_sha256(const std::string &filename)
    {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int length = 0;

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
            throw std::runtime_error("Failed to create EVP_MD_CTX");

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
            throw std::runtime_error("Digest init failed");

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Could not open file: " + filename);

        std::vector<char> buffer(8192);
        while (file.good())
        {
            file.read(buffer.data(), buffer.size());
            if (EVP_DigestUpdate(ctx, buffer.data(), file.gcount()) != 1)
                throw std::runtime_error("Digest update failed");
        }

        if (EVP_DigestFinal_ex(ctx, hash, &length) != 1)
            throw std::runtime_error("Digest final failed");

        EVP_MD_CTX_free(ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < length; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

        return oss.str();
    }

} // namespace p2pfs
