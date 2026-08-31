#include "drawing.h"
#include "embedded_fonts.h"
#include "logger.h"
#include <string>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <fstream>

// ---------------------------------------------------------
// CONFIGURATION STORAGE
// ---------------------------------------------------------

namespace {
    struct SpeedometerConfig {
        // Global Settings
        bool useMetric = false;
        float globalScale = 1.0f;
        float globalPosX = 40.0f;       // Padding from Right
        float globalPosY = 20.0f;       // Padding from Bottom
        bool isVisible = true;
        bool smartLayout = true;        // Auto-center and dynamic anchoring

        // Speed Value Settings
        bool showSpeed = true;
        float speedScale = 1.0f;
        float speedOffsetX = 0.0f;
        float speedOffsetY = 0.0f;

        // Speed Unit Settings
        bool showUnit = true;
        float unitScale = 1.0f;
        float unitOffsetX = 100.0f;     // Default offset relative to start, not speed text end
        float unitOffsetY = -5.0f;      // Slightly move up by default (User Preference)
        float smartGap = 10.0f;         // Gap between speed and unit in Smart Layout

        // Gauge Bar Settings
        bool showBar = true;
        float barWidth = 350.0f;
        float barHeight = 20.0f;
        float barSkew = 20.0f;
        float barOffsetX = 0.0f;
        float barOffsetY = 10.0f;       // Relative to text bottom

        // Internal State
        bool showConfigWindow = false;
        
        // Font Selection
        // 0 = Default (Pixel)
        // 1 = Dash Horizon
        // 2 = Sprintura
        int fontStyle = 0;
    };

    SpeedometerConfig config;
    
    // Global Font Pointers
    ImFont* font_default = nullptr; // Index 0 (ImGui Default Pixel)
    ImFont* font_dash = nullptr;    // Index 1 (Dash Horizon)
    ImFont* font_sprint = nullptr;  // Index 2 (Sprintura)
    // Removed Arial/Sport font as requested
    
    std::string GetConfigPath() {
        char path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            std::string p(path);
            size_t pos = p.find_last_of("\\/");
            return p.substr(0, pos) + "\\speedometer.ini";
        }
        return "speedometer.ini";
    }

    // Cached game state — updated once per frame in Render(), read by
    // OnWndProc() so we never call into game code from the message pump.
    bool g_inRace = false;
    bool g_isPaused = false;

    // Cached D3D9 state block (created once, released on device reset).
    IDirect3DStateBlock9* g_stateBlock = nullptr;
}

// ---------------------------------------------------------
// GAME ADDRESSES 
// ---------------------------------------------------------
// --- VERSION SELECTION ---
// Uncomment the line below to build for STEAM version. Keep commented for RETAIL/CRACKED.
#define USE_STEAM_VERSION

#ifdef USE_STEAM_VERSION
    // STEAM[xenon] Version Addresses
    uintptr_t SPEED_ADDRESS = 0x00D7E2F4;
    uintptr_t IN_GAME_UI_PTR = 0xD5A170;
    const uintptr_t IS_PAUSED_FUNC_ADDR = 0x798290;
#else
    // RETAIL (Cracked) Version Addresses
    uintptr_t SPEED_ADDRESS = 0x00D8B0EC;
    uintptr_t IN_GAME_UI_PTR = 0xd66ad0;
    const uintptr_t IS_PAUSED_FUNC_ADDR = 0x7990b0;
#endif

using is_game_paused_function = bool(__fastcall*)(void* thisPtr, void* edxDummy);

// ---------------------------------------------------------
// STATE VARIABLES
// ---------------------------------------------------------
bool wasInRace = false;

// ---------------------------------------------------------
// HELPER FUNCTIONS
// ---------------------------------------------------------

void SaveConfig() {
    std::ofstream file(GetConfigPath());
    if (file.is_open()) {
        #define SAVE_VAR(name, var) file << name << "=" << var << "\n"
        
        SAVE_VAR("useMetric", config.useMetric);
        SAVE_VAR("globalScale", config.globalScale);
        SAVE_VAR("globalPosX", config.globalPosX);
        SAVE_VAR("globalPosY", config.globalPosY);
        SAVE_VAR("isVisible", config.isVisible);
        SAVE_VAR("smartLayout", config.smartLayout);
        
        SAVE_VAR("showSpeed", config.showSpeed);
        SAVE_VAR("speedScale", config.speedScale);
        SAVE_VAR("speedOffsetX", config.speedOffsetX);
        SAVE_VAR("speedOffsetY", config.speedOffsetY);
        
        SAVE_VAR("showUnit", config.showUnit);
        SAVE_VAR("unitScale", config.unitScale);
        SAVE_VAR("unitOffsetX", config.unitOffsetX);
        SAVE_VAR("unitOffsetY", config.unitOffsetY);
        SAVE_VAR("smartGap", config.smartGap);

        SAVE_VAR("showBar", config.showBar);
        SAVE_VAR("barWidth", config.barWidth);
        SAVE_VAR("barHeight", config.barHeight);
        SAVE_VAR("barSkew", config.barSkew);
        SAVE_VAR("barOffsetX", config.barOffsetX);
        SAVE_VAR("barOffsetY", config.barOffsetY);
        SAVE_VAR("fontStyle", config.fontStyle);
        
        #undef SAVE_VAR
        file.close();
    }
}

