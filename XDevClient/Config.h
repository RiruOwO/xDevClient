#pragma once
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace Config
{
    inline std::string& Path()
    {
        static std::string p;
        return p;
    }

    // %APPDATA%\XDevClient\config.ini Çï€ë∂êÊÇ…Ç∑ÇÈ
    inline void Init()
    {
        char dir[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, dir)))
        {
            std::string d = std::string(dir) + "\\XDevClient";
            CreateDirectoryA(d.c_str(), nullptr);
            Path() = d + "\\config.ini";
        }
    }

    inline int GetInt(const char* section, const char* key, int def)
    {
        if (Path().empty()) return def;
        return (int)GetPrivateProfileIntA(section, key, def, Path().c_str());
    }

    inline void SetInt(const char* section, const char* key, int value)
    {
        if (Path().empty()) return;
        char buf[32];
        sprintf_s(buf, "%d", value);
        WritePrivateProfileStringA(section, key, buf, Path().c_str());
    }
}