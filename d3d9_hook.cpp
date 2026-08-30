// ---------------------------------------------------------
// INCLUDES
// ---------------------------------------------------------
#include "d3d9_hook.h"
#include <windows.h>
#include "drawing.h"
#include "logger.h"
#include "minhook/MinHook.h" // Ensure you have MinHook installed

// ---------------------------------------------------------
// DEFINITIONS & FORWARD DECLARATIONS
// ---------------------------------------------------------
// Ensure GWLP_WNDPROC is defined for 32-bit compatibility
#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC -4
#endif

// Forward declaration for ImGui WndProc Handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------
// GLOBAL VARIABLES
// ---------------------------------------------------------
D3D9Hook::tEndScene D3D9Hook::oEndScene = nullptr;
D3D9Hook::tReset D3D9Hook::oReset = nullptr;
void* d3d9Device[119]; // Storage for VTable
bool attached = false;
WNDPROC oWndProc = nullptr;
HWND g_hWnd = nullptr;   // window whose WndProc we replaced (to restore on shutdown)

// ---------------------------------------------------------
// HOOKS: SHOW RESULTS
// ---------------------------------------------------------
// Retail: 0x00B6B990 | Steam: 0x00B6B8C0
typedef void(__thiscall* tShowResults)(void* thisPtr);
tShowResults oShowResults = nullptr;

// Address will be set in Initialize based on version or config
void* SHOW_RESULTS_ADDR = nullptr;

void __fastcall hkShowResults(void* thisPtr, void* edx) {
    // Auto-hide HUD when race results appear
    Drawing::SetVisible(false);
    
    if (oShowResults)
        oShowResults(thisPtr);
}

// ---------------------------------------------------------
// HOOKS: SET CURSOR POS
// ---------------------------------------------------------
typedef BOOL (WINAPI* tSetCursorPos)(int, int);
tSetCursorPos oSetCursorPos = nullptr;

BOOL WINAPI hkSetCursorPos(int X, int Y) {
    if (Drawing::IsConfigOpen()) {
        // If config window is open, prevent game from moving the cursor
        return TRUE;
    }
    return oSetCursorPos(X, Y);
}

// ---------------------------------------------------------
// HOOKS: WNDPROC
// ---------------------------------------------------------
// Custom WndProc to handle ImGui inputs
LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 1. Let ImGui handle its inputs
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // 2. While the config window is open, swallow game keyboard input so the
    //    car doesn't react to W/A/S/D etc. while the user edits settings.
    //    Our own hotkeys (F1/K/M) still pass through, and system key messages
    //    (WM_SYSKEYDOWN...) are untouched so Alt+F4 keeps working.
    if (Drawing::IsConfigOpen()) {
        const bool isKeyboard = (msg == WM_KEYDOWN || msg == WM_KEYUP ||
                                 msg == WM_CHAR || msg == WM_DEADCHAR);
        if (isKeyboard && wParam != VK_F1 && wParam != 'K' && wParam != 'M')
            return true;
    }

    // 3. Let our Drawing module handle inputs (Hotkeys)
    if (Drawing::OnWndProc(hWnd, msg, wParam, lParam))
        return true; // Input consumed by our hotkeys

    // 4. Pass to original game WndProc
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------
// HOOKS: RESET
// ---------------------------------------------------------
// The Hooked Reset Function (For Alt-Tab support)
HRESULT APIENTRY hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (!attached) return D3D9Hook::oReset(pDevice, pPresentationParameters);

    // Release our cached state block: D3D9 requires all state blocks to be
    // released before Reset() can succeed. It is re-created on next frame.
    Drawing::OnDeviceLost();

    // Lost Device: Invalidate ImGui objects
    ImGui_ImplDX9_InvalidateDeviceObjects();

    // Call Original Reset
    HRESULT result = D3D9Hook::oReset(pDevice, pPresentationParameters);

    // Reset Succeeded: Re-create ImGui objects
    if (SUCCEEDED(result)) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    return result;
}

