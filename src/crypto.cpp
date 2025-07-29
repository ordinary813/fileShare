#include <openssl/sha.h>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>

namespace p2pfs
{
    std::string calculate_sha256(const std::string &filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
            return "";

        SHA256_CTX ctx;
        SHA256_Init(&ctx);

        std::vector<char> buffer(8192);
        while (file.read(buffer.data(), buffer.size()) || file.gcount())
        {
            SHA256_Update(&ctx, buffer.data(), file.gcount());
        }

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &ctx);

        std::ostringstream result;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

        return result.str();
    }
}