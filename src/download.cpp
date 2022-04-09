#include <array>
#include <boost/algorithm/string.hpp>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

#include <wincrypt.h>
#include <wininet.h>

bool download(std::wstring infourl, std::wstring path, unsigned long &downloaded, unsigned long &total, std::string &status) {
    status = "started";
    HINTERNET hopen = InternetOpenW(L"cocogoat-updater", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    DWORD flags = INTERNET_FLAG_DONT_CACHE;
    if (!hopen) {
        status = "InternetOpenW failed";
        return false;
    }
    if (infourl.find(L"https://") == 0)
        flags |= INTERNET_FLAG_SECURE;

    // get content of checkerurl
    HINTERNET hrequest = InternetOpenUrlW(hopen, infourl.c_str(), NULL, 0, flags, 0);
    if (!hrequest) {
        status = "Precheck: InternetOpenUrlW failed";
        return false;
    }
    status = "prechecking";
    uint8_t buffer[10240];
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
    total = std::stoul(parts[1]);
    std::string surl = parts[2];
    std::wstring url = std::wstring(surl.begin(), surl.end());
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv,
                             NULL,
                             NULL,
                             PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
        status = "CryptAcquireContext failed";
    }
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        status = "CryptCreateHash failed";
        CryptReleaseContext(hProv, 0);
        return false;
    }
    status = "downloading";
    HINTERNET hfile = InternetOpenUrlW(hopen, url.c_str(), NULL, 0, flags, 0);
    if (!hfile) {
        InternetCloseHandle(hopen);
        status = "Download: InternetOpenUrlW failed";
        return false;
    }
    downloaded = 0;
    HANDLE hfileout = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfileout == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hfile);
        InternetCloseHandle(hopen);
        status = "CreateFileW failed";
        return false;
    }
    DWORD read = 0;
    while (InternetReadFile(hfile, buffer, sizeof(buffer), &read) && read > 0) {
        downloaded += read;
        DWORD written;
        WriteFile(hfileout, buffer, read, &written, NULL);
        if (!CryptHashData(hHash, buffer, read, 0)) {
            status = "CryptHashData failed";
            CloseHandle(hfileout);
            InternetCloseHandle(hfile);
            InternetCloseHandle(hopen);
            return false;
        }
    }
    CloseHandle(hfileout);
    InternetCloseHandle(hfile);
    InternetCloseHandle(hopen);
    status = "checksum";
    // CryptGetHashParam
    DWORD hashsize = 16;
    BYTE hash[16];
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashsize, 0)) {
        status = "CryptGetHashParam failed";
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
        status = "checksum failed: " + hashText + " != " + md5;
        return false;
    }
    status = "done";
    return true;
}
void downloadInNewThread(std::wstring infourl, std::wstring path, unsigned long &downloaded, unsigned long &total, std::string &status) {
    std::thread t(download, infourl, path, std::ref(downloaded), std::ref(total), std::ref(status));
    t.detach();
}