// ---------------------------------------------------------
// HOOKS: ENDSCENE
// ---------------------------------------------------------
// The Hooked EndScene Function
HRESULT APIENTRY hkEndScene(LPDIRECT3DDEVICE9 pDevice) {
    if (!attached) return D3D9Hook::oEndScene(pDevice);

    // Initialize ImGui / Drawing once
    static bool init = false;
    if (!init) {
        Drawing::Init(pDevice);
        
        // Hook WndProc for Input Handling
        D3DDEVICE_CREATION_PARAMETERS cp;
        pDevice->GetCreationParameters(&cp);
        oWndProc = (WNDPROC)SetWindowLongPtr(cp.hFocusWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
        g_hWnd = cp.hFocusWindow;
        
        if (oWndProc) {
            Logger::Log("WndProc hooked successfully. Window Handle: " + std::to_string((uintptr_t)cp.hFocusWindow));
        } else {
            Logger::Log("Failed to hook WndProc! Error: " + std::to_string(GetLastError()));
        }
        
        init = true;
    }

    // Call our custom drawing function
    Drawing::Render(pDevice);

    // Call original function
    return D3D9Hook::oEndScene(pDevice);
}

// ---------------------------------------------------------
// HELPER FUNCTIONS
// ---------------------------------------------------------
// Function to find the D3D9 Device VTable address
bool GetD3D9Device(void** pTable, size_t Size) {
    if (!pTable) return false;

    // Create a dummy D3D9 object
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return false;

    // Create our own hidden window: GetForegroundWindow() can return NULL at
    // injection time (no foreground window yet), which makes CreateDevice fail
    // and silently kills the whole mod.
    HWND hDummyWnd = CreateWindowExA(0, "STATIC", "D3D9Hook", WS_POPUP,
                                     0, 0, 100, 100, NULL, NULL,
                                     GetModuleHandleA(nullptr), nullptr);
    if (!hDummyWnd) {
        pD3D->Release();
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hDummyWnd;

    IDirect3DDevice9* pDummyDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hDummyWnd,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);

    bool ok = false;
    if (SUCCEEDED(hr) && pDummyDevice) {
        // Copy the VTable
        memcpy(pTable, *reinterpret_cast<void***>(pDummyDevice), Size);
        pDummyDevice->Release();
        ok = true;
    }

    DestroyWindow(hDummyWnd);
    pD3D->Release();
    return ok;
}

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void D3D9Hook::Initialize(HMODULE hModule) {
    Logger::Log("D3D9Hook::Initialize started");
    
    // 1. Get D3D9 VTable
    if (GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
        Logger::Log("D3D9 VTable found at " + std::to_string((uintptr_t)d3d9Device));
        
        // 2. Initialize MinHook
        MH_STATUS mhStatus = MH_Initialize();
        if (mhStatus != MH_OK) {
            Logger::Log("Failed to initialize MinHook! Error: " + std::to_string(mhStatus));
            return;
        }
        Logger::Log("MinHook initialized");

        // 3. Create Hook for EndScene (Index 42)
        mhStatus = MH_CreateHook(d3d9Device[42], reinterpret_cast<void*>(&hkEndScene), reinterpret_cast<void**>(&oEndScene));
        if (mhStatus != MH_OK) {
            Logger::Log("Failed to hook EndScene! Error: " + std::to_string(mhStatus));
            return;
        }
        Logger::Log("EndScene hook created");

        // 4. Create Hook for Reset (Index 16)
        mhStatus = MH_CreateHook(d3d9Device[16], reinterpret_cast<void*>(&hkReset), reinterpret_cast<void**>(&oReset));
        if (mhStatus != MH_OK) {
            Logger::Log("Failed to hook Reset! Error: " + std::to_string(mhStatus));
            return;
        }
        Logger::Log("Reset hook created");

        // 5. Create Hook for SetCursorPos
        mhStatus = MH_CreateHookApi(L"user32", "SetCursorPos", reinterpret_cast<void*>(&hkSetCursorPos), reinterpret_cast<void**>(&oSetCursorPos));
        if (mhStatus != MH_OK) {
             Logger::Log("Warning: Failed to hook SetCursorPos! Error: " + std::to_string(mhStatus));
        } else {
             Logger::Log("SetCursorPos hook created");
        }

        // 6. Enable Hooks
        mhStatus = MH_EnableHook(MH_ALL_HOOKS);
        if (mhStatus != MH_OK) {
            Logger::Log("Failed to enable hooks! Error: " + std::to_string(mhStatus));
            return;
        }
        Logger::Log("All hooks enabled");

        // 7. Resolve and Create Hook for ShowResults (Auto-Hide) safely
        // Steam: verified 0xB6B8E0 (the debug-HUD-setup function, not 0xB6B8C0
        // which is a math function right before it).
        SHOW_RESULTS_ADDR = Drawing::IsSteamVersion() ? (void*)0x00B6B8E0 : (void*)0x00B6B990;
        Logger::Log("Target ShowResults address: " + std::to_string((uintptr_t)SHOW_RESULTS_ADDR));
        
        if (SHOW_RESULTS_ADDR) {
            mhStatus = MH_CreateHook(SHOW_RESULTS_ADDR, reinterpret_cast<void*>(&hkShowResults), reinterpret_cast<void**>(&oShowResults));
            if (mhStatus == MH_OK) {
                MH_EnableHook(SHOW_RESULTS_ADDR);
                Logger::Log("ShowResults hook created and enabled");
            } else {
                Logger::Log("Warning: Failed to hook ShowResults! Error: " + std::to_string(mhStatus));
            }
        }

        attached = true;
        Logger::Log("D3D9Hook::Initialize finished successfully");
    } else {
        Logger::Log("Failed to find D3D9 VTable!");
    }
}

// ---------------------------------------------------------
// SHUTDOWN
// ---------------------------------------------------------
void D3D9Hook::Shutdown() {
    attached = false;

    // Restore the original WndProc if ours is still installed
    if (oWndProc && g_hWnd) {
        if ((WNDPROC)GetWindowLongPtr(g_hWnd, GWLP_WNDPROC) == WndProc)
            SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = nullptr;
        g_hWnd = nullptr;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
