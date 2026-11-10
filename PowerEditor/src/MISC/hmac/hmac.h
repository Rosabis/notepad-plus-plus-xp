#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// HMAC-SHA256 is computed with the SHA-256 implementation bundled in MISC/sha2.
// The original implementation used BCrypt (bcrypt.dll), which only exists from
// Windows Vista on and therefore cannot be linked into the XP edition.
#include "sha-256.h"

// Read MachineGuid from registry
static std::string getMachineGUID()
{
    char guid[64]{};
    DWORD size = sizeof(guid);
    HKEY hKey;
    if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return "";

    ::RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, (LPBYTE)guid, &size);
    ::RegCloseKey(hKey);
    return std::string(guid);
}

// HMAC-SHA256 as specified by RFC 2104
static std::string computeHMAC(const std::string& key, const std::string& data)
{
    constexpr size_t blockSize = 64;	// SHA-256 input block size
    constexpr size_t hashSize = 32;		// SHA-256 output size

    // Keys longer than one block are replaced by their digest
    std::vector<uint8_t> normalizedKey(blockSize, 0);
    if (key.size() > blockSize)
    {
        uint8_t digest[hashSize]{};
        calc_sha_256(digest, key.data(), key.size());
        std::copy(std::begin(digest), std::end(digest), normalizedKey.begin());
    }
    else
    {
        std::transform(key.begin(), key.end(), normalizedKey.begin(),
            [](char c) { return static_cast<uint8_t>(c); });
    }

    std::vector<uint8_t> innerPad(blockSize, 0x36);
    std::vector<uint8_t> outerPad(blockSize, 0x5c);
    for (size_t i = 0; i < blockSize; ++i)
    {
        innerPad[i] ^= normalizedKey[i];
        outerPad[i] ^= normalizedKey[i];
    }

    // H((K ^ opad) || H((K ^ ipad) || data))
    std::vector<uint8_t> inner;
    inner.reserve(blockSize + data.size());
    inner.insert(inner.end(), innerPad.begin(), innerPad.end());
    inner.insert(inner.end(), data.begin(), data.end());

    uint8_t innerDigest[hashSize]{};
    calc_sha_256(innerDigest, inner.data(), inner.size());

    std::vector<uint8_t> outer;
    outer.reserve(blockSize + hashSize);
    outer.insert(outer.end(), outerPad.begin(), outerPad.end());
    outer.insert(outer.end(), std::begin(innerDigest), std::end(innerDigest));

    uint8_t hash[hashSize]{};
    calc_sha_256(hash, outer.data(), outer.size());

    std::string result;
    result.reserve(hashSize * 2);
    constexpr char hexDigits[] = "0123456789abcdef";
    for (uint8_t byte : hash)
    {
        result.push_back(hexDigits[(byte >> 4) & 0x0F]);
        result.push_back(hexDigits[byte & 0x0F]);
    }
    return result;
}
