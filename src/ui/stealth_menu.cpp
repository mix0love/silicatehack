#include "stealth_menu.hpp"

#include <Geode/Geode.hpp>
#include <filesystem>

#include "bot/bot.hpp"
#include "replay/system.hpp"
#include "settings/settings.hpp"

using namespace geode::prelude;

static const wchar_t* STEALTH_WND_CLASS = L"SilicateStealthOverlay";
static bool s_classRegistered = false;

StealthMenu::~StealthMenu() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

std::string StealthMenu::vkToString(int vk) {
    switch (vk) {
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_PAUSE: return "Pause";
        case VK_SCROLL: return "ScrollLock";
        case VK_NUMLOCK: return "NumLock";
        default: {
            if (vk >= 0x30 && vk <= 0x39) {
                return std::string(1, (char)vk);
            }
            if (vk >= 0x41 && vk <= 0x5A) {
                return std::string(1, (char)vk);
            }
            char buf[16];
            snprintf(buf, sizeof(buf), "0x%02X", vk);
            return buf;
        }
    }
}

void StealthMenu::create(HWND parentHwnd) {
    m_parentHwnd = parentHwnd;

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = StealthMenu::WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = STEALTH_WND_CLASS;
        wc.hbrBackground = nullptr;
        wc.style = CS_OWNDC;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    // Position: top-right corner of screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int posX = screenW - WINDOW_WIDTH - 20;
    int posY = 20;

    // WS_EX_TOOLWINDOW: hidden from taskbar + OBS window capture list
    // WS_EX_TOPMOST: always on top
    // WS_EX_NOACTIVATE: doesn't steal focus from GD
    // WS_EX_LAYERED: for transparency
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        STEALTH_WND_CLASS, L"Silicate Stealth",
        WS_POPUP,
        posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr,  // no parent — independent top-level window
        nullptr, GetModuleHandle(nullptr), nullptr);

    if (m_hwnd) {
        // Semi-transparent window
        SetLayeredWindowAttributes(m_hwnd, 0, 230, LWA_ALPHA);
        geode::log::info("Stealth menu window created");
    } else {
        geode::log::error("Failed to create stealth menu window: {}",
                          GetLastError());
    }
}

void StealthMenu::toggle() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

