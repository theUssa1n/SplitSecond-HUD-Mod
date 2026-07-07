// ---------------------------------------------------------
// DRAWING HEADER
// ---------------------------------------------------------
#pragma once
#include <d3d9.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

namespace Drawing {
    // ---------------------------------------------------------
    // CORE FUNCTIONS
    // ---------------------------------------------------------
    void Init(LPDIRECT3DDEVICE9 pDevice);
    void Render(LPDIRECT3DDEVICE9 pDevice);
    
    // ---------------------------------------------------------
    // HELPER FUNCTIONS
    // ---------------------------------------------------------
    void SetVisible(bool visible);
    bool IsConfigOpen();
    bool IsSteamVersion();
    
    // ---------------------------------------------------------
    // INPUT HANDLING
    // ---------------------------------------------------------
    bool OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