void LoadConfig() {
    std::ifstream file(GetConfigPath());
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                float fVal = 0.0f;
                try { fVal = std::stof(value); } catch(...) {}
                int iVal = (int)fVal;

                #define LOAD_BOOL(name, var) if (key == name) var = (bool)iVal
                #define LOAD_FLOAT(name, var) else if (key == name) var = fVal

                LOAD_BOOL("useMetric", config.useMetric);
                LOAD_FLOAT("globalScale", config.globalScale);
                LOAD_FLOAT("globalPosX", config.globalPosX);
                LOAD_FLOAT("globalPosY", config.globalPosY);
                LOAD_BOOL("isVisible", config.isVisible);
                LOAD_BOOL("smartLayout", config.smartLayout);
                LOAD_BOOL("showSpeed", config.showSpeed);
                LOAD_FLOAT("speedScale", config.speedScale);
                LOAD_FLOAT("speedOffsetX", config.speedOffsetX);
                LOAD_FLOAT("speedOffsetY", config.speedOffsetY);
                LOAD_BOOL("showUnit", config.showUnit);
                LOAD_FLOAT("unitScale", config.unitScale);
                LOAD_FLOAT("unitOffsetX", config.unitOffsetX);
                LOAD_FLOAT("unitOffsetY", config.unitOffsetY);
                LOAD_FLOAT("smartGap", config.smartGap);
                LOAD_BOOL("showBar", config.showBar);
                LOAD_FLOAT("barWidth", config.barWidth);
                LOAD_FLOAT("barHeight", config.barHeight);
                LOAD_FLOAT("barSkew", config.barSkew);
                LOAD_FLOAT("barOffsetX", config.barOffsetX);
                LOAD_FLOAT("barOffsetY", config.barOffsetY);
                
                if (key == "fontStyle") config.fontStyle = iVal;

                #undef LOAD_BOOL
                #undef LOAD_FLOAT
            }
        }
        file.close();
    }
}

void ResetConfig() {
    config.globalScale = 1.0f;
    config.globalPosX = 30.0f;
    config.globalPosY = 15.0f;
    config.smartLayout = true;
    // Keep isVisible and useMetric as is, or reset? Resetting usually implies layout.
    // Let's keep preference for unit but reset layout.
    
    config.showSpeed = true;
    config.speedScale = 1.0f;
    config.speedOffsetX = 0.0f;
    config.speedOffsetY = 0.0f;
    
    config.showUnit = true;
    config.unitScale = 1.0f;
    config.unitOffsetX = 100.0f;
    config.unitOffsetY = -5.0f; // Slightly move up by default (User Preference)
    config.smartGap = 10.0f;

    config.showBar = true;
    config.barWidth = 350.0f;
    config.barHeight = 20.0f;
    config.barSkew = 20.0f;
    config.barOffsetX = 0.0f;
    config.barOffsetY = 10.0f;
}

// ---------------------------------------------------------
// INPUT HANDLING
// ---------------------------------------------------------

bool CallIsGamePaused(uintptr_t funcAddr, uintptr_t thisPtr);

bool Drawing::OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Hotkeys only — game state comes from the per-frame cache in Render(),
    // so nothing expensive (game calls, IsBadReadPtr, logging) happens here
    // for every single window message.
    if (msg != WM_KEYDOWN)
        return false;

    // Allow hotkeys only if in-game and not paused
    if (!g_inRace || g_isPaused)
        return false;

    if (wParam == 'K') {
        config.isVisible = !config.isVisible;
        return true; // Consume input
    }
    if (wParam == 'M') {
        config.useMetric = !config.useMetric;
        return true; // Consume input
    }
    if (wParam == VK_F1) {
        config.showConfigWindow = !config.showConfigWindow;
        return true; // Consume input
    }

    return false; // Don't consume other inputs
}

