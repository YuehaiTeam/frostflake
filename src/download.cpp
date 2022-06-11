#include <array>
#include <boost/algorithm/string.hpp>
#include <string>
#include <thread>
#include <vector>

#include "../version.h"
#include "struct.h"

#include <windows.h>

#include <shlobj.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <wininet.h>

#include "../lib/json11.hpp"
#include <curl/curl.h>

typedef struct IupdateInfo {
    std::string url;
    std::string name;
    std::string md5;
    std::string status;
    long downloaded;
    long total;
} IupdateInfo;

typedef struct IWriteAndHash {
    IupdateInfo *info;
    HCRYPTHASH hash;
    HANDLE file;
    boolean status;
} IWriteAndHash;

void curlOpt(CURL *ch) {
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(ch, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, std::string("cocogoat-control/" + std::string(VERSION)).c_str());
}
size_t curlWriteToString(void *ptr, size_t size, size_t nmemb, void *data) {
    ((std::string *)data)->append((char *)ptr, size * nmemb);
    return size * nmemb;
}
bool curlGetInfo(std::wstring infourl, IupdateInfo *info) {
    info->status = "started";
    if (fileExists(getRelativePath("cocogoat-noupdate"))) {
        info->status = "noupdate";
        return false;
    }
    info->status = "prechecking";
    CURL *ch = curl_easy_init();
    if (!ch) {
        info->status = "check: curl_easy_init failed";
        return false;
    }
    curl_easy_setopt(ch, CURLOPT_URL, ToString(infourl).c_str());
    curlOpt(ch);
    std::string data;
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, &data);
    CURLcode res = curl_easy_perform(ch);
    curl_easy_cleanup(ch);
    if (res != CURLE_OK) {
        info->status = "check: " + std::string(curl_easy_strerror(res));
        return false;
    }
    std::string jsonerr = "";
    json11::Json json = json11::Json::parse(data, jsonerr);
    if (jsonerr != "") {
        info->status = "check: " + jsonerr;
        return false;
    }
    info->url = json["url"].string_value();
    info->md5 = json["md5"].string_value();
    info->name = json["name"].string_value();
    info->total = (long)json["size"].number_value();
    return true;
}
std::string toChecksum(HCRYPTHASH pHash, IupdateInfo *info) {
    DWORD pHashsize = 16;
    BYTE pHashText[16];
    if (!CryptGetHashParam(pHash, HP_HASHVAL, pHashText, &pHashsize, 0)) {
        info->status = "CryptGetHashParam failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }
    std::string hashText = "";
    CHAR digits[] = "0123456789abcdef";
    for (DWORD i = 0; i < pHashsize; i++) {
        char s[16];
        sprintf(s, "%c%c", digits[pHashText[i] >> 4], digits[pHashText[i] & 0xf]);
        hashText += s;
    }
    // to lower
    std::transform(hashText.begin(), hashText.end(), hashText.begin(), ::tolower);
    return hashText;
}
size_t curlWriteAndHash(void *ptr, size_t size, size_t nmemb, void *data) {
    IWriteAndHash *writeAndHash = (IWriteAndHash *)data;
    if (!writeAndHash->status)
        return 0;
    DWORD written;
    WriteFile(writeAndHash->file, ptr, size * nmemb, &written, NULL);
    writeAndHash->info->downloaded += written;
    if (!CryptHashData(writeAndHash->hash, (BYTE *)ptr, size * nmemb, 0)) {
        writeAndHash->info->status = "CryptHashData failed: [" + std::to_string(GetLastError()) + "]";
        writeAndHash->status = false;
        return 0;
    }
    return size * nmemb;
}
bool checkAndDownloadUsingCurl(std::wstring infourl, IupdateInfo *info) {
    uint8_t buffer[20480];
    if (!curlGetInfo(infourl, info)) {
        return false;
    }
    // get target path
    std::wstring path = getLocalPath(info->name);
    if (path == L"") {
        info->status = "getLocalPath failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        info->status = "CryptAcquireContext failed: [" + std::to_string(GetLastError()) + "]";
    }
    // check target file exists
    HANDLE rFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (rFile != INVALID_HANDLE_VALUE) {
        info->status = "prehashing";
        // target file exists, check md5
        HCRYPTHASH pHash = 0;
        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &pHash)) {
            info->status = "Precheck: CryptCreateHash failed: [" + std::to_string(GetLastError()) + "]";
            CryptReleaseContext(hProv, 0);
            return false;
        }
        DWORD read = 0;
        while (ReadFile(rFile, buffer, sizeof(buffer), &read, NULL)) {
            if (!CryptHashData(pHash, buffer, read, 0)) {
                info->status = "Precheck: CryptHashData failed: [" + std::to_string(GetLastError()) + "]";
                CryptReleaseContext(hProv, 0);
                CloseHandle(rFile);
                return false;
            }
            info->downloaded += read;
            // break on end
            if (read == 0)
                break;
        }
        CloseHandle(rFile);
        info->status = "prechecksum";
        std::string hashText = toChecksum(pHash, info);
        if (hashText == "") {
            return false;
        }
        if (info->md5 == hashText) {
            info->status = "noupdate";
            CryptReleaseContext(hProv, 0);
            return false;
        }
    }
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        info->status = "CryptCreateHash failed: [" + std::to_string(GetLastError()) + "]";
        CryptReleaseContext(hProv, 0);
        return false;
    }
    info->downloaded = 0;
    info->status = "downloading";
    info->downloaded = 0;
    CURL *ch = curl_easy_init();
    if (!ch) {
        info->status = "dl: curl_easy_init failed";
        CryptReleaseContext(hProv, 0);
        return false;
    }
    HANDLE hfileout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfileout == INVALID_HANDLE_VALUE) {
        info->status = "CreateFileW failed: [" + std::to_string(GetLastError()) + "]";
        CryptReleaseContext(hProv, 0);
        return false;
    }
    curl_easy_setopt(ch, CURLOPT_URL, info->url.c_str());
    IWriteAndHash writeAndHash = {
        info,
        hHash,
        hfileout,
        true};
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, &writeAndHash);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curlWriteAndHash);
    curlOpt(ch);
    CURLcode res = curl_easy_perform(ch);
    CloseHandle(hfileout);
    if (!writeAndHash.status) {
        CryptReleaseContext(hProv, 0);
        return false;
    }
    if (res != CURLE_OK) {
        info->status = "dl: " + std::string(curl_easy_strerror(res));
        CryptReleaseContext(hProv, 0);
        return false;
    }
    info->status = "checksum";
    std::string hashText = toChecksum(hHash, info);
    if (hashText == "") {
        return false;
    }
    if (info->md5 != hashText) {
        info->status = "checksum failed: " + hashText + " != " + info->md5;
        return false;
    }
    CryptReleaseContext(hProv, 0);
    info->status = "done";
    return true;
}

void checkAndDownloadInNewThread(std::wstring infourl, IupdateInfo *info) {
    std::thread t(checkAndDownloadUsingCurl, infourl, info);
    t.detach();
}