void StealthMenu::show() {
    if (!m_hwnd) return;

    refreshMacroList();
    m_visible = true;
    m_waitingForToggleKey = false;
    m_waitingForMenuKey = false;

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void StealthMenu::hide() {
    if (!m_hwnd) return;

    m_visible = false;
    m_waitingForToggleKey = false;
    m_waitingForMenuKey = false;
    ShowWindow(m_hwnd, SW_HIDE);
}

void StealthMenu::refreshMacroList() {
    m_macroNames.clear();

    auto replayDir = Mod::get()->getPersistentDir() / "replays";
    if (!std::filesystem::exists(replayDir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(replayDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".slc") {
            m_macroNames.push_back(entry.path().stem().string());
        }
    }

    std::sort(m_macroNames.begin(), m_macroNames.end());

    // Sync selected index with current replay name
    auto& currentName = Bot::get()->replaySystem().m_replayName;
    for (int i = 0; i < (int)m_macroNames.size(); i++) {
        if (m_macroNames[i] == currentName) {
            m_selectedIndex = i;
            return;
        }
    }

    if (m_selectedIndex >= (int)m_macroNames.size()) {
        m_selectedIndex = m_macroNames.empty() ? 0 : (int)m_macroNames.size() - 1;
    }
}

void StealthMenu::selectNext() {
    if (m_macroNames.empty()) return;
    m_selectedIndex = (m_selectedIndex + 1) % (int)m_macroNames.size();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void StealthMenu::selectPrevious() {
    if (m_macroNames.empty()) return;
    m_selectedIndex--;
    if (m_selectedIndex < 0) m_selectedIndex = (int)m_macroNames.size() - 1;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void StealthMenu::applySelection() {
    if (m_macroNames.empty()) return;
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_macroNames.size()) return;

    auto& rs = Bot::get()->replaySystem();
    rs.m_replayName = m_macroNames[m_selectedIndex];

    auto path = rs.getCurrentPath();
    rs.load(path);

    geode::log::info("Stealth menu: loaded macro '{}'", rs.m_replayName);
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

const std::string& StealthMenu::getSelectedMacro() const {
    static std::string empty;
    if (m_macroNames.empty()) return empty;
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_macroNames.size())
        return empty;
    return m_macroNames[m_selectedIndex];
}

bool StealthMenu::processKey(WPARAM vk, bool down) {
    if (!m_visible || !down) return false;

    // Keybind rebinding mode
    if (m_waitingForToggleKey) {
        SLSettings::get()->macroToggleKey = (int)vk;
        m_waitingForToggleKey = false;
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
        geode::log::info("Macro toggle key set to: {}", vkToString((int)vk));
        return true;
    }

    if (m_waitingForMenuKey) {
        SLSettings::get()->stealthMenuKey = (int)vk;
        m_waitingForMenuKey = false;
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
        geode::log::info("Stealth menu key set to: {}", vkToString((int)vk));
        return true;
    }

    handleKeyDown(vk);
    return true;
}

void StealthMenu::handleKeyDown(WPARAM vk) {
    switch (vk) {
        case VK_PRIOR:  // PageUp
            selectPrevious();
            break;
        case VK_NEXT:  // PageDown
            selectNext();
            break;
        case VK_RETURN:  // Enter — load selected macro
            applySelection();
            break;
        case VK_ESCAPE:
            hide();
            break;
        case VK_F1:  // F1 — rebind macro toggle key
            m_waitingForToggleKey = true;
            m_waitingForMenuKey = false;
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
            break;
        case VK_F2:  // F2 — rebind stealth menu key
            m_waitingForMenuKey = true;
            m_waitingForToggleKey = false;
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
            break;
        default:
            break;
    }
}

// ---- Win32 Window Proc ----

LRESULT CALLBACK StealthMenu::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            StealthMenu::get()->paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // prevent flicker
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // don't steal focus
        case WM_NCHITTEST:
            return HTTRANSPARENT;  // mouse passes through
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---- GDI Painting ----

void StealthMenu::drawRoundRect(HDC hdc, int x, int y, int w, int h, int r,
                                 COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    RoundRect(hdc, x, y, x + w, y + h, r, r);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void StealthMenu::paint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap =
        CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Dark background
    HBRUSH bgBrush = CreateSolidBrush(RGB(18, 18, 24));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(60, 60, 80));
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    HBRUSH hollow = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    HBRUSH oldBr = (HBRUSH)SelectObject(memDC, hollow);
    RoundRect(memDC, 0, 0, clientRect.right, clientRect.bottom, 12, 12);
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBr);
    DeleteObject(borderPen);

    SetBkMode(memDC, TRANSPARENT);

    RECT rc = clientRect;
    rc.left += 12;
    rc.right -= 12;
    rc.top += 8;

    drawHeader(memDC, rc);
    drawMacroList(memDC, rc);
    drawFooter(memDC, rc);

    // Blit
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0,
           SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

void StealthMenu::drawHeader(HDC hdc, RECT& rc) {
    // Title
    HFONT titleFont =
        CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);

    SetTextColor(hdc, RGB(200, 200, 220));
    RECT titleRect = rc;
    titleRect.bottom = titleRect.top + 26;
    DrawTextW(hdc, L"SILICATE STEALTH", -1, &titleRect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);

    // Subtitle with macro status
    HFONT subFont =
        CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, subFont);

    auto& rs = Bot::get()->replaySystem();
    bool isPlaying = Bot::get()->isPlaying();
    std::string status = isPlaying ? "PLAYING" : "RECORDING";
    std::string macroInfo = "Mode: " + status;
    if (!rs.m_replayName.empty()) {
        macroInfo += "  |  Current: " + rs.m_replayName;
    }

    SetTextColor(hdc, isPlaying ? RGB(100, 220, 130) : RGB(220, 130, 100));
    RECT subRect = rc;
    subRect.top = titleRect.bottom + 2;
    subRect.bottom = subRect.top + 16;

    std::wstring wMacroInfo(macroInfo.begin(), macroInfo.end());
    DrawTextW(hdc, wMacroInfo.c_str(), -1, &subRect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(subFont);

    // Divider
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 70));
    HPEN oldPen = (HPEN)SelectObject(hdc, divPen);
    int divY = subRect.bottom + 4;
    MoveToEx(hdc, rc.left, divY, nullptr);
    LineTo(hdc, rc.right, divY);
    SelectObject(hdc, oldPen);
    DeleteObject(divPen);

    rc.top = divY + 6;
}

