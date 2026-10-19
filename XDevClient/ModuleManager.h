#pragma once
#include <vector>
#include <memory>
#include <windows.h>
#include "Module.h"
#include "Theme.h"

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

    // 前面のウィンドウが、このゲームのものか
    bool IsGameFocused() const
    {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // 画面にマウスカーソルが出ているか(インベントリなどを開いているとき)
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

    // メニューの中身(ヘッダー・左のカテゴリ・右の機能一覧)
    void DrawMenu()
    {
        // ヘッダー
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextColored(Theme::Accent(), "xdev client");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();

        // 下のボタン行の分を残して、残りの高さを使う
        float h = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.y;

        // 左: カテゴリ
        ImGui::BeginChild("sidebar", ImVec2(130, h), 0);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Indent(8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
        const char* names[] = { "Combat", "Move", "Visual", "HUD" };
        for (int i = 0; i < 4; i++)
        {
            float w = ImGui::GetContentRegionAvail().x - 8.0f;
            if (ImGui::Selectable(names[i], m_selected == i, 0, ImVec2(w, 32)))
                m_selected = i;
        }
        ImGui::PopStyleVar();
        ImGui::Unindent(8.0f);
        ImGui::EndChild();

        ImGui::SameLine();

        // 右: 選んだカテゴリの機能
        ImGui::BeginChild("content", ImVec2(0, h), 0);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Indent(14.0f);
        DrawModules((Category)m_selected);
        ImGui::Unindent(14.0f);
        ImGui::EndChild();
    }

    // DLLを外す直前の後片付け
    void Shutdown()
    {
        for (auto& m : m_modules)
            m->OnUnload();
    }

    // 全機能の設定を保存する
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
    // 1つのカテゴリの機能を、トグルスイッチで並べる
    void DrawModules(Category cat)
    {
        bool any = false;
        for (auto& m : m_modules)
        {
            if (m->GetCategory() != cat) continue;
            any = true;
            ImGui::PushID(m.get());
            Theme::Toggle(m->GetName(), &m->Enabled());
            m->OnMenu();
            ImGui::PopID();
            ImGui::Spacing();
        }
        if (!any) ImGui::TextDisabled("(no modules)");
    }

    int m_selected = 0;
    std::vector<std::unique_ptr<Module>> m_modules;
};