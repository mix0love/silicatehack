#ifndef STEALTH_MENU_HPP
#define STEALTH_MENU_HPP

#include <Windows.h>
#include <string>
#include <vector>

class StealthMenu {
   private:
    HWND m_hwnd = nullptr;
    HWND m_parentHwnd = nullptr;
    bool m_visible = false;
    int m_selectedIndex = 0;
    std::vector<std::string> m_macroNames;

    // Keybind editing state
    bool m_waitingForToggleKey = false;
    bool m_waitingForMenuKey = false;

    // Appearance
    static constexpr int WINDOW_WIDTH = 340;
    static constexpr int WINDOW_HEIGHT = 440;
    static constexpr int ITEM_HEIGHT = 28;
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int FOOTER_HEIGHT = 80;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
    void paint(HDC hdc);
    void handleKeyDown(WPARAM vk);

    // GDI drawing helpers
    void drawHeader(HDC hdc, RECT& rc);
    void drawMacroList(HDC hdc, RECT& rc);
    void drawFooter(HDC hdc, RECT& rc);
    void drawRoundRect(HDC hdc, int x, int y, int w, int h, int r,
                       COLORREF fill, COLORREF border);

    static std::string vkToString(int vk);

   public:
    static StealthMenu* get() {
        static StealthMenu instance;
        return &instance;
    }

    StealthMenu() = default;
    ~StealthMenu();

    StealthMenu(const StealthMenu&) = delete;
    void operator=(const StealthMenu&) = delete;

    void create(HWND parentHwnd);
    void toggle();
    void show();
    void hide();
    bool isVisible() const { return m_visible; }

    void selectNext();
    void selectPrevious();
    void applySelection();

    void refreshMacroList();

    const std::string& getSelectedMacro() const;
    int getSelectedIndex() const { return m_selectedIndex; }

    // Called from main wndproc to forward keys
    bool processKey(WPARAM vk, bool down);
};

#endif  // STEALTH_MENU_HPP