void StealthMenu::drawMacroList(HDC hdc, RECT& rc) {
    HFONT listFont =
        CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, listFont);

    int listAreaHeight = rc.bottom - rc.top - FOOTER_HEIGHT;
    int maxVisible = listAreaHeight / ITEM_HEIGHT;

    if (m_macroNames.empty()) {
        SetTextColor(hdc, RGB(120, 120, 140));
        RECT emptyRect = rc;
        emptyRect.bottom = emptyRect.top + ITEM_HEIGHT;
        DrawTextW(hdc, L"No macros found in replays/", -1, &emptyRect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        rc.top += ITEM_HEIGHT + 4;
        SelectObject(hdc, oldFont);
        DeleteObject(listFont);
        return;
    }

    // Scroll offset
    int scrollStart = 0;
    if (m_selectedIndex >= maxVisible) {
        scrollStart = m_selectedIndex - maxVisible + 1;
    }

    auto& currentName = Bot::get()->replaySystem().m_replayName;

    for (int i = scrollStart;
         i < (int)m_macroNames.size() && (i - scrollStart) < maxVisible; i++) {
        int y = rc.top + (i - scrollStart) * ITEM_HEIGHT;

        bool isSelected = (i == m_selectedIndex);
        bool isCurrent = (m_macroNames[i] == currentName);

        // Background for selected item
        if (isSelected) {
            drawRoundRect(hdc, rc.left - 4, y, rc.right - rc.left + 8,
                          ITEM_HEIGHT - 2, 6, RGB(45, 45, 70),
                          RGB(80, 80, 120));
        }

        // Index number
        SetTextColor(hdc, RGB(80, 80, 100));
        RECT idxRect = {rc.left, y, rc.left + 28, y + ITEM_HEIGHT};
        std::string idxStr = std::to_string(i + 1) + ".";
        std::wstring wIdx(idxStr.begin(), idxStr.end());
        DrawTextW(hdc, wIdx.c_str(), -1, &idxRect,
                  DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        // Macro name
        if (isCurrent && isSelected) {
            SetTextColor(hdc, RGB(130, 220, 255));
        } else if (isSelected) {
            SetTextColor(hdc, RGB(220, 220, 240));
        } else if (isCurrent) {
            SetTextColor(hdc, RGB(100, 180, 220));
        } else {
            SetTextColor(hdc, RGB(160, 160, 180));
        }

        RECT nameRect = {rc.left + 34, y, rc.right - 8, y + ITEM_HEIGHT};
        std::wstring wName(m_macroNames[i].begin(), m_macroNames[i].end());
        DrawTextW(hdc, wName.c_str(), -1, &nameRect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        // Active indicator
        if (isCurrent) {
            HBRUSH dot = CreateSolidBrush(RGB(100, 220, 130));
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, dot);
            HPEN noPen = CreatePen(PS_NULL, 0, 0);
            HPEN oldPn = (HPEN)SelectObject(hdc, noPen);
            Ellipse(hdc, rc.right - 10, y + 9, rc.right - 2, y + 17);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPn);
            DeleteObject(dot);
            DeleteObject(noPen);
        }
    }

    rc.top += maxVisible * ITEM_HEIGHT + 4;
    SelectObject(hdc, oldFont);
    DeleteObject(listFont);
}

void StealthMenu::drawFooter(HDC hdc, RECT& rc) {
    // Divider
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 70));
    HPEN oldPen = (HPEN)SelectObject(hdc, divPen);
    MoveToEx(hdc, rc.left, rc.bottom - FOOTER_HEIGHT, nullptr);
    LineTo(hdc, rc.right, rc.bottom - FOOTER_HEIGHT);
    SelectObject(hdc, oldPen);
    DeleteObject(divPen);

    HFONT footFont =
        CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, footFont);

    int y = rc.bottom - FOOTER_HEIGHT + 6;
    int lineH = 16;

    auto settings = SLSettings::get();

    // Keybind info
    SetTextColor(hdc, RGB(140, 140, 160));

    // Line 1: Toggle keybind
    std::string toggleStr = "Macro Toggle: [" +
                             vkToString(settings->macroToggleKey) + "]";
    if (m_waitingForToggleKey) {
        toggleStr = "Macro Toggle: [Press any key...]";
        SetTextColor(hdc, RGB(255, 200, 100));
    }
    RECT line1 = {rc.left, y, rc.right, y + lineH};
    std::wstring wToggle(toggleStr.begin(), toggleStr.end());
    DrawTextW(hdc, wToggle.c_str(), -1, &line1,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Line 2: Menu keybind
    SetTextColor(hdc, m_waitingForMenuKey ? RGB(255, 200, 100)
                                          : RGB(140, 140, 160));
    std::string menuStr =
        "Stealth Menu: [" + vkToString(settings->stealthMenuKey) + "]";
    if (m_waitingForMenuKey) {
        menuStr = "Stealth Menu: [Press any key...]";
    }
    RECT line2 = {rc.left, y + lineH, rc.right, y + lineH * 2};
    std::wstring wMenu(menuStr.begin(), menuStr.end());
    DrawTextW(hdc, wMenu.c_str(), -1, &line2,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Line 3: Controls help
    SetTextColor(hdc, RGB(90, 90, 110));
    RECT line3 = {rc.left, y + lineH * 2, rc.right, y + lineH * 3};
    DrawTextW(hdc, L"PgUp/PgDn: Select  Enter: Load  Esc: Close",
              -1, &line3, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Line 4: Rebind help
    RECT line4 = {rc.left, y + lineH * 3, rc.right, y + lineH * 4};
    DrawTextW(hdc, L"F1: Rebind Toggle  F2: Rebind Menu Key",
              -1, &line4, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(footFont);
}