bool CallIsGamePaused(uintptr_t funcAddr, uintptr_t thisPtr) {
    if (funcAddr == 0 || thisPtr == 0) return false;
    auto func = reinterpret_cast<is_game_paused_function>(funcAddr);
    return func((void*)thisPtr, nullptr);
}

ImU32 GetSpeedColor(float fraction) {
    float r, g, b;
    if (fraction <= 0.5f) {
        float t = fraction * 2.0f; 
        r = t; g = 1.0f; b = 1.0f - t;
    } else {
        float t = (fraction - 0.5f) * 2.0f;
        r = 1.0f; g = 1.0f - t; b = 0.0f;
    }
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
}

// ---------------------------------------------------------
// MAIN DRAWING
// ---------------------------------------------------------

void Drawing::Init(LPDIRECT3DDEVICE9 pDevice) {
    Logger::Log("Drawing::Init started");
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    
    // Load High-Res Font
    // We load fonts at 20px to match standard ImGui size (approx) but with better quality
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    
    // 1. Default Font: Use ImGui's built-in pixel font (ProggyClean)
    // This is fixed size (~13px).
    font_default = io.Fonts->AddFontDefault();
    Logger::Log("Default font loaded");

    // 2. Load Racing Font 1: Dash Horizon (Embedded)
    font_dash = nullptr;
    ImFontConfig font_cfg_dash = font_cfg;
    strcpy_s(font_cfg_dash.Name, "Dash Horizon");
    // Load at 13.0f to match Default Font size (Consistency in scaling)
    if (io.Fonts->AddFontFromMemoryTTF((void*)font_data_dash, sizeof(font_data_dash), 13.0f, &font_cfg_dash) != nullptr) {
        font_dash = io.Fonts->Fonts.back();
        Logger::Log("Dash Horizon font loaded from memory");
    }
    
    if (!font_dash) {
        font_dash = font_default;
        Logger::Log("Warning: Dash Horizon failed, fallback to default");
    }

    // 3. Load Racing Font 2: Sprintura (Embedded)
    font_sprint = nullptr;
    ImFontConfig font_cfg_sprint = font_cfg;
    strcpy_s(font_cfg_sprint.Name, "Sprintura");
    // Load at 13.0f to match Default Font size (Consistency in scaling)
    if (io.Fonts->AddFontFromMemoryTTF((void*)font_data_sprint, sizeof(font_data_sprint), 13.0f, &font_cfg_sprint) != nullptr) {
        font_sprint = io.Fonts->Fonts.back();
        Logger::Log("Sprintura font loaded from memory");
    }

    if (!font_sprint) {
        font_sprint = font_default;
        Logger::Log("Warning: Sprintura failed, fallback to default");
    }

    D3DDEVICE_CREATION_PARAMETERS cp;
    pDevice->GetCreationParameters(&cp);
    ImGui_ImplWin32_Init(cp.hFocusWindow);
    ImGui_ImplDX9_Init(pDevice);
    Logger::Log("ImGui Backends initialized");

    LoadConfig();
    Logger::Log("Configuration loaded");
    
    #ifdef USE_STEAM_VERSION
        Logger::Log("Build Mode: STEAM");
        Logger::Log("Target Speed Address: 0x00D7E2F4");
    #else
        Logger::Log("Build Mode: RETAIL/CRACKED");
        Logger::Log("Target Speed Address: 0x00D8B0EC");
    #endif

    // Verify addresses
    if (IsBadReadPtr((void*)SPEED_ADDRESS, sizeof(float))) {
        Logger::Log("Warning: SPEED_ADDRESS is NOT readable at this time.");
    } else {
        Logger::Log("SPEED_ADDRESS is readable. Current raw value: " + std::to_string(*reinterpret_cast<float*>(SPEED_ADDRESS)));
    }
    
    if (IsBadReadPtr((void*)IN_GAME_UI_PTR, sizeof(uint32_t))) {
        Logger::Log("Warning: IN_GAME_UI_PTR is NOT readable.");
    } else {
        uint32_t ui_val = *reinterpret_cast<uint32_t*>(IN_GAME_UI_PTR);
        Logger::Log("IN_GAME_UI_PTR is readable. Value: " + std::to_string(ui_val));
    }
}

