#include "drawing.h"
#include "embedded_fonts.h"
#include "logger.h"
#include <string>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
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

    std::string GetFontPath(const std::string& fontName) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            std::string p(path);
            size_t pos = p.find_last_of("\\/");
            return p.substr(0, pos) + "\\fonts\\" + fontName;
        }
        return "fonts\\" + fontName;
    }
}

// ---------------------------------------------------------
// GAME ADDRESSES 
// ---------------------------------------------------------
// --- VERSION SELECTION ---
// Uncomment the line below to build for STEAM version. Keep commented for RETAIL/CRACKED.
//#define USE_STEAM_VERSION

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
    // Only process inputs if we are in-game
    bool isPaused = false;
    std::uint32_t in_game_ui = 0;
    
    // Check game state first
    if (IN_GAME_UI_PTR != 0 && !IsBadReadPtr((void*)IN_GAME_UI_PTR, sizeof(std::uint32_t))) {
        in_game_ui = *reinterpret_cast<std::uint32_t*>(IN_GAME_UI_PTR);
        if (in_game_ui != 0) {
            isPaused = CallIsGamePaused(IS_PAUSED_FUNC_ADDR, in_game_ui);
        }
    }

    // Allow inputs only if in-game and not paused
    if (in_game_ui != 0 && !isPaused) {
        if (msg == WM_KEYDOWN) {
            Logger::Log("Key pressed: " + std::to_string(wParam));
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
        }
    } else {
        // Force close config if not in race
        if (config.showConfigWindow) config.showConfigWindow = false;
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

    // Use standard size, no manual scaling needed as DisplaySize matches Window Size
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
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

    // Save State Block to prevent graphical glitches
    IDirect3DStateBlock9* stateBlock = nullptr;
    if (SUCCEEDED(pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock)) && stateBlock) {
        if (FAILED(stateBlock->Capture())) {
            stateBlock->Release();
            stateBlock = nullptr;
        }
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // --- RESOLUTION SETUP ---
    ImGuiIO& io = ImGui::GetIO();
    
    // 1. Get Actual Window Size (Client Area from Windows)
    float windowW = 0.0f, windowH = 0.0f;
    D3DDEVICE_CREATION_PARAMETERS cp;
    if (SUCCEEDED(pDevice->GetCreationParameters(&cp)) && cp.hFocusWindow) {
        RECT rect;
        if (GetClientRect(cp.hFocusWindow, &rect)) {
            windowW = (float)(rect.right - rect.left);
            windowH = (float)(rect.bottom - rect.top);
        }
    }

    // 2. Get BackBuffer Size (Internal Game Resolution)
    float backbufferW = windowW;
    float backbufferH = windowH;
    
    IDirect3DSurface9* rt = nullptr;
    if (SUCCEEDED(pDevice->GetRenderTarget(0, &rt)) && rt) {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(rt->GetDesc(&desc))) {
            backbufferW = (float)desc.Width;
            backbufferH = (float)desc.Height;
        }
        rt->Release();
    }
    
    // Fallback: If Backbuffer read failed, use Viewport
    if (backbufferW <= 0) {
        D3DVIEWPORT9 vp;
        pDevice->GetViewport(&vp);
        backbufferW = (float)vp.Width;
        backbufferH = (float)vp.Height;
    }

    // Fallback: If Window size failed, assume it matches Backbuffer (Windowed Fullscreen)
    if (windowW <= 0) {
        windowW = backbufferW;
        windowH = backbufferH;
    }

    // 3. Setup ImGui Resolution
    // Use Window Size for Logical Layout and FramebufferScale for Rendering
    io.DisplaySize = ImVec2(windowW, windowH);
    
    if (windowW > 0 && windowH > 0) {
        io.DisplayFramebufferScale = ImVec2(backbufferW / windowW, backbufferH / windowH);
    } else {
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }

    // Style Scaling
    ImGuiStyle& style = ImGui::GetStyle();
    // Reset FontGlobalScale to 1.0f (Standard)
    io.FontGlobalScale = 1.0f; 
    
    // Reset Style Scaling to 1.0f (Standard)
    style.ScaleAllSizes(1.0f);
    
    // Ensure FontGlobalScale is 1.0f
    io.FontGlobalScale = 1.0f; 

    // ---------------------------------------------------------

    ImGui::NewFrame();

    // 1. Read Game Data
    float currentSpeed = 0.0f;
    std::uint32_t in_game_ui = 0;
    bool isPaused = false;

    static bool speedAddrFound = false;
    static bool uiAddrFound = false;

    try {
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
    } catch (...) {}

    // 2. Handle Inputs
    // Input handling is now done in OnWndProc to avoid Alt-Tab issues.
    
    // Safety: Force close config window if not in race or paused
    if (in_game_ui == 0 || isPaused) {
        config.showConfigWindow = false;
    }

    // Auto-Show Logic (Race Start)
    bool currentlyInRace = (in_game_ui != 0);
    if (currentlyInRace && !wasInRace) {
        config.isVisible = true;
    }
    wasInRace = currentlyInRace;

    // 3. Render
    // Separate HUD visibility from Config Window visibility
    bool shouldDrawHUD = config.isVisible && in_game_ui != 0 && !isPaused;
    bool shouldDrawConfig = config.showConfigWindow;

    // Mouse Cursor Logic
    if (shouldDrawConfig) {
        io.MouseDrawCursor = true;
    } else {
        io.MouseDrawCursor = false;
    }

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

        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - config.globalPosX * scaleFactor, io.DisplaySize.y - config.globalPosY * scaleFactor),
            ImGuiCond_Always,
            ImVec2(1.0f, 1.0f)
        );

        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin("HUD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground)) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();

            // Reserve space to prevent clipping (Important for AlwaysAutoResize)
            // Estimate max size based on configuration
            float estimatedWidth = 400.0f * scaleFactor;
            float estimatedHeight = 200.0f * scaleFactor;
            
            // Adjust based on config values to be safe
            if (config.barWidth * scaleFactor > estimatedWidth) estimatedWidth = config.barWidth * scaleFactor + 50.0f;
            
            // Use Dummy to force window size
            ImGui::Dummy(ImVec2(estimatedWidth, estimatedHeight));

            // Select Font based on config
            ImFont* currentFont = font_default;
            if (config.fontStyle == 1 && font_dash) currentFont = font_dash;
            if (config.fontStyle == 2 && font_sprint) currentFont = font_sprint;

            ImGui::PushFont(currentFont);

            // Prepare strings and sizes for layout calculation
            char speedText[32];
            sprintf_s(speedText, sizeof(speedText), "%d", (int)displaySpeed);
            
            ImVec2 speedSize(0,0);
            if (config.showSpeed) {
                float fontScale = 5.0f * config.globalScale * config.speedScale;
                ImGui::SetWindowFontScale(fontScale);
                speedSize = ImGui::CalcTextSize(speedText);
            }

            ImVec2 unitSize(0,0);
            if (config.showUnit) {
                float fontScale = 2.0f * config.globalScale * config.unitScale;
                ImGui::SetWindowFontScale(fontScale);
                unitSize = ImGui::CalcTextSize(unitStr.c_str());
            }

            // Calculate Positions
            ImVec2 speedPos;
            ImVec2 unitPos;
            ImVec2 barStart;

            // Bar dimensions
            float bWidth = config.barWidth * scaleFactor;
            float bHeight = config.barHeight * scaleFactor;
            float bSkew = config.barSkew * scaleFactor;
            
            // Bar position (Default/Legacy)
            barStart = ImVec2(p.x + config.barOffsetX * scaleFactor, p.y + config.barOffsetY * scaleFactor);
            
            if (config.smartLayout) {
                // Smart Layout Logic:
                // 1. Establish a stable visual center relative to the Window's Right Edge.
                //    (Because the window is anchored Bottom-Right, 'p' moves left when window expands.
                //     We want the text to stay put even if the bar grows/moves.)
                float fixedCenterOffset = 200.0f * scaleFactor; // Half of base width (400)
                float visualCenterX = p.x + estimatedWidth - fixedCenterOffset;

                // 2. Calculate Total Text Width
                float gap = config.smartGap * scaleFactor;
                float totalTextWidth = 0.0f;
                if (config.showSpeed) totalTextWidth += speedSize.x;
                if (config.showSpeed && config.showUnit) totalTextWidth += gap;
                if (config.showUnit) totalTextWidth += unitSize.x;
                
                // 3. Center Text Block around visualCenterX
                float currentX = visualCenterX - (totalTextWidth / 2.0f);
                
                // Set Speed Pos
                if (config.showSpeed) {
                    speedPos = ImVec2(currentX, p.y + config.speedOffsetY * scaleFactor);
                    currentX += speedSize.x + gap;
                }
                
                // Set Unit Pos
                if (config.showUnit) {
                    // Decoupled from Speed Y (Absolute Y offset relative to anchor)
                    unitPos = ImVec2(currentX, p.y + config.unitOffsetY * scaleFactor);
                }
                
                // 4. Center Bar around visualCenterX + barOffsetX
                //    This decouples Bar movement from Text movement.
                //    Adjust for Skew to center visually (Center of Mass)
                float bSkew = config.barSkew * scaleFactor;
                float effectiveBarCenterOffset = (bWidth / 2.0f) - (bSkew / 2.0f);
                float barCenterX = visualCenterX + config.barOffsetX * scaleFactor;
                
                barStart.x = barCenterX - effectiveBarCenterOffset;

                // 5. Adjust Bar Y (Independent of Text Position)
                //    We base it on the Text Height (so it defaults to below text), 
                //    but NOT on the Text Offset (so moving text doesn't move bar).
                float maxTextHeight = 0.0f;
                if (config.showSpeed) maxTextHeight = (std::max)(maxTextHeight, speedSize.y);
                if (config.showUnit) maxTextHeight = (std::max)(maxTextHeight, unitSize.y);
                
                // Add padding to prevent text from overlapping bar (e.g. 5px scaled)
                float barPadding = 5.0f * scaleFactor; 
                barStart.y = p.y + maxTextHeight + config.barOffsetY * scaleFactor + barPadding;
            } else {
                // Legacy / Manual Layout
                speedPos = ImVec2(p.x + config.speedOffsetX * scaleFactor, p.y + config.speedOffsetY * scaleFactor);
                unitPos = ImVec2(p.x + config.unitOffsetX * scaleFactor, p.y + config.unitOffsetY * scaleFactor);
                
                float currentTextBottom = p.y;
                if (config.showSpeed) currentTextBottom += speedSize.y;
                
                // Add padding here too
                float barPadding = 5.0f * scaleFactor;
                barStart.y = currentTextBottom + config.barOffsetY * scaleFactor + barPadding;
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

            // 2. Draw Speed Text
            if (config.showSpeed) {
                float fontScale = 5.0f * config.globalScale * config.speedScale;
                ImGui::SetWindowFontScale(fontScale);
                
                // Align Bottom with Unit if Smart Layout is ON (or just always align baselines?)
                // Let's align bottoms of Speed and Unit based on their height
                float maxH = speedSize.y;
                if (config.showUnit && unitSize.y > maxH) maxH = unitSize.y;
                
                // Apply Offset for Bottom Alignment
                float yOffset = maxH - speedSize.y;
                ImVec2 finalPos = ImVec2(speedPos.x, speedPos.y + yOffset);

                // Shadow
                draw_list->AddText(ImVec2(finalPos.x + 2, finalPos.y + 2), IM_COL32(0,0,0,200), speedText);
                // Main Text
                draw_list->AddText(finalPos, IM_COL32(255,255,255,255), speedText);
            }

            // 3. Draw Speed Unit
            if (config.showUnit) {
                float fontScale = 2.0f * config.globalScale * config.unitScale;
                ImGui::SetWindowFontScale(fontScale);
                
                // Align Bottom
                float maxH = unitSize.y;
                if (config.showSpeed && speedSize.y > maxH) maxH = speedSize.y;
                
                float yOffset = maxH - unitSize.y;
                ImVec2 finalPos = ImVec2(unitPos.x, unitPos.y + yOffset);

                draw_list->AddText(finalPos, IM_COL32(180,180,180,255), unitStr.c_str());
            }

            ImGui::PopFont(); // Restore default font for other windows (like Config)
            ImGui::End();
        }
        ImGui::PopStyleVar(2);
    }

    // Draw Config Window LAST so it appears on top
    if (shouldDrawConfig) {
        DrawConfigWindow();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // Restore State
    if (stateBlock) {
        stateBlock->Apply();
        stateBlock->Release();
    }
}

void Drawing::SetVisible(bool visible) {
    config.isVisible = visible;
}

bool Drawing::IsConfigOpen() {
    return config.showConfigWindow;
}

bool Drawing::IsSteamVersion() {
    return IN_GAME_UI_PTR == 0xD5A170;
}
