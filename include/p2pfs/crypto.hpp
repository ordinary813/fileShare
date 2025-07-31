#pragma once

#include <openssl/sha.h>
#include <string>

namespace p2pfs{
    std::string calculate_sha256(const std::string &filePath);
}