void DrawConfigWindow() {
    if (!config.showConfigWindow) return;

    // Sizes are in logical (window client) pixels — the standard ImGui space.
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    // Panel font: default 13px at logical size. The draw data is scaled up to
    // the render target in Render(), so at higher internal resolutions the
    // text reads "thinner" rather than blurry.
    const bool fontPushed = (font_default != nullptr);
    if (fontPushed)
        ImGui::PushFont(font_default, 13.0f);

    if (ImGui::Begin("Speedometer Configuration", &config.showConfigWindow)) {
        
        ImGui::Text("Global Settings");
        ImGui::Separator();
        
        const char* fontItems[] = { "Default (Pixel)", "Dash Horizon", "Sprintura" };
        ImGui::Combo("Font Style", &config.fontStyle, fontItems, IM_ARRAYSIZE(fontItems));
        
        if (ImGui::Button(config.useMetric ? "Unit: KM/H" : "Unit: MPH")) {
            config.useMetric = !config.useMetric;
        }
        ImGui::SameLine();
        if (ImGui::Button(config.isVisible ? "HUD: Visible" : "HUD: Hidden")) {
            config.isVisible = !config.isVisible;
        }

        ImGui::Checkbox("Smart Layout", &config.smartLayout);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto-centers text and dynamically positions unit");

        ImGui::SliderFloat("Global Scale", &config.globalScale, 0.5f, 3.0f);
        ImGui::DragFloat("Pos X (Right)", &config.globalPosX, 1.0f, -2000.0f, 4000.0f);
        ImGui::DragFloat("Pos Y (Bottom)", &config.globalPosY, 1.0f, -2000.0f, 2000.0f);

        ImGui::Spacing();
        ImGui::Text("Speed Value Settings");
        ImGui::Separator();
        ImGui::Checkbox("Show Value", &config.showSpeed);
        ImGui::SliderFloat("Value Scale", &config.speedScale, 0.5f, 3.0f);
        if (!config.smartLayout) {
            ImGui::DragFloat("Value Off X", &config.speedOffsetX, 1.0f, -200.0f, 200.0f);
        } else {
            ImGui::TextDisabled("Value Off X (Auto)");
        }
        ImGui::DragFloat("Value Off Y", &config.speedOffsetY, 1.0f, -200.0f, 200.0f);

        ImGui::Spacing();
        ImGui::Text("Speed Unit Settings");
        ImGui::Separator();
        ImGui::Checkbox("Show Unit", &config.showUnit);
        ImGui::SliderFloat("Unit Scale", &config.unitScale, 0.5f, 3.0f);
        
        if (config.smartLayout) {
            ImGui::SliderFloat("Unit Gap", &config.smartGap, 0.0f, 100.0f);
        } else {
            ImGui::DragFloat("Unit Off X", &config.unitOffsetX, 1.0f, -200.0f, 400.0f);
        }
        ImGui::DragFloat("Unit Off Y", &config.unitOffsetY, 1.0f, -200.0f, 200.0f);

        ImGui::Spacing();
        ImGui::Text("Gauge Bar Settings");
        ImGui::Separator();
        ImGui::Checkbox("Show Bar", &config.showBar);
        ImGui::SliderFloat("Bar Width", &config.barWidth, 100.0f, 600.0f);
        ImGui::SliderFloat("Bar Height", &config.barHeight, 5.0f, 50.0f);
        ImGui::SliderFloat("Bar Skew", &config.barSkew, 0.0f, 50.0f);
        ImGui::DragFloat("Bar Off X", &config.barOffsetX, 1.0f, -200.0f, 200.0f);
        ImGui::DragFloat("Bar Off Y", &config.barOffsetY, 1.0f, -200.0f, 200.0f);

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Reset to Default")) {
            ResetConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Configuration")) {
            SaveConfig();
        }
    }
    ImGui::End();
    if (fontPushed)
        ImGui::PopFont();
}

// Helper for safe __thiscall (Implementation moved to top)

