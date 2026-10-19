#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdio>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "ModuleManager.h"
#include "HudModules.h"
#include "MoveModules.h"
#include "CombatModules.h"
#include "VisualModules.h"
#include "DevTools.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
typedef void(__stdcall* ExecuteFn)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

static PresentFn oPresent = nullptr;
static ExecuteFn oExecute = nullptr;
static WNDPROC oWndProc = nullptr;

static const UINT MAX_FRAMES = 8;
static ID3D12Device* g_dev = nullptr;
static ID3D12CommandQueue* g_queue = nullptr;
static ID3D12DescriptorHeap* g_srvHeap = nullptr;
static ID3D12DescriptorHeap* g_rtvHeap = nullptr;
static ID3D12CommandAllocator* g_alloc[MAX_FRAMES] = {};
static ID3D12GraphicsCommandList* g_cmd = nullptr;
static UINT g_bufCount = 0;
static UINT g_rtvInc = 0;
static UINT g_srvInc = 0;
static UINT g_srvNext = 0;
static HWND g_hwnd = nullptr;

static bool g_init = false;
static bool g_initFailed = false;
static bool g_menu = true;
static volatile bool g_unload = false;
static volatile bool g_called = false;
static HRESULT g_hrDev = 0;

// キー入力: Insertでメニューの表示/非表示
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_KEYDOWN && w == VK_INSERT)
        g_menu = !g_menu;

    if (g_menu && ImGui_ImplWin32_WndProcHandler(hWnd, msg, w, l))
        return TRUE;

    return CallWindowProcW(oWndProc, hWnd, msg, w, l);
}

// ゲームのコマンドキューを捕まえる
void __stdcall hkExecute(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* lists)
{
    if (!g_queue && q->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
        g_queue = q;
    oExecute(q, n, lists);
}

static bool InitD3D12(IDXGISwapChain* sc)
{
    g_hrDev = sc->GetDevice(__uuidof(ID3D12Device), (void**)&g_dev);
    if (FAILED(g_hrDev)) { g_initFailed = true; return false; }

    DXGI_SWAP_CHAIN_DESC desc;
    sc->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;
    g_bufCount = desc.BufferCount;
    if (g_bufCount == 0 || g_bufCount > MAX_FRAMES) { g_initFailed = true; return false; }

    D3D12_DESCRIPTOR_HEAP_DESC sd{};
    sd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    sd.NumDescriptors = 64;
    sd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_dev->CreateDescriptorHeap(&sd, __uuidof(ID3D12DescriptorHeap), (void**)&g_srvHeap)))
    {
        g_initFailed = true; return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rd{};
    rd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rd.NumDescriptors = g_bufCount;
    rd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(g_dev->CreateDescriptorHeap(&rd, __uuidof(ID3D12DescriptorHeap), (void**)&g_rtvHeap)))
    {
        g_initFailed = true; return false;
    }

    g_rtvInc = g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_srvInc = g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (UINT i = 0; i < g_bufCount; i++)
    {
        if (FAILED(g_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            __uuidof(ID3D12CommandAllocator), (void**)&g_alloc[i])))
        {
            g_initFailed = true; return false;
        }
    }
    if (FAILED(g_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc[0], nullptr,
        __uuidof(ID3D12GraphicsCommandList), (void**)&g_cmd)))
    {
        g_initFailed = true; return false;
    }
    g_cmd->Close();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // imgui.ini を作らない
    if (GetFileAttributesA("C:\\Windows\\Fonts\\segoeui.ttf") != INVALID_FILE_ATTRIBUTES)
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f);
    Theme::Apply();
    ImGui_ImplWin32_Init(g_hwnd);

    ImGui_ImplDX12_InitInfo ii;
    ii.Device = g_dev;
    ii.CommandQueue = g_queue;
    ii.NumFramesInFlight = (int)g_bufCount;
    ii.RTVFormat = desc.BufferDesc.Format;
    ii.DSVFormat = DXGI_FORMAT_UNKNOWN;
    ii.SrvDescriptorHeap = g_srvHeap;
    ii.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
        {
            UINT i = g_srvNext++;
            *cpu = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
            cpu->ptr += (SIZE_T)i * g_srvInc;
            *gpu = g_srvHeap->GetGPUDescriptorHandleForHeapStart();
            gpu->ptr += (UINT64)i * g_srvInc;
        };
    ii.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};
    ImGui_ImplDX12_Init(&ii);

    oWndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
    return true;
}

