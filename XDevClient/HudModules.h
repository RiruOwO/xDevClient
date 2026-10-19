#pragma once
#include <deque>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "ModuleManager.h"

class WatermarkModule : public Module
{
public:
    WatermarkModule() : Module("Watermark", Category::Hud) {}

    void OnRender(ImDrawList* dl, float& y) override
    {
        dl->AddText(ImVec2(10, y), IM_COL32(255, 255, 255, 255), "xdev client");
        y += 20.0f;
    }
};

class FpsModule : public Module
{
public:
    FpsModule() : Module("FPS", Category::Hud) {}

    void OnRender(ImDrawList* dl, float& y) override
    {
        char buf[32];
        sprintf_s(buf, "FPS: %.0f", ImGui::GetIO().Framerate);
        dl->AddText(ImVec2(10, y), IM_COL32(255, 255, 255, 255), buf);
        y += 18.0f;
    }
};

class CpsModule : public Module
{
public:
    CpsModule() : Module("CPS", Category::Hud) {}

    void OnUpdate() override
    {
        double now = ImGui::GetTime();
        bool focused = ModuleManager::Get().IsGameFocused();
        bool l = focused && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool r = focused && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (l && !m_lPrev) m_left.push_back(now);
        if (r && !m_rPrev) m_right.push_back(now);
        m_lPrev = l;
        m_rPrev = r;
        while (!m_left.empty() && now - m_left.front() > 1.0) m_left.pop_front();
        while (!m_right.empty() && now - m_right.front() > 1.0) m_right.pop_front();
    }

    void OnRender(ImDrawList* dl, float& y) override
    {
        char buf[32];
        sprintf_s(buf, "CPS: %d | %d", (int)m_left.size(), (int)m_right.size());
        dl->AddText(ImVec2(10, y), IM_COL32(255, 255, 255, 255), buf);
        y += 18.0f;
    }

private:
    std::deque<double> m_left, m_right;
    bool m_lPrev = false, m_rPrev = false;
};

class KeystrokesModule : public Module
{
public:
    KeystrokesModule() : Module("Keystrokes", Category::Hud) {}

    void OnRender(ImDrawList* dl, float& y) override
    {
        bool focused = ModuleManager::Get().IsGameFocused();
        auto key = [&](const char* label, int vk, float x, float yy)
            {
                bool down = focused && (GetAsyncKeyState(vk) & 0x8000) != 0;
                dl->AddRectFilled(ImVec2(x, yy), ImVec2(x + 30, yy + 30),
                    down ? IM_COL32(255, 255, 255, 170) : IM_COL32(0, 0, 0, 120), 4.0f);
                dl->AddText(ImVec2(x + 10, yy + 7),
                    down ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255), label);
            };
        float x0 = 10.0f;
        key("W", 'W', x0 + 34, y + 4);
        key("A", 'A', x0, y + 38);
        key("S", 'S', x0 + 34, y + 38);
        key("D", 'D', x0 + 68, y + 38);
        y += 76.0f;
    }
};

class ArrayListModule : public Module
{
public:
    ArrayListModule() : Module("ArrayList", Category::Hud) {}

    void OnRender(ImDrawList* dl, float& y) override
    {
        // オンの機能を集める(HUDカテゴリは除外)
        std::vector<const char*> names;
        for (const auto& m : ModuleManager::Get().GetModules())
        {
            if (m->IsEnabled() && m->HasToggle() && m->GetCategory() != Category::Hud)
                names.push_back(m->GetName());
        }

        // 名前の長い順に並べる
        std::sort(names.begin(), names.end(), [](const char* a, const char* b)
            {
                return ImGui::CalcTextSize(a).x > ImGui::CalcTextSize(b).x;
            });

        float screenW = ImGui::GetIO().DisplaySize.x;
        float yy = 10.0f;
        for (const char* n : names)
        {
            ImVec2 sz = ImGui::CalcTextSize(n);
            float x = screenW - sz.x - 12.0f;

            // 背景と、右端のアクセントの線
            dl->AddRectFilled(ImVec2(x - 4, yy - 1), ImVec2(screenW - 6, yy + sz.y + 1),
                IM_COL32(0, 0, 0, 110));
            dl->AddRectFilled(ImVec2(screenW - 6, yy - 1), ImVec2(screenW - 4, yy + sz.y + 1),
                IM_COL32(120, 200, 255, 255));

            dl->AddText(ImVec2(x, yy), IM_COL32(255, 255, 255, 255), n);
            yy += sz.y + 3.0f;
        }
    }
};