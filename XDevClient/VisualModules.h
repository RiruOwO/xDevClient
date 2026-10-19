#pragma once
#include "ModuleManager.h"

class SelfTagModule : public Module
{
public:
    SelfTagModule() : Module("SelfTag", Category::Visual, true)
    {
        strcpy_s(m_text, "Player");
    }

    bool HasToggle() const override { return false; }   // 常に有効

    // F5を押した回数で、視点(0:一人称 / 1:三人称背面 / 2:三人称前面)を追う
    void OnUpdate() override
    {
        ModuleManager& mm = ModuleManager::Get();
        bool f5 = mm.IsGameFocused() && !mm.menuOpen && (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5 && !m_f5Prev) m_view = (m_view + 1) % 3;
        m_f5Prev = f5;
    }

    void OnRender(ImDrawList* dl, float& y) override
    {
        if (m_view == 0 || m_text[0] == '\0') return;   // 一人称のときは出さない

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        float scale = (float)m_size / 100.0f;
        float fontSize = ImGui::GetFontSize() * scale;
        ImVec2 base = ImGui::CalcTextSize(m_text);
        ImVec2 sz(base.x * scale, base.y * scale);
        float padX = 10.0f * scale;
        float padY = 4.0f * scale;

        float cx = ds.x * (float)m_x / 100.0f;
        float top = ds.y * (float)m_y / 100.0f;

        ImVec2 p0(cx - sz.x * 0.5f - padX, top);
        ImVec2 p1(cx + sz.x * 0.5f + padX, top + sz.y + padY * 2.0f);

        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 115), 3.0f);
        dl->AddText(ImGui::GetFont(), fontSize, ImVec2(p0.x + padX, p0.y + padY),
            IM_COL32(255, 255, 255, 255), m_text);
    }

    void OnMenu() override
    {
        const char* views[] = { "First person (hidden)", "Third person (back)", "Third person (front)" };

        ImGui::Indent();
        ImGui::SliderInt("X %", &m_x, 0, 100);
        ImGui::SliderInt("Y %", &m_y, 0, 100);
        ImGui::SliderInt("Size %", &m_size, 80, 300);
        ImGui::TextDisabled("View: %s (F5 to switch)", views[m_view]);
        if (ImGui::Button("Sync: now first person")) m_view = 0;
        ImGui::TextDisabled("Name: edit [SelfTag] in config.ini, then Load");
        ImGui::Unindent();
    }

    void OnSave() override
    {
        Config::SetString(m_name, "name", m_text);
        Config::SetInt(m_name, "x", m_x);
        Config::SetInt(m_name, "y", m_y);
        Config::SetInt(m_name, "size", m_size);
    }

    void OnLoad() override
    {
        Config::GetString(m_name, "name", m_text, m_text, sizeof(m_text));
        m_x = Clamp(Config::GetInt(m_name, "x", m_x), 0, 100);
        m_y = Clamp(Config::GetInt(m_name, "y", m_y), 0, 100);
        m_size = Clamp(Config::GetInt(m_name, "size", m_size), 80, 300);
    }

private:
    static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    char m_text[32];
    int m_x = 50;
    int m_y = 30;
    int m_size = 160;
    int m_view = 0;
    bool m_f5Prev = false;
};