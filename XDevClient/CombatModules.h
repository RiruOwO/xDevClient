#pragma once
#include <cstdio>
#include <atomic>
#include <windows.h>
#include "ModuleManager.h"

class AutoClickerModule : public Module
{
public:
    AutoClickerModule() : Module("AutoClicker", Category::Combat, false) {}

    void OnUpdate() override
    {
        ModuleManager& mm = ModuleManager::Get();

        // オンのときだけマウス監視を動かす
        if (IsEnabled() && !m_hooked) StartHook();
        if (!IsEnabled() && m_hooked) { Restore(); StopHook(); }
        if (!m_hooked) return;

        bool held = PhysLeft().load();   // 本物の左ボタンが押されているか
        if (!held)
        {
            m_synUp = false;
            m_prevHeld = false;
            return;
        }

        double now = ImGui::GetTime();
        double interval = 1.0 / (double)m_cps;

        if (!m_prevHeld) m_next = now + interval;  // 押した瞬間の本物のクリックを1回目とする
        m_prevHeld = true;

        if (!m_synUp)
        {
            if (now >= m_next)
            {
                Send(MOUSEEVENTF_LEFTUP);          // 一瞬離す
                m_synUp = true;
                double hold = interval * 0.4;
                if (hold > 0.03) hold = 0.03;
                m_downAt = now + hold;
                m_next = now + interval;
            }
        }
        else if (now >= m_downAt)
        {
            Send(MOUSEEVENTF_LEFTDOWN);            // また押す
            m_synUp = false;
        }
    }

    void OnMenu() override
    {
        ImGui::Indent();
        ImGui::SliderInt("CPS", &m_cps, 1, 20);
        ImGui::TextDisabled("Hold left mouse button to auto-click");
        ImGui::Unindent();
    }

    void OnSave() override
    {
        Config::SetInt(m_name, "cps", m_cps);
    }

    void OnLoad() override
    {
        int v = Config::GetInt(m_name, "cps", m_cps);
        if (v < 1) v = 1;
        if (v > 20) v = 20;
        m_cps = v;
    }

    void OnUnload() override
    {
        Restore();
        if (m_hooked) StopHook();
    }

private:
    // ----- 本物の左ボタンの状態(監視スレッドが書き込む) -----
    static std::atomic<bool>& PhysLeft()
    {
        static std::atomic<bool> v(false);
        return v;
    }
    static std::atomic<int>& InjCount() { static std::atomic<int> v(0); return v; }
    static std::atomic<int>& PhysCount() { static std::atomic<int> v(0); return v; }
    static int& SentCount() { static int v = 0; return v; }
    static UINT& LastRet() { static UINT v = 0; return v; }
    static HANDLE& HookThreadHandle() { static HANDLE h = nullptr; return h; }
    static DWORD& HookThreadId() { static DWORD id = 0; return id; }

    static LRESULT CALLBACK MouseProc(int code, WPARAM w, LPARAM l)
    {
        if (code == HC_ACTION && (w == WM_LBUTTONDOWN || w == WM_LBUTTONUP))
        {
            const MSLLHOOKSTRUCT* ms = (const MSLLHOOKSTRUCT*)l;
            if (ms->flags & LLMHF_INJECTED)
            {
                InjCount()++;                 // このツールが送った疑似クリック
            }
            else
            {
                PhysCount()++;                // 本物のクリック
                PhysLeft() = (w == WM_LBUTTONDOWN);
            }
        }
        return CallNextHookEx(nullptr, code, w, l);
    }

    static DWORD WINAPI HookThread(LPVOID)
    {
        HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (hook) UnhookWindowsHookEx(hook);
        return 0;
    }

    void StartHook()
    {
        PhysLeft() = false;
        HookThreadHandle() = CreateThread(nullptr, 0, HookThread, nullptr, 0, &HookThreadId());
        m_hooked = (HookThreadHandle() != nullptr);
    }

    void StopHook()
    {
        HANDLE h = HookThreadHandle();
        if (h)
        {
            for (int i = 0; i < 50; i++)
            {
                if (PostThreadMessageW(HookThreadId(), WM_QUIT, 0, 0)) break;
                Sleep(10);
            }
            WaitForSingleObject(h, 1000);
            CloseHandle(h);
            HookThreadHandle() = nullptr;
        }
        m_hooked = false;
        PhysLeft() = false;
    }

    // ----- クリック送信など -----
    static void Send(DWORD flags)
    {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = flags;
        LastRet() = SendInput(1, &in, sizeof(INPUT));
        SentCount()++;
    }

    static bool CursorShowing()
    {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        return GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) != 0;
    }

    // 疑似的に離している途中なら、本物がまだ押されている場合に押し直して元に戻す
    void Restore()
    {
        if (!m_synUp) return;
        if (PhysLeft().load()) Send(MOUSEEVENTF_LEFTDOWN);
        m_synUp = false;
    }

    int m_cps = 8;
    bool m_hooked = false;
    bool m_synUp = false;
    bool m_prevHeld = false;
    double m_next = 0.0;
    double m_downAt = 0.0;
};