#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace Scanner
{
    struct ModuleInfo
    {
        uintptr_t base;
        size_t size;
    };

    // ゲーム本体(exe)のメモリ上の位置と大きさ
    inline bool GetMainModule(ModuleInfo& out)
    {
        HMODULE h = GetModuleHandleW(nullptr);
        if (!h) return false;
        auto dos = (PIMAGE_DOS_HEADER)h;
        auto nt = (PIMAGE_NT_HEADERS)((uintptr_t)h + dos->e_lfanew);
        out.base = (uintptr_t)h;
        out.size = (size_t)nt->OptionalHeader.SizeOfImage;
        return true;
    }

    // "48 8B ?? 05" のような文字列を、バイトの並びに変える(?? は何でもよい)
    inline std::vector<int> Parse(const char* pattern)
    {
        std::vector<int> bytes;
        const char* p = pattern;
        while (*p)
        {
            if (*p == ' ') { p++; continue; }
            if (*p == '?')
            {
                bytes.push_back(-1);
                while (*p == '?') p++;
                continue;
            }
            bytes.push_back((int)strtol(p, nullptr, 16));
            p += 2;
        }
        return bytes;
    }

    // パターンを探す。最初に見つかった場所を返し、見つかった個数を outCount に入れる
    inline uintptr_t Find(const char* pattern, const ModuleInfo& m, int* outCount = nullptr)
    {
        std::vector<int> pat = Parse(pattern);
        uintptr_t first = 0;
        int count = 0;

        if (!pat.empty())
        {
            uintptr_t addr = m.base;
            uintptr_t end = m.base + m.size;
            while (addr < end)
            {
                MEMORY_BASIC_INFORMATION mbi{};
                if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

                uintptr_t rb = (uintptr_t)mbi.BaseAddress;
                uintptr_t re = rb + mbi.RegionSize;
                if (re > end) re = end;

                bool readable = mbi.State == MEM_COMMIT &&
                    !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
                if (readable && re - rb >= pat.size())
                {
                    const uint8_t* data = (const uint8_t*)rb;
                    size_t len = (size_t)(re - rb) - pat.size() + 1;
                    for (size_t i = 0; i < len; i++)
                    {
                        bool ok = true;
                        for (size_t j = 0; j < pat.size(); j++)
                        {
                            if (pat[j] >= 0 && data[i + j] != (uint8_t)pat[j]) { ok = false; break; }
                        }
                        if (ok)
                        {
                            if (!first) first = rb + i;
                            count++;
                        }
                    }
                }
                addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            }
        }

        if (outCount) *outCount = count;
        return first;
    }
}