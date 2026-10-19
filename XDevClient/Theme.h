#pragma once
#include <algorithm>
#include "imgui.h"

namespace Theme
{
    // アクセントカラー(青紫)。変えたいときはここの数字を変える
    inline ImVec4 Accent() { return ImVec4(0.42f, 0.56f, 1.00f, 1.00f); }

    inline void Apply()
    {
        ImGuiStyle& s = ImGui::GetStyle();

        // 角丸・余白
        s.WindowRounding = 12.0f;
        s.ChildRounding = 10.0f;
        s.FrameRounding = 7.0f;
        s.PopupRounding = 8.0f;
        s.GrabRounding = 7.0f;
        s.ScrollbarRounding = 8.0f;
        s.WindowPadding = ImVec2(16, 14);
        s.FramePadding = ImVec2(10, 6);
        s.ItemSpacing = ImVec2(10, 10);
        s.GrabMinSize = 14.0f;
        s.ScrollbarSize = 10.0f;

        // 枠線をなくす
        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 0.0f;

        // 色
        ImVec4 a = Accent();
        ImVec4* c = s.Colors;
        c[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.98f, 1.0f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.60f, 1.0f);
        c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.97f);
        c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.12f, 1.0f);
        c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.11f, 0.98f);
        c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
        c[ImGuiCol_SliderGrab] = a;
        c[ImGuiCol_SliderGrabActive] = ImVec4(a.x + 0.10f, a.y + 0.10f, a.z, 1.0f);
        c[ImGuiCol_CheckMark] = a;
        c[ImGuiCol_Button] = ImVec4(0.14f, 0.14f, 0.19f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(a.x, a.y, a.z, 0.55f);
        c[ImGuiCol_ButtonActive] = ImVec4(a.x, a.y, a.z, 0.85f);
        c[ImGuiCol_Header] = ImVec4(a.x, a.y, a.z, 0.25f);
        c[ImGuiCol_HeaderHovered] = ImVec4(a.x, a.y, a.z, 0.40f);
        c[ImGuiCol_HeaderActive] = ImVec4(a.x, a.y, a.z, 0.55f);
        c[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    }

    // トグルスイッチ(アニメーション付き)
    inline bool Toggle(const char* label, bool* v)
    {
        ImGui::PushID(label);

        float h = ImGui::GetFrameHeight();
        float w = h * 1.8f;
        ImVec2 p = ImGui::GetCursorScreenPos();

        bool clicked = ImGui::InvisibleButton("##t", ImVec2(w, h));
        if (clicked) *v = !*v;

        // 0→1 へなめらかに動かす
        ImGuiStorage* st = ImGui::GetStateStorage();
        ImGuiID id = ImGui::GetID("##anim");
        float target = *v ? 1.0f : 0.0f;
        float t = st->GetFloat(id, target);
        float speed = ImGui::GetIO().DeltaTime * 16.0f;
        if (speed > 1.0f) speed = 1.0f;
        t += (target - t) * speed;
        st->SetFloat(id, t);

        ImVec4 a = Accent();
        ImVec4 off(0.20f, 0.20f, 0.26f, 1.0f);
        ImU32 bg = ImGui::GetColorU32(ImVec4(
            off.x + (a.x - off.x) * t,
            off.y + (a.y - off.y) * t,
            off.z + (a.z - off.z) * t, 1.0f));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float trackH = h - 6.0f;
        dl->AddRectFilled(ImVec2(p.x, p.y + 3.0f), ImVec2(p.x + w, p.y + 3.0f + trackH), bg, trackH * 0.5f);

        float knobR = trackH * 0.5f - 2.0f;
        float knobX = p.x + trackH * 0.5f + t * (w - trackH);
        dl->AddCircleFilled(ImVec2(knobX, p.y + h * 0.5f), knobR, IM_COL32(255, 255, 255, 245));

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::PopID();
        return clicked;
    }
}