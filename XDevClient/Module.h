#pragma once
#include "imgui.h"
#include "Config.h"

// カテゴリの種類(メニューのタブになる)
enum class Category { Combat, Move, Visual, Hud };

// 全機能のひな形
class Module
{
public:
    Module(const char* name, Category cat, bool enabled = true)
        : m_name(name), m_category(cat), m_enabled(enabled) {
    }
    virtual ~Module() {}

    const char* GetName() const { return m_name; }
    Category GetCategory() const { return m_category; }
    bool IsEnabled() const { return m_enabled; }
    bool& Enabled() { return m_enabled; }

    // 以下は「必要な機能だけが中身を書く」関数。書かなければ何もしない。
    virtual void OnUpdate() {}                          // 毎フレーム呼ばれる(入力処理など)
    virtual void OnRender(ImDrawList* dl, float& y) {}  // HUDを描くときに呼ばれる
    virtual void OnMenu() {}                            // メニューに追加の項目を出す
    virtual void OnUnload() {}                          // DLLを外す直前に呼ばれる(後片付け)
    virtual void OnSave() {}                            // 追加の設定を保存する
    virtual void OnLoad() {}                            // 追加の設定を読み込む
    virtual bool HasToggle() const { return true; }     // falseにすると、オン/オフのスイッチが出ず、常に有効になる

protected:
    const char* m_name;
    Category m_category;
    bool m_enabled;
};