#include <boost/algorithm/string.hpp>
#include <iostream>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <vector>
std::wstring ToWString(const std::string &str);
std::wstring getLocalPath(std::string name);
void ws_broadcast(std::string action, std::string msg);
std::string yasJsonContent = "";
BOOL APIENTRY MyCreatePipeEx(
    OUT LPHANDLE lpReadPipe,
    OUT LPHANDLE lpWritePipe,
    IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
    IN DWORD nSize,
    DWORD dwReadMode,
    DWORD dwWriteMode);
void runYas(std::string &argv) {
    std::wstring path = getLocalPath("yas.exe");
    // check exists
    HANDLE rFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (rFile == INVALID_HANDLE_VALUE) {
        ws_broadcast("yas", "yas not found");
        return;
    }
    CloseHandle(rFile);
    // delete old yasjson
    std::wstring yasJson = getLocalPath("mona.json");
    DeleteFileW(yasJson.c_str());
    // pipes
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    // create named pipe for stdout with FILE_FLAG_OVERLAPPED
    HANDLE outRead, outWrite;
    if (!MyCreatePipeEx(&outRead, &outWrite, &sa, 0, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED)) {
        ws_broadcast("yas", "create pipe failed");
        return;
    }
    // create pipe for stdin
    HANDLE inRead, inWrite;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0)) {
        ws_broadcast("yas", "create pipe failed");
        return;
    }
    // create child process
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;
    si.hStdInput = inRead;
    // set cwd
    std::wstring cwd = getLocalPath("");
    // prepend yas.exe to argv
    argv = "yas.exe " + argv;
    ws_broadcast("yas-output", argv);
    const wchar_t *szargv = ToWString(argv).c_str();
    // make it not const
    wchar_t *szargv2 = new wchar_t[argv.size() + 1];
    wcscpy(szargv2, szargv);
    // CREATE_NO_WINDOW
    if (!CreateProcessW(path.c_str(), szargv2, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd.c_str(), &si, &pi)) {
        ws_broadcast("yas", "create process failed");
        delete[] szargv2;
        return;
    }
    ws_broadcast("yas", "loaded");
    // send enter to stdin
    DWORD dwWritten;
    WriteFile(inWrite, "\n", 1, &dwWritten, NULL);
    // loop for stdout until exit
    DWORD dwRead = 0;
    char buf[1024];
    std::string output;
    while (true) {
        std::cout << "readfile[1]" << std::endl;
        // WaitForMultipleObjects readfile
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!ReadFile(outRead, buf, sizeof(buf), &dwRead, &ov)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                std::cout << "readfile[3]" << std::endl;
                ws_broadcast("yas", "read pipe failed");
                break;
            }
        }
        std::cout << "readfile[2]" << std::endl;
        // wait both readfile and subprocess
        HANDLE h[2] = {ov.hEvent, pi.hProcess};
        std::cout << "wait[1]" << std::endl;
        DWORD dw = WaitForMultipleObjects(2, h, FALSE, INFINITE);
        std::cout << "wait[2]:" << dw << std::endl;
        // check which event is triggered
        if (dw == WAIT_OBJECT_0) {
            // readfile is triggered
            std::cout << "readfile result" << std::endl;
            if (!GetOverlappedResult(outRead, &ov, &dwRead, FALSE)) {
                ws_broadcast("yas", "read pipe failed");
                break;
            } else {
                output.append(buf, dwRead);
            }
        } else if (dw == WAIT_OBJECT_0 + 1) {
            // cancelevent is triggered
            std::cout << "canceled" << std::endl;
            // send rest of output
            if (output != "") {
                ws_broadcast("yas-output", output);
            }
            break;
        }
        std::vector<std::string> lines;
        boost::split(lines, output, boost::is_any_of("\n"));
        int i = -1;
        for (auto &line : lines) {
            i++;
            // if is the last line
            if (i == lines.size() - 1) {
                output = line;
                continue;
            }
            ws_broadcast("yas-output", line);
        }
    }
    CloseHandle(inRead);
    CloseHandle(inWrite);
    CloseHandle(outRead);
    CloseHandle(outWrite);
    delete[] szargv2;
    // read yas json content using readfilew
    HANDLE jFile = CreateFileW(yasJson.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (jFile == INVALID_HANDLE_VALUE) {
        ws_broadcast("yas", "yas json not found");
        return;
    }
    DWORD dwRead2 = 0;
    char buf2[1024];
    yasJsonContent = "";
    while (true) {
        if (!ReadFile(jFile, buf2, sizeof(buf2), &dwRead2, NULL)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                ws_broadcast("yas", "read json failed");
                break;
            }
        } else {
            yasJsonContent.append(buf2, dwRead2);
        }
        if (dwRead2 == 0) {
            break;
        }
    }
    CloseHandle(jFile);
    // delete yas json
    DeleteFileW(yasJson.c_str());
    ws_broadcast("yas", "exit");
    return;
}
std::string getYasJsonContent() {
    return yasJsonContent;
}
void runYasInNewThread(std::string &argv) {
    std::thread t(runYas, argv);
    t.detach();
}