void Drawing::Render(LPDIRECT3DDEVICE9 pDevice) {
    // 0. Safety Check: Avoid rendering if device is lost or not ready
    if (pDevice->TestCooperativeLevel() != D3D_OK)
        return;

    static bool firstRender = true;
    if (firstRender) {
        Logger::Log("First Render frame started (EndScene called)");
        firstRender = false;
    }

    // Save State Block to prevent graphical glitches.
    // Created once and reused every frame (creating/releasing per frame is
    // expensive); released in OnDeviceLost() before a device reset.
    if (!g_stateBlock) {
        if (FAILED(pDevice->CreateStateBlock(D3DSBT_ALL, &g_stateBlock)))
            g_stateBlock = nullptr;
    }
    if (g_stateBlock)
        g_stateBlock->Capture();

    // Backend NewFrame: the Win32 backend sets io.DisplaySize to the window
    // CLIENT size every frame — that IS our logical coordinate space (and the
    // same space mouse messages use, so hit-testing is exact by construction).
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // --- RESOLUTION SETUP ---
    ImGuiIO& io = ImGui::GetIO();

    // The game may render into a surface LARGER than the window (DSR /
    // internal supersampling). We keep ImGui in window-client (logical) space
    // and scale the ImDrawData geometry up to the render target ourselves
    // right before RenderDrawData (the same approach other Split/Second
    // overlay mods use). io.DisplayFramebufferScale stays (1,1) so fonts are
    // baked at their logical size.
    float surfaceW = 0.0f, surfaceH = 0.0f;

    IDirect3DSurface9* rt = nullptr;
    if (SUCCEEDED(pDevice->GetRenderTarget(0, &rt)) && rt) {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(rt->GetDesc(&desc))) {
            surfaceW = (float)desc.Width;
            surfaceH = (float)desc.Height;
        }
        rt->Release();
    }

    if (surfaceW <= 0) {
        IDirect3DSurface9* bb = nullptr;
        if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
            D3DSURFACE_DESC desc;
            if (SUCCEEDED(bb->GetDesc(&desc))) {
                surfaceW = (float)desc.Width;
                surfaceH = (float)desc.Height;
            }
            bb->Release();
        }
    }

    if (surfaceW <= 0) {
        D3DVIEWPORT9 vp;
        pDevice->GetViewport(&vp);
        surfaceW = (float)vp.Width;
        surfaceH = (float)vp.Height;
    }

    float fbScaleX = 1.0f, fbScaleY = 1.0f;
    if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f && surfaceW > 0.0f && surfaceH > 0.0f) {
        fbScaleX = surfaceW / io.DisplaySize.x;
        fbScaleY = surfaceH / io.DisplaySize.y;
        if (fbScaleX < 0.25f) fbScaleX = 0.25f;
        if (fbScaleX > 4.0f)  fbScaleX = 4.0f;
        if (fbScaleY < 0.25f) fbScaleY = 0.25f;
        if (fbScaleY > 4.0f)  fbScaleY = 4.0f;
    }

    static float lastFbScaleX = 0.0f;
    if (fbScaleX != lastFbScaleX) {
        Logger::Log("Surface " + std::to_string((int)surfaceW) + "x" + std::to_string((int)surfaceH) +
                    ", client " + std::to_string((int)io.DisplaySize.x) + "x" + std::to_string((int)io.DisplaySize.y) +
                    ", framebuffer scale " + std::to_string(fbScaleX));
        lastFbScaleX = fbScaleX;
    }

    // Style: keep defaults — no per-frame scaling noise needed.
    io.FontGlobalScale = 1.0f;

    // ---------------------------------------------------------

    ImGui::NewFrame();

    // 1. Read Game Data
    float currentSpeed = 0.0f;
    std::uint32_t in_game_ui = 0;
    bool isPaused = false;

    static bool speedAddrFound = false;
    static bool uiAddrFound = false;

    if (SPEED_ADDRESS != 0) {
        // Basic pointer check before read
        if (!IsBadReadPtr((void*)SPEED_ADDRESS, sizeof(float))) {
            currentSpeed = *(float*)SPEED_ADDRESS;
            if (!speedAddrFound) {
                Logger::Log("Speed address is readable: " + std::to_string(currentSpeed));
                speedAddrFound = true;
            }
        }
    }

    if (IN_GAME_UI_PTR != 0) {
        if (!IsBadReadPtr((void*)IN_GAME_UI_PTR, sizeof(std::uint32_t))) {
            in_game_ui = *reinterpret_cast<std::uint32_t*>(IN_GAME_UI_PTR);
            if (!uiAddrFound && in_game_ui != 0) {
                Logger::Log("In-Game UI pointer resolved: " + std::to_string(in_game_ui));
                uiAddrFound = true;
            }
        }
    }

    if (in_game_ui != 0) {
        // Safe call wrapper
        isPaused = CallIsGamePaused(IS_PAUSED_FUNC_ADDR, in_game_ui);
    }

    // Read the InGameUI view state ([InGameUI+8]). This is the signal that
    // tells us whether the 3D HUD (lap/rank/powerplay) is actually up:
    //   0 = racing, HUD up
    //   1 = modal state (not racing-critical; keep HUD for now)
    //   2,3 = paused
    //   4 = race results (HUD hidden)
    //   5 = loading / mode-map info screen (HUD hidden)
    int viewState = -1;
    if (in_game_ui != 0 && !IsBadReadPtr((void*)(in_game_ui + 8), sizeof(int)))
        viewState = *(int*)(in_game_ui + 8);

    // Hide for every known non-racing view state (pause/results/loading).
    // Debounce: on transitions (entering loading, leaving results) the state
    // can briefly report "Racing" (0) — on the Steam build this lasts up to
    // ~1s at the very start of a loading screen before it flips to Loading
    // (5). If that transient 0 was trusted the HUD would blink once per
    // loading screen (retail either has no such transient or it is shorter).
    // Fix: keep a "pre-race" flag that holds the HUD (longer grace) until the
    // race's Loading state (5) has been confirmed or the 0 state proved
    // stable, then fall back to the normal short grace. DeltaTime is clamped
    // (0.1s) so a single hitched frame can't jump the timer past the grace.
    static float hudShowTimer = 0.0f;
    static bool s_preRace = true;   // waiting for the race's Loading (5) state
    bool hudUp = false;
    const bool isHideView = (viewState == 2 || viewState == 3 || viewState == 4 || viewState == 5);
    if (in_game_ui == 0) {
        s_preRace = true;           // fresh entry (menu): don't trust the next 0 yet
        hudShowTimer = 0.0f;
        hudUp = false;
    } else if (isHideView) {
        if (viewState == 5)
            s_preRace = false;      // Loading confirmed -> next 0 is the real race
        else if (viewState == 4)
            s_preRace = true;       // results -> a rematch may re-enter via a 0 transient
        hudShowTimer = 0.0f;
        hudUp = false;
    } else {
        const float dt = (io.DeltaTime > 0.1f) ? 0.1f : io.DeltaTime;
        const float grace = s_preRace ? 3.0f : 0.75f;
        hudShowTimer += dt;
        hudUp = (hudShowTimer >= grace);
        if (hudUp)
            s_preRace = false;      // 0 proved stable -> treat as a real race
    }

    // Cache the game state for the WndProc hotkey handler (one update per
    // frame — OnWndProc() never calls into the game itself).
    g_inRace = (in_game_ui != 0) && hudUp;
    g_isPaused = isPaused;

    // 2. Handle Inputs
    // Input handling is now done in OnWndProc to avoid Alt-Tab issues.
    
    // Safety: Force close config window if not in race or paused
    if (in_game_ui == 0 || isPaused || !hudUp) {
        config.showConfigWindow = false;
    }

    // Auto-Show Logic (Race Start)
    // "In race" now means: InGameUI exists AND the 3D HUD view is up
    // (so loading screens, results and pause don't count as racing).
    bool currentlyInRace = (in_game_ui != 0) && hudUp;
    if (currentlyInRace && !wasInRace) {
        config.isVisible = true;
    }
    wasInRace = currentlyInRace;

    // 3. Render
    // Separate HUD visibility from Config Window visibility
    bool shouldDrawHUD = config.isVisible && currentlyInRace && !isPaused;
    bool shouldDrawConfig = config.showConfigWindow;

    // Mouse cursor: use the OS hardware cursor while the config window is
    // open (like GM does). A software cursor (io.MouseDrawCursor) is only
    // redrawn once per rendered frame, so with the game's refresh rate it
    // feels laggy — the hardware cursor moves at the mouse's own polling
    // rate, independent of the game's framerate.
    // Re-show every frame (in case the game hides the cursor again) and undo
    // exactly the number of ShowCursor(TRUE) calls we made when it closes.
    static int s_cursorShown = 0;
    if (shouldDrawConfig) {
        // The game may have hidden the cursor several times (deep negative
        // display count) — boost until it is actually visible, counting EVERY
        // ShowCursor(TRUE) we issue so closing undoes them exactly (otherwise
        // the cursor is left visible after close).
        int count;
        do {
            count = ShowCursor(TRUE);
            s_cursorShown++;
        } while (count < 0);
    } else if (s_cursorShown > 0) {
        for (int i = 0; i < s_cursorShown; i++)
            ShowCursor(FALSE);
        s_cursorShown = 0;
    }
    io.MouseDrawCursor = false;

    // Always attempt to draw config window if enabled
    // Moved to the end of Render to ensure it's drawn ON TOP of the HUD

    if (shouldDrawHUD) {
        // Calculate Speed & Units
        float displaySpeed = currentSpeed;
        std::string unitStr = "MPH";
        float maxSpeedGauge = 183.0f;

        if (config.useMetric) {
            displaySpeed = currentSpeed * 1.60934f;
            unitStr = "KM/H";
            maxSpeedGauge = 294.4f;
        }

        // HUD Rendering
        float scaleFactor = (io.DisplaySize.y / 1080.0f) * config.globalScale;
        if (scaleFactor < 0.1f) scaleFactor = 0.1f;

        // Safety check for invalid positions (NaN or Infinity)
        if (std::isnan(config.globalPosX)) config.globalPosX = 40.0f;
        if (std::isnan(config.globalPosY)) config.globalPosY = 40.0f;

        // Anchor: bottom-right corner of the HUD area
        ImVec2 anchor(io.DisplaySize.x - config.globalPosX * scaleFactor,
                      io.DisplaySize.y - config.globalPosY * scaleFactor);

        // Select Font based on config
        ImFont* currentFont = font_default;
        if (config.fontStyle == 1 && font_dash) currentFont = font_dash;
        if (config.fontStyle == 2 && font_sprint) currentFont = font_sprint;

        // Explicit pixel font sizes (13px base * legacy 5x / 2x factors).
        // Logical pixels — passed straight to AddText(): the 1.92 font system
        // bakes glyphs at size * framebuffer density, so text stays crisp at
        // any internal resolution. (The old SetWindowFontScale path multiplied
        // the window scale instead.)
        float speedFontPx = 65.0f * config.globalScale * config.speedScale;
        float unitFontPx  = 26.0f * config.globalScale * config.unitScale;

        // Prepare strings and measure them at their real sizes
        char speedText[32];
        sprintf_s(speedText, sizeof(speedText), "%d", (int)lroundf(displaySpeed));

        ImVec2 speedSize(0, 0);
        if (config.showSpeed)
            speedSize = currentFont->CalcTextSizeA(speedFontPx, FLT_MAX, 0.0f, speedText);

        ImVec2 unitSize(0, 0);
        if (config.showUnit)
            unitSize = currentFont->CalcTextSizeA(unitFontPx, FLT_MAX, 0.0f, unitStr.c_str());

        // Draw on the background draw list: no window, no Dummy(), no
        // estimated-size clipping — the HUD can never be cut off anymore,
        // and the config window still renders on top of it.
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        // Calculate Positions
        ImVec2 speedPos(0, 0);
        ImVec2 unitPos(0, 0);
        ImVec2 barStart(0, 0);

        // Bar dimensions
        float bWidth = config.barWidth * scaleFactor;
        float bHeight = config.barHeight * scaleFactor;
        float bSkew = config.barSkew * scaleFactor;

        // Stable reference for the whole HUD block: a 400x200 (scaled) box
        // whose bottom-right corner sits on the anchor.
        float refX = anchor.x - 400.0f * scaleFactor;
        float refY = anchor.y - 200.0f * scaleFactor;

        if (config.smartLayout) {
            // Stable visual center of the HUD block (200 scaled px left of
            // the anchor). Everything below is anchored to fixed points, so
            // the layout never depends on the current text size.
            float visualCenterX = refX + 200.0f * scaleFactor;

            // Right-align the text block at a FIXED right edge: the unit
            // never moves and the number grows leftward as digits are added.
            // This removes the horizontal jitter the old centered layout had
            // whenever the digit count changed (99 -> 100 -> ...).
            float rightEdge = visualCenterX + 100.0f * scaleFactor;
            float gap = config.smartGap * scaleFactor;
            float currentX = rightEdge;
            if (config.showUnit) {
                unitPos.x = currentX - unitSize.x;
                currentX = unitPos.x - gap;
            }
            if (config.showSpeed)
                speedPos.x = currentX - speedSize.x;

            // Y: absolute offsets relative to the reference top
            speedPos.y = refY + config.speedOffsetY * scaleFactor;
            unitPos.y  = refY + config.unitOffsetY * scaleFactor;

            // Bar centered on visualCenterX + barOffsetX (decoupled from text)
            float effectiveBarCenterOffset = (bWidth / 2.0f) - (bSkew / 2.0f);
            float barCenterX = visualCenterX + config.barOffsetX * scaleFactor;
            barStart.x = barCenterX - effectiveBarCenterOffset;

            // Bar Y: below the tallest text, independent of text offsets
            float maxTextHeight = 0.0f;
            if (config.showSpeed) maxTextHeight = (std::max)(maxTextHeight, speedSize.y);
            if (config.showUnit)  maxTextHeight = (std::max)(maxTextHeight, unitSize.y);
            float barPadding = 5.0f * scaleFactor;
            barStart.y = refY + maxTextHeight + config.barOffsetY * scaleFactor + barPadding;
        } else {
            // Legacy / Manual Layout — offsets relative to the fixed reference
            // box. (The old version measured them from a window whose size
            // depended on the estimated content, which made these offsets
            // shift around when the bar width changed.)
            speedPos = ImVec2(refX + config.speedOffsetX * scaleFactor,
                              refY + config.speedOffsetY * scaleFactor);
            unitPos  = ImVec2(refX + config.unitOffsetX * scaleFactor,
                              refY + config.unitOffsetY * scaleFactor);

            float currentTextBottom = refY;
            if (config.showSpeed) currentTextBottom += speedSize.y;

            float barPadding = 5.0f * scaleFactor;
            barStart = ImVec2(refX + config.barOffsetX * scaleFactor,
                              currentTextBottom + config.barOffsetY * scaleFactor + barPadding);
        }

        // 1. Draw Gauge Bar (Draw FIRST so text is on top)
        if (config.showBar) {
            float gaugeSpeed = (displaySpeed > maxSpeedGauge) ? maxSpeedGauge : ((displaySpeed < 0.0f) ? 0.0f : displaySpeed);
            float speedFraction = gaugeSpeed / maxSpeedGauge;

            ImVec2 barEnd = ImVec2(barStart.x + bWidth, barStart.y + bHeight);

            // Background
            ImVec2 p1 = barStart;
            ImVec2 p2 = ImVec2(barEnd.x, barStart.y);
            ImVec2 p3 = ImVec2(barEnd.x - bSkew, barEnd.y);
            ImVec2 p4 = ImVec2(barStart.x - bSkew, barEnd.y);

            draw_list->AddQuadFilled(p1, p2, p3, p4, IM_COL32(40, 40, 40, 200));

            // Active
            if (speedFraction > 0.0f) {
                float fillWidth = bWidth * speedFraction;
                ImVec2 f2 = ImVec2(barStart.x + fillWidth, barStart.y);
                ImVec2 f3 = ImVec2(barStart.x + fillWidth - bSkew, barEnd.y);
                ImU32 barColor = GetSpeedColor(speedFraction);
                draw_list->AddQuadFilled(p1, f2, f3, p4, barColor);
            }
        }

        // 2. Draw Speed Text (bottom-aligned with the unit)
        if (config.showSpeed) {
            float maxH = speedSize.y;
            if (config.showUnit && unitSize.y > maxH) maxH = unitSize.y;

            float yOffset = maxH - speedSize.y;
            ImVec2 finalPos = ImVec2(speedPos.x, speedPos.y + yOffset);

            // Shadow offset scales with the font so it stays visible at 65px+
            float shadowOff = (std::max)(2.0f, speedFontPx * 0.03f);

            // Shadow
            draw_list->AddText(currentFont, speedFontPx,
                               ImVec2(finalPos.x + shadowOff, finalPos.y + shadowOff),
                               IM_COL32(0, 0, 0, 200), speedText);
            // Main Text
            draw_list->AddText(currentFont, speedFontPx, finalPos,
                               IM_COL32(255, 255, 255, 255), speedText);
        }

        // 3. Draw Speed Unit (bottom-aligned with the speed value)
        if (config.showUnit) {
            float maxH = unitSize.y;
            if (config.showSpeed && speedSize.y > maxH) maxH = speedSize.y;

            float yOffset = maxH - unitSize.y;
            ImVec2 finalPos = ImVec2(unitPos.x, unitPos.y + yOffset);

            draw_list->AddText(currentFont, unitFontPx, finalPos,
                               IM_COL32(180, 180, 180, 255), unitStr.c_str());
        }
    }

    // Draw Config Window LAST so it appears on top
    if (shouldDrawConfig) {
        DrawConfigWindow();
    }

    ImGui::EndFrame();
    ImGui::Render();

    // Scale the draw data up to the render target (same technique as other
    // Split/Second overlay mods): scale vertices, set DisplaySize to the
    // surface size and scale the clip rects. Fonts stay baked at logical
    // size, so on a 2x surface they read "thinner" rather than blurry.
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && (fbScaleX != 1.0f || fbScaleY != 1.0f)) {
        for (auto& list : drawData->CmdLists) {
            for (auto& vert : list->VtxBuffer) {
                vert.pos.x *= fbScaleX;
                vert.pos.y *= fbScaleY;
            }
        }
        drawData->DisplaySize.x = surfaceW;
        drawData->DisplaySize.y = surfaceH;
        drawData->ScaleClipRects(ImVec2(fbScaleX, fbScaleY));
    }
    ImGui_ImplDX9_RenderDrawData(drawData);

    // Restore State
    if (g_stateBlock)
        g_stateBlock->Apply();
}

void Drawing::OnDeviceLost() {
    // The device is about to be reset: D3D9 requires all state blocks to be
    // released beforehand. Render() re-creates it on the next frame.
    if (g_stateBlock) {
        g_stateBlock->Release();
        g_stateBlock = nullptr;
    }
}

void Drawing::SetVisible(bool visible) {
    config.isVisible = visible;
}

bool Drawing::IsConfigOpen() {
    return config.showConfigWindow;
}

bool Drawing::IsSteamVersion() {
    // The addresses are compile-time selected via USE_STEAM_VERSION — this
    // only reports which build we are.
#ifdef USE_STEAM_VERSION
    return true;
#else
    return false;
#endif
}