static void RenderFrame(IDXGISwapChain* sc)
{
    IDXGISwapChain3* sc3 = nullptr;
    if (FAILED(sc->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&sc3)))
        return;

    UINT idx = sc3->GetCurrentBackBufferIndex();
    ID3D12Resource* bb = nullptr;

    if (idx < g_bufCount &&
        SUCCEEDED(sc3->GetBuffer(idx, __uuidof(ID3D12Resource), (void**)&bb)))
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (SIZE_T)idx * g_rtvInc;
        g_dev->CreateRenderTargetView(bb, nullptr, rtv);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ===== 機能の処理とUI =====
        ModuleManager::Get().menuOpen = g_menu;
        ModuleManager::Get().Update();
        ModuleManager::Get().RenderHud();

        if (g_menu)
        {
            ImVec2 ds = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f - 280.0f, ds.y * 0.5f - 180.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("xdev client", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

            ModuleManager::Get().DrawMenu();

            if (ImGui::Button("Save")) ModuleManager::Get().SaveAll();
            ImGui::SameLine();
            if (ImGui::Button("Load")) ModuleManager::Get().LoadAll();
            ImGui::SameLine();
            if (ImGui::Button("Unload")) g_unload = true;

            ImGui::End();
        }
        ImGui::Render();

        g_alloc[idx]->Reset();
        g_cmd->Reset(g_alloc[idx], nullptr);

        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = bb;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_cmd->ResourceBarrier(1, &b);

        g_cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        g_cmd->SetDescriptorHeaps(1, &g_srvHeap);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmd);

        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_cmd->ResourceBarrier(1, &b);
        g_cmd->Close();

        ID3D12CommandList* lists[] = { g_cmd };
        oExecute(g_queue, 1, lists);
        bb->Release();
    }
    sc3->Release();
}

HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags)
{
    g_called = true;

    if (!g_init && !g_initFailed && g_queue)
        g_init = InitD3D12(sc);

    if (g_init)
        RenderFrame(sc);

    return oPresent(sc, sync, flags);
}

// ダミーのDX12デバイスから、PresentとExecuteCommandListsのアドレスを取得する
static bool GetAddresses(void** present, void** execute)
{
    WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                    GetModuleHandleW(nullptr), 0, 0, 0, 0, L"xdev_dummy", 0 };
    RegisterClassExW(&wc);
    HWND hw = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, 0, 0, wc.hInstance, 0);

    bool ok = false;
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* q = nullptr;
    IDXGISwapChain* sc = nullptr;

    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory)) &&
        SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev)))
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (SUCCEEDED(dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&q)))
        {
            DXGI_SWAP_CHAIN_DESC sd{};
            sd.BufferCount = 2;
            sd.BufferDesc.Width = 100;
            sd.BufferDesc.Height = 100;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hw;
            sd.SampleDesc.Count = 1;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

            if (SUCCEEDED(factory->CreateSwapChain(q, &sd, &sc)))
            {
                *present = (*(void***)sc)[8];  // IDXGISwapChain::Present
                *execute = (*(void***)q)[10];  // ID3D12CommandQueue::ExecuteCommandLists
                ok = true;
            }
        }
    }

    if (sc) sc->Release();
    if (q) q->Release();
    if (dev) dev->Release();
    if (factory) factory->Release();
    DestroyWindow(hw);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

DWORD WINAPI MainThread(LPVOID param)
{
    HMODULE self = (HMODULE)param;

    void* present = nullptr;
    void* execute = nullptr;
    if (!GetAddresses(&present, &execute))
    {
        MessageBoxA(NULL, "address not found", "xdev client", MB_OK);
        FreeLibraryAndExitThread(self, 0);
    }

    // HUD(表示は、登録した順に上から並ぶ)
    ModuleManager::Get().Add<ArrayListModule>();
    ModuleManager::Get().Add<WatermarkModule>();
    ModuleManager::Get().Add<FpsModule>();
    ModuleManager::Get().Add<CpsModule>();
    ModuleManager::Get().Add<KeystrokesModule>();
    ModuleManager::Get().Add<SelfTagModule>();   // Visual(常時有効)
    ModuleManager::Get().Add<DevToolsModule>();   // Visual(開発用)
    // Move
    ModuleManager::Get().Add<SprintModule>();
    // Combat
    ModuleManager::Get().Add<AutoClickerModule>();
    Config::Init();
    ModuleManager::Get().LoadAll(); 

    MH_Initialize();
    MH_CreateHook(execute, &hkExecute, (void**)&oExecute);
    MH_CreateHook(present, &hkPresent, (void**)&oPresent);
    MH_EnableHook(MH_ALL_HOOKS);

    // Endキーでアンロード
    bool endPrev = false;
    while (!g_unload)
    {
        bool end = ModuleManager::Get().IsGameFocused() &&
            (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (end && !endPrev) break;
        endPrev = end;
        Sleep(30);
    }

    MH_DisableHook(MH_ALL_HOOKS);
    Sleep(500);
    ModuleManager::Get().SaveAll();
    ModuleManager::Get().Shutdown();   // 機能の後片付け(AutoClickerの監視を止める)
    if (g_init)
    {
        if (oWndProc) SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        Sleep(300);
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    MH_Uninitialize();
    FreeLibraryAndExitThread(self, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
    }
    return TRUE;
}