#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

DWORD FindPid(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe))
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::cout << "usage: XDevInjector <dll full path>\n"; return 1; }
    std::string dll = argv[1];

    DWORD pid = FindPid(L"Minecraft.Windows.exe");
    if (!pid) { std::cout << "Minecraft not found\n"; return 1; }

    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) { std::cout << "OpenProcess failed: " << GetLastError() << "\n"; return 1; }

    void* mem = VirtualAllocEx(h, NULL, dll.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(h, mem, dll.c_str(), dll.size() + 1, NULL);
    HANDLE t = CreateRemoteThread(h, NULL, 0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
        mem, 0, NULL);
    if (!t) { std::cout << "CreateRemoteThread failed: " << GetLastError() << "\n"; return 1; }
    std::cout << "injected\n";
    CloseHandle(h);
    return 0;
}