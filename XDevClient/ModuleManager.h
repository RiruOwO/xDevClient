#pragma once
#include <vector>
#include <memory>
#include <windows.h>
#include "Module.h"

class ModuleManager
{
public:
    // どこからでも同じ1つの管理係にアクセスできるようにする
    static ModuleManager& Get()
    {
        static ModuleManager instance;
        return instance;
    }

    HWND gameWindow = nullptr;   // ゲームのウィンドウ
    bool menuOpen = false;       // xdev clientのメニューが開いているか

    bool IsGameFocused() const
    {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    bool IsCursorShowing() const
    {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        return GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) != 0;
    }

    // 機能を登録する: Add<FpsModule>() のように使う
    template <class T>
    void Add()
    {
        m_modules.push_back(std::unique_ptr<Module>(new T()));
    }

    const std::vector<std::unique_ptr<Module>>& GetModules() const
    {
        return m_modules;
    }

    void Update()
    {
        for (auto& m : m_modules)
            m->OnUpdate();
    }

    void RenderHud()
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        float y = 10.0f;
        for (auto& m : m_modules)
            if (m->IsEnabled()) m->OnRender(dl, y);
    }

    void DrawMenu()
    {
        if (ImGui::BeginTabBar("categories"))
        {
            DrawCategory("Combat", Category::Combat);
            DrawCategory("Move", Category::Move);
            DrawCategory("HUD", Category::Hud);
            ImGui::EndTabBar();
        }
    }

    void Shutdown()
    {
        for (auto& m : m_modules)
            m->OnUnload();
    }

    // 全機能の設定を保存する(オン/オフは共通、それ以外は各機能のOnSaveで)
    void SaveAll()
    {
        for (auto& m : m_modules)
        {
            Config::SetInt(m->GetName(), "enabled", m->IsEnabled() ? 1 : 0);
            m->OnSave();
        }
    }

    // 全機能の設定を読み込む
    void LoadAll()
    {
        for (auto& m : m_modules)
        {
            m->Enabled() = Config::GetInt(m->GetName(), "enabled", m->IsEnabled() ? 1 : 0) != 0;
            m->OnLoad();
        }
    }

private:
    void DrawCategory(const char* label, Category cat)
    {
        if (!ImGui::BeginTabItem(label)) return;

        bool any = false;
        for (auto& m : m_modules)
        {
            if (m->GetCategory() != cat) continue;
            any = true;
            ImGui::PushID(m.get());
            ImGui::Checkbox(m->GetName(), &m->Enabled());
            m->OnMenu();
            ImGui::PopID();
        }
        if (!any) ImGui::TextDisabled("(no modules)");
        ImGui::EndTabItem();
    }

    std::vector<std::unique_ptr<Module>> m_modules;
};