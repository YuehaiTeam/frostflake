#include <array>
#include <boost/algorithm/string.hpp>
#include <string>
#include <thread>
#include <vector>

#include "struct.h"

#include <windows.h>

#include <shlobj.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <wininet.h>

typedef struct IupdateInfo {
    std::string status;
    long downloaded;
    long total;
} IupdateInfo;

bool checkAndDownload(std::wstring infourl, IupdateInfo *info) {
    if (fileExists(getRelativePath("cocogoat-noupdate"))) {
        info->status = "noupdate";
        return false;
    }
    info->status = "started";
    HINTERNET hopen = InternetOpenW(L"cocogoat-updater", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    DWORD flags = INTERNET_FLAG_DONT_CACHE;
    if (!hopen) {
        info->status = "InternetOpenW failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }
    if (infourl.find(L"https://") == 0)
        flags |= INTERNET_FLAG_SECURE;

    // get content of checkerurl
    HINTERNET hrequest = InternetOpenUrlW(hopen, infourl.c_str(), NULL, 0, flags, 0);
    if (!hrequest) {
        info->status = "Precheck: InternetOpenUrlW failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }
    info->status = "prechecking";
    uint8_t buffer[20480];
    DWORD bytesRead;
    std::string content;
    while (InternetReadFile(hrequest, buffer, sizeof(buffer), &bytesRead)) {
        if (bytesRead == 0)
            break;
        content += std::string((char *)buffer, bytesRead);
    }
    InternetCloseHandle(hrequest);
    // split by ','
    std::vector<std::string> parts;
    boost::split(parts, content, boost::is_any_of(","));
    std::string md5 = parts[0];
    long total = std::stol(parts[1]);
    std::string surl = parts[2];
    std::string fname = parts[3];
    std::wstring url = std::wstring(surl.begin(), surl.end());
    // get target path
    std::wstring path = getLocalPath(fname);
    if (path == L"") {
        info->status = "getLocalPath failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv,
                             NULL,
                             NULL,
                             PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
        info->status = "CryptAcquireContext failed: [" + std::to_string(GetLastError()) + "]";
    }

    // check target file exists
    HANDLE rFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (rFile != INVALID_HANDLE_VALUE) {
        info->status = "prehashing";
        // get file size
        DWORD fileSize = GetFileSize(rFile, NULL);
        info->total = fileSize;
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
        // CryptGetHashParam
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
        if (md5 == hashText) {
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
    info->total = total;
    info->status = "downloading";
    HINTERNET hfile = InternetOpenUrlW(hopen, url.c_str(), NULL, 0, flags, 0);
    if (!hfile) {
        info->status = "Download: InternetOpenUrlW failed: [" + std::to_string(GetLastError()) + "]";
        InternetCloseHandle(hopen);
        return false;
    }
    info->downloaded = 0;
    HANDLE hfileout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfileout == INVALID_HANDLE_VALUE) {
        info->status = "CreateFileW failed: [" + std::to_string(GetLastError()) + "]";
        InternetCloseHandle(hfile);
        InternetCloseHandle(hopen);
        return false;
    }
    DWORD read = 0;
    while (InternetReadFile(hfile, buffer, sizeof(buffer), &read) && read > 0) {
        info->downloaded += read;
        DWORD written;
        WriteFile(hfileout, buffer, read, &written, NULL);
        if (!CryptHashData(hHash, buffer, read, 0)) {
            info->status = "CryptHashData failed: [" + std::to_string(GetLastError()) + "]";
            CloseHandle(hfileout);
            InternetCloseHandle(hfile);
            InternetCloseHandle(hopen);
            return false;
        }
    }
    CloseHandle(hfileout);
    InternetCloseHandle(hfile);
    InternetCloseHandle(hopen);
    info->status = "checksum";
    // CryptGetHashParam
    DWORD hashsize = 16;
    BYTE hash[16];
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashsize, 0)) {
        info->status = "CryptGetHashParam failed: [" + std::to_string(GetLastError()) + "]";
        return false;
    }
    std::string hashText = "";
    CHAR digits[] = "0123456789abcdef";
    for (DWORD i = 0; i < hashsize; i++) {
        char s[16];
        sprintf(s, "%c%c", digits[hash[i] >> 4], digits[hash[i] & 0xf]);
        hashText += s;
    }
    // to lower
    std::transform(hashText.begin(), hashText.end(), hashText.begin(), ::tolower);
    if (md5 != hashText) {
        info->status = "checksum failed: " + hashText + " != " + md5;
        return false;
    }
    info->status = "done";
    return true;
}
void checkAndDownloadInNewThread(std::wstring infourl, IupdateInfo *info) {
    std::thread t(checkAndDownload, infourl, info);
    t.detach();
}