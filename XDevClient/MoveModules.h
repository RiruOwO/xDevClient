#pragma once
#include "ModuleManager.h"

class SprintModule : public Module
{
public:
    SprintModule() : Module("Sprint", Category::Move, false) {}

    void OnUpdate() override
    {
        ModuleManager& mm = ModuleManager::Get();

        bool want = IsEnabled()
            && mm.IsGameFocused()
            && !mm.menuOpen
            && !mm.IsCursorShowing()
            && (GetAsyncKeyState('W') & 0x8000) != 0
            && (GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0;

        if (want && !m_down)
        {
            SendKey(VK_LCONTROL, true);
            m_down = true;
        }
        else if (!want && m_down)
        {
            SendKey(VK_LCONTROL, false);
            m_down = false;
        }
    }

    void OnUnload() override
    {
        if (m_down)
        {
            SendKey(VK_LCONTROL, false);
            m_down = false;
        }
    }

private:
    static void SendKey(WORD vk, bool down)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    }

    bool m_down = false;
};