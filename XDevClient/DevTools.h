#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include "ModuleManager.h"
#include "Scanner.h"

class DevToolsModule : public Module
{
public:
    DevToolsModule() : Module("DevTools", Category::Visual, true)
    {
        strcpy_s(m_result, "-");
    }

    bool HasToggle() const override { return false; }   // 常にメニューに出す

    void OnMenu() override
    {
        ImGui::Indent();
        if (ImGui::Button("Scan")) Scan();
        ImGui::SameLine();
        if (ImGui::Button("Dump module")) Dump();
        ImGui::TextWrapped("%s", m_result);
        ImGui::TextDisabled("Pattern: [DevTools] pattern= in config.ini");
        ImGui::Unindent();
    }

private:
    // config.ini の [DevTools] pattern= に書いたパターンを検索する
    void Scan()
    {
        char pattern[256];
        Config::GetString("DevTools", "pattern", "", pattern, sizeof(pattern));
        if (pattern[0] == '\0') { strcpy_s(m_result, "pattern is empty"); return; }

        Scanner::ModuleInfo mod;
        if (!Scanner::GetMainModule(mod)) { strcpy_s(m_result, "module not found"); return; }

        int count = 0;
        uintptr_t addr = Scanner::Find(pattern, mod, &count);
        if (count == 0)
            strcpy_s(m_result, "matches=0");
        else
            sprintf_s(m_result, "matches=%d  first RVA=0x%llX", count, (unsigned long long)(addr - mod.base));
    }

    // ゲーム本体のメモリ上のイメージを、PEファイルとして保存する(解析用)
    void Dump()
    {
        Scanner::ModuleInfo mod;
        if (!Scanner::GetMainModule(mod)) { strcpy_s(m_result, "module not found"); return; }

        std::string path = Config::Path();
        size_t pos = path.find_last_of('\\');
        if (path.empty() || pos == std::string::npos) { strcpy_s(m_result, "config path not ready"); return; }
        path = path.substr(0, pos + 1) + "module_dump.exe";

        HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) { strcpy_s(m_result, "cannot create file"); return; }

        static const size_t CHUNK = 1 << 20;
        std::vector<uint8_t> zeros(CHUNK, 0);

        // 1) メモリの中身を、そのままの配置で書き出す
        uintptr_t addr = mod.base;
        uintptr_t end = mod.base + mod.size;
        while (addr < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

            uintptr_t rb = (uintptr_t)mbi.BaseAddress;
            uintptr_t re = rb + mbi.RegionSize;
            if (re > end) re = end;
            size_t len = (size_t)(re - rb);

            bool readable = mbi.State == MEM_COMMIT &&
                !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
            size_t done = 0;
            DWORD written = 0;
            while (done < len)
            {
                size_t n = len - done;
                if (n > CHUNK) n = CHUNK;
                const void* src = readable ? (const void*)(rb + done) : (const void*)zeros.data();
                WriteFile(f, src, (DWORD)n, &written, nullptr);
                done += n;
            }
            addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }

        // 2) PEヘッダーを、「ファイルの位置 = メモリの位置」になるよう書き換える
        auto dos = (PIMAGE_DOS_HEADER)mod.base;
        auto nt = (PIMAGE_NT_HEADERS)(mod.base + dos->e_lfanew);
        DWORD sa = nt->OptionalHeader.SectionAlignment;

        // FileAlignment を SectionAlignment に合わせる
        {
            LARGE_INTEGER li;
            li.QuadPart = (LONGLONG)((uintptr_t)&nt->OptionalHeader.FileAlignment - mod.base);
            SetFilePointerEx(f, li, nullptr, FILE_BEGIN);
            DWORD w = 0;
            WriteFile(f, &sa, sizeof(sa), &w, nullptr);
        }

        // 各セクションの「ファイル上の位置と大きさ」を、メモリ上のものに合わせる
        WORD n = nt->FileHeader.NumberOfSections;
        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < n; i++)
        {
            IMAGE_SECTION_HEADER s = sec[i];
            DWORD vs = s.Misc.VirtualSize;
            s.PointerToRawData = s.VirtualAddress;
            s.SizeOfRawData = (vs + sa - 1) / sa * sa;

            LARGE_INTEGER li;
            li.QuadPart = (LONGLONG)((uintptr_t)&sec[i] - mod.base);
            SetFilePointerEx(f, li, nullptr, FILE_BEGIN);
            DWORD w = 0;
            WriteFile(f, &s, sizeof(s), &w, nullptr);
        }

        CloseHandle(f);
        sprintf_s(m_result, "dumped %llu MB: %s", (unsigned long long)(mod.size >> 20), path.c_str());
    }

    char m_result[400];
};