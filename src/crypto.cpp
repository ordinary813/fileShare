#include <openssl/evp.h>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace p2pfs
{
    std::string calculate_sha256(const std::string &filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
            return "";

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
            return "";

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
        {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        std::vector<char> buffer(8192);
        while (file.read(buffer.data(), buffer.size()) || file.gcount())
        {
            if (EVP_DigestUpdate(ctx, buffer.data(), file.gcount()) != 1)
            {
                EVP_MD_CTX_free(ctx);
                return "";
            }
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1)
        {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        EVP_MD_CTX_free(ctx);

        std::ostringstream result;
        for (unsigned int i = 0; i < hash_len; ++i)
            result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);

        return result.str();
    }
}