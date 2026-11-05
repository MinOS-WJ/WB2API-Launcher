#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <commdlg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ws2_32.lib")

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // ---------- 配色（参考软件界面设计.svg） ----------
    constexpr COLORREF kBg       = RGB(0xE9, 0xEB, 0xEE);
    constexpr COLORREF kHeader   = RGB(0xF8, 0xF9, 0xFB);
    constexpr COLORREF kText     = RGB(0x1A, 0x20, 0x24);
    constexpr COLORREF kSubText  = RGB(0x6B, 0x74, 0x7C);
    constexpr COLORREF kBorder   = RGB(0xDD, 0xE1, 0xE6);
    constexpr COLORREF kDisabled = RGB(0xE2, 0xE5, 0xE9);
    constexpr COLORREF kDisText  = RGB(0x9A, 0xA2, 0xAB);

    constexpr COLORREF kLinkBg   = RGB(0xFF, 0xE1, 0xE1);
    constexpr COLORREF kLinkBgHv = RGB(0xFF, 0xCF, 0xCF);
    constexpr COLORREF kLinkText = RGB(0xC0, 0x39, 0x2B);

    constexpr wchar_t kWindowClassName[] = L"WB2API.Launcher.Window";
    constexpr wchar_t kProjectUrl[] = L"https://github.com/ardeyouxipianyi/workbuddy2api-hub";
    constexpr wchar_t kDashboardUrl[] = L"http://localhost:8788";
    constexpr WORD kServicePort = 8788;

    constexpr int kButtonImport  = 1004;
    constexpr int kButtonGoto    = 1005;
    constexpr int kButtonOpenDir = 1006;
    constexpr int kStatusControl = 2001;
    constexpr int kProgressBar   = 2002;
    constexpr int kPathControl   = 2003;

    constexpr UINT WM_IMPORT_DONE     = WM_APP + 1;
    constexpr UINT WM_SERVICE_WATCHED = WM_APP + 2;

    struct Action
    {
        const wchar_t* label;
        const wchar_t* sub;
        const wchar_t* fileName;
        bool openDashboard;
        COLORREF base;
        bool darkText;
        int id;
        HWND button = nullptr;
    };

    HINSTANCE g_instance = nullptr;
    HWND g_window = nullptr;
    HWND g_status = nullptr;
    HWND g_progress = nullptr;
    HICON g_appIcon = nullptr;
    HICON g_appIcon48 = nullptr;
    int g_dpi = 96;

    std::wstring g_exeDirectory;
    std::wstring g_wbProxyDirectory;
    std::atomic<bool> g_busy{false};

    HFONT g_fontTitle = nullptr;
    HFONT g_fontBtn = nullptr;
    HFONT g_fontBtnSub = nullptr;
    HFONT g_fontSmall = nullptr;
    HBRUSH g_brBg = nullptr;
    HBRUSH g_brHeader = nullptr;

    std::vector<Action> g_actions = {
        {L"本地启动",        L"仅本机访问 · 推荐",    L"start-wb-proxy.bat",             true,  RGB(0xF5,0x37,0x3C), false, 1101},
        {L"局域网启动",      L"允许局域网设备访问",   L"start-wb-proxy-lan.bat",         true,  RGB(0x18,0x9A,0x4A), false, 1102},
        {L"本地·可见窗口",   L"前台控制台看日志",     L"start-wb-proxy-visible.bat",     true,  RGB(0xFF,0xC4,0x00), true,  1103},
        {L"局域网·可见窗口", L"共享模式 + 前台日志",  L"start-wb-proxy-lan-visible.bat", true,  RGB(0x0F,0xA6,0xCE), false, 1104},
        {L"重启服务",        L"停止后重新启动代理",   L"restart-wb-proxy.bat",           true,  RGB(0x2E,0x5B,0xFF), false, 1105},
        {L"防火墙放行",      L"允许局域网端口通过",   L"allow-firewall.bat",             false, RGB(0xC8,0x40,0xD8), false, 1106},
    };

    int Scale(int v) { return MulDiv(v, g_dpi, 96); }

    void InitDpi(HWND hwnd)
    {
        HDC dc = GetDC(hwnd);
        g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(hwnd, dc);
    }

    std::wstring GetLastErrorText(DWORD errorCode)
    {
        wchar_t* buffer = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD length = FormatMessageW(flags, nullptr, errorCode, 0,
            reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
        if (length == 0 || buffer == nullptr) return L"未知错误";
        std::wstring result(buffer, length);
        LocalFree(buffer);
        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n'))
            result.pop_back();
        return result;
    }

    std::wstring GetExecutableDirectory()
    {
        DWORD bufferSize = MAX_PATH;
        for (;;)
        {
            std::vector<wchar_t> buffer(bufferSize);
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), bufferSize);
            if (length == 0) return {};
            if (length < bufferSize - 1)
            {
                std::wstring fullPath(buffer.data(), length);
                const std::size_t sep = fullPath.find_last_of(L"\\/");
                return sep == std::wstring::npos ? std::wstring{} : fullPath.substr(0, sep);
            }
            if (bufferSize >= 32768) return {};
            bufferSize *= 2;
        }
    }

    bool IsRegularFile(const std::wstring& path)
    {
        const DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool ScriptExists(const Action& a)
    {
        return IsRegularFile(g_wbProxyDirectory + L"\\" + a.fileName);
    }

    bool CoreScriptReady()
    {
        return IsRegularFile(g_wbProxyDirectory + L"\\start-wb-proxy.bat");
    }

    void SetStatus(const std::wstring& text)
    {
        if (g_status) SetWindowTextW(g_status, text.c_str());
    }

    void FillRoundRect(HDC dc, const RECT& rc, int radius, COLORREF color)
    {
        HBRUSH br = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        HBRUSH oldBr = (HBRUSH)SelectObject(dc, br);
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        SelectObject(dc, oldBr); SelectObject(dc, oldPen);
        DeleteObject(br); DeleteObject(pen);
    }

    COLORREF Tint(COLORREF c, int amount)
    {
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        if (amount >= 0) {
            r += (255 - r) * amount / 255;
            g += (255 - g) * amount / 255;
            b += (255 - b) * amount / 255;
        } else {
            r = r * (255 + amount) / 255;
            g = g * (255 + amount) / 255;
            b = b * (255 + amount) / 255;
        }
        return RGB(r, g, b);
    }

    HFONT MakeFont(int pt, int weight)
    {
        int h = -MulDiv(pt, g_dpi, 72);
        HFONT f = CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
        if (!f) f = CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        return f;
    }

    void CreateFontsAndBrushes()
    {
        g_fontTitle = MakeFont(15, FW_BOLD);
        g_fontBtn = MakeFont(13, FW_SEMIBOLD);
        g_fontBtnSub = MakeFont(9, FW_NORMAL);
        g_fontSmall = MakeFont(9, FW_NORMAL);
        g_brBg = CreateSolidBrush(kBg);
        g_brHeader = CreateSolidBrush(kHeader);
    }

    void DestroyFontsAndBrushes()
    {
        for (HFONT f : {g_fontTitle, g_fontBtn, g_fontBtnSub, g_fontSmall})
            if (f) DeleteObject(f);
        if (g_brBg) DeleteObject(g_brBg);
        if (g_brHeader) DeleteObject(g_brHeader);
        g_fontTitle = g_fontBtn = g_fontBtnSub = g_fontSmall = nullptr;
        g_brBg = g_brHeader = nullptr;
    }

    void RefreshAllButtons()
    {
        for (Action& a : g_actions)
        {
            EnableWindow(a.button, (ScriptExists(a) && !g_busy) ? TRUE : FALSE);
            InvalidateRect(a.button, nullptr, TRUE);
        }
        for (int id : {kButtonImport, kButtonGoto, kButtonOpenDir})
        {
            HWND b = GetDlgItem(g_window, id);
            if (b) { EnableWindow(b, g_busy ? FALSE : TRUE); InvalidateRect(b, nullptr, TRUE); }
        }
        if (g_progress) ShowWindow(g_progress, g_busy ? SW_SHOW : SW_HIDE);
    }

    bool LaunchBatchFile(const Action& action)
    {
        const std::wstring scriptPath = g_wbProxyDirectory + L"\\" + action.fileName;
        if (!IsRegularFile(scriptPath))
        {
            const std::wstring msg = L"未找到脚本：\n" + scriptPath;
            SetStatus(msg);
            MessageBoxW(g_window, msg.c_str(), L"无法启动", MB_OK | MB_ICONSTOP);
            return false;
        }
        wchar_t sysDir[MAX_PATH] = {};
        const UINT sysLen = GetSystemDirectoryW(sysDir, static_cast<UINT>(_countof(sysDir)));
        if (sysLen == 0 || sysLen >= _countof(sysDir))
        {
            SetStatus(L"无法定位 cmd.exe。");
            MessageBoxW(g_window, L"无法定位 cmd.exe。", L"无法启动", MB_OK | MB_ICONSTOP);
            return false;
        }
        const std::wstring cmdExe = std::wstring(sysDir, sysLen) + L"\\cmd.exe";
        const std::wstring commandLine = L"cmd.exe /d /c call \"" + scriptPath + L"\"";
        std::vector<wchar_t> mcmd(commandLine.begin(), commandLine.end());
        mcmd.push_back(L'\0');
        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(cmdExe.c_str(), mcmd.data(), nullptr, nullptr,
            FALSE, CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, nullptr,
            g_wbProxyDirectory.c_str(), &si, &pi);
        if (!ok)
        {
            const DWORD err = GetLastError();
            const std::wstring msg = L"无法启动：\n" + scriptPath + L"\n\n错误 " +
                std::to_wstring(err) + L": " + GetLastErrorText(err);
            SetStatus(msg);
            MessageBoxW(g_window, msg.c_str(), L"无法启动", MB_OK | MB_ICONSTOP);
            return false;
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return true;
    }

    bool IsServicePortOpen()
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return false;
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(kServicePort);
        inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
        const int r = connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
        closesocket(s);
        return r == 0;
    }

    void ServiceWatchProc()
    {
        bool ok = false;
        for (int i = 0; i < 40; ++i)
        {
            Sleep(500);
            if (IsServicePortOpen()) { ok = true; break; }
        }
        PostMessageW(g_window, WM_SERVICE_WATCHED, ok ? 1 : 0, 0);
    }

    void StartServiceWatch()
    {
        if (g_busy.exchange(true)) return;
        RefreshAllButtons();
        SetStatus(L"服务启动中，正在等待端口 8788 就绪…");
        if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 40);
        std::thread(ServiceWatchProc).detach();
    }

    bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir,
                    std::wstring& errorOut)
    {
        std::wstring cmd = L"tar.exe -xf \"" + zipPath + L"\" -C \"" + destDir + L"\"";
        std::vector<wchar_t> mcmd(cmd.begin(), cmd.end()); mcmd.push_back(L'\0');
        STARTUPINFOW si{}; si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, mcmd.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi))
        { errorOut = L"无法运行 tar.exe：" + GetLastErrorText(GetLastError()); return false; }
        WaitForSingleObject(pi.hProcess, 300000);
        DWORD exitCode = 1; GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        if (exitCode != 0)
        { errorOut = L"解压失败（tar 退出码 " + std::to_wstring(exitCode) +
                     L"），请确认选择的是有效的 ZIP 压缩包。"; return false; }
        return true;
    }

    void ImportThreadProc(std::wstring zipPath)
    {
        std::wstring err; bool ok = false;
        if (ExtractZip(zipPath, g_exeDirectory, err))
        {
            ok = CoreScriptReady();
            if (!ok)
                err = L"解压完成，但未检测到 wb-proxy 启动脚本。\n"
                      L"请确认选择的是 workbuddy2api-hub 官方压缩包。";
        }
        auto* heapErr = new std::wstring(err);
        PostMessageW(g_window, WM_IMPORT_DONE, ok ? 1 : 0,
            reinterpret_cast<LPARAM>(heapErr));
    }

    void StartImport()
    {
        if (g_busy.load()) return;
        wchar_t filePath[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_window;
        ofn.lpstrFilter = L"workbuddy2api-hub 压缩包 (*.zip)\0*.zip\0所有文件 (*.*)\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"选择下载好的 workbuddy2api-hub 压缩包";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn)) return;
        g_busy = true;
        RefreshAllButtons();
        SetStatus(L"正在解压导入…");
        if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 40);
        std::thread(ImportThreadProc, std::wstring(filePath)).detach();
    }

    void LayoutControls(HWND window)
    {
        RECT rc{}; GetClientRect(window, &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        const int m = Scale(22);
        const int cw = width - m * 2;

        const int headH = Scale(72);
        SetWindowPos(GetDlgItem(window, 3001), nullptr,
            m + Scale(60), Scale(14), cw - Scale(60), Scale(28),
            SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(window, kPathControl), nullptr,
            m + Scale(60), Scale(44), cw - Scale(60), Scale(18),
            SWP_NOZORDER | SWP_NOACTIVATE);

        const int gridTop = headH + Scale(16);
        const int gap = Scale(14);
        const int pillW = (cw - gap) / 2;
        const int pillH = Scale(86);
        for (size_t i = 0; i < g_actions.size(); ++i)
        {
            const int col = (int)(i % 2);
            const int row = (int)(i / 2);
            MoveWindow(g_actions[i].button,
                m + col * (pillW + gap), gridTop + row * (pillH + gap),
                pillW, pillH, TRUE);
        }
        const int gridBottom = gridTop + 3 * pillH + 2 * gap;

        if (g_progress)
            MoveWindow(g_progress, m, gridBottom + Scale(10), cw, Scale(8), TRUE);

        const int linkW = Scale(100);
        const int linkH = Scale(28);
        const int linkGap = Scale(14);
        const int linksTotal = 3 * linkW + 2 * linkGap;
        const int linkY = gridBottom + Scale(26);
        for (int i = 0; i < 3; ++i)
        {
            const int id = (i == 0) ? kButtonImport
                         : (i == 1) ? kButtonGoto : kButtonOpenDir;
            HWND b = GetDlgItem(window, id);
            if (b) MoveWindow(b, m + (cw - linksTotal) / 2 + i * (linkW + linkGap),
                              linkY, linkW, linkH, TRUE);
        }

        const int stH = Scale(20);
        if (g_status)
            MoveWindow(g_status, m, height - stH - Scale(10), cw, stH, TRUE);
    }

    HWND Mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, HWND parent)
    {
        return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    }

    Action* FindActionById(int id)
    {
        for (auto& a : g_actions) if (a.id == id) return &a;
        return nullptr;
    }
    Action* FindActionByHwnd(HWND h)
    {
        for (auto& a : g_actions) if (a.button == h) return &a;
        return nullptr;
    }

    void DrawButton(DRAWITEMSTRUCT* dis)
    {
        const int id = static_cast<int>(dis->CtlID);
        const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        const bool hover = (dis->itemState & ODS_HOTLIGHT) != 0;
        const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
        const RECT rc = dis->rcItem;
        SetBkMode(dis->hDC, TRANSPARENT);

        if (id == kButtonImport || id == kButtonGoto || id == kButtonOpenDir)
        {
            const wchar_t* text = (id == kButtonImport) ? L"导入 ZIP 初始化"
                                : (id == kButtonGoto)   ? L"前往下载"
                                                        : L"打开目录";
            FillRoundRect(dis->hDC, rc, rc.bottom - rc.top,
                disabled ? kDisabled : (pressed ? Tint(kLinkBg, -15)
                                                : hover ? kLinkBgHv : kLinkBg));
            SetTextColor(dis->hDC, disabled ? kDisText : kLinkText);
            HFONT old = (HFONT)SelectObject(dis->hDC, g_fontSmall);
            RECT tr = rc;
            DrawTextW(dis->hDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return;
        }

        Action* a = FindActionByHwnd(dis->hwndItem);
        if (!a) return;
        const int radius = (rc.bottom - rc.top) / 2;

        COLORREF fill = kDisabled;
        if (!disabled)
            fill = pressed ? Tint(a->base, -42) : hover ? Tint(a->base, 34) : a->base;
        FillRoundRect(dis->hDC, rc, radius, fill);

        const int h = rc.bottom - rc.top;
        const COLORREF mainText = disabled ? kDisText
                                : (a->darkText ? RGB(0x4A, 0x3A, 0x00) : RGB(255, 255, 255));
        const COLORREF subText = disabled ? kDisText
                               : (a->darkText ? RGB(0x6B, 0x57, 0x10)
                                              : RGB(255, 255, 255));

        HFONT old = (HFONT)SelectObject(dis->hDC, g_fontBtn);
        SetTextColor(dis->hDC, mainText);
        RECT tr = rc; tr.top += h * 22 / 100; tr.left += Scale(10); tr.right -= Scale(10);
        DrawTextW(dis->hDC, a->label, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);

        SelectObject(dis->hDC, g_fontBtnSub);
        SetTextColor(dis->hDC, subText);
        RECT sr = rc; sr.top += h * 54 / 100; sr.left += Scale(10); sr.right -= Scale(10);
        DrawTextW(dis->hDC, a->sub, -1, &sr, DT_CENTER | DT_TOP | DT_SINGLELINE);
        SelectObject(dis->hDC, old);
    }

    LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            g_window = window;
            InitDpi(window);
            CreateFontsAndBrushes();

            HWND t = Mk(L"STATIC", L"WB2API 启动器", SS_LEFT | SS_NOPREFIX, 3001, window);
            SendMessageW(t, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);

            HWND p = Mk(L"STATIC", g_wbProxyDirectory.c_str(),
                SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, kPathControl, window);
            SendMessageW(p, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            for (auto& a : g_actions)
                a.button = Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, a.id, window);

            Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, kButtonImport, window);
            Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, kButtonGoto, window);
            Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, kButtonOpenDir, window);

            g_progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
                WS_CHILD | PBS_SMOOTH | PBS_MARQUEE, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kProgressBar)),
                g_instance, nullptr);
            SendMessageW(g_progress, PBM_SETBARCOLOR, 0, RGB(0xF5, 0x37, 0x3C));
            SendMessageW(g_progress, PBM_SETBKCOLOR, 0, kBorder);
            ShowWindow(g_progress, SW_HIDE);

            g_status = Mk(L"STATIC", L"就绪", SS_LEFT | SS_ENDELLIPSIS, kStatusControl, window);
            SendMessageW(g_status, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            RefreshAllButtons();
            if (!CoreScriptReady())
                SetStatus(L"未检测到 wb-proxy，请先点击下方「导入 ZIP 初始化」");
            LayoutControls(window);
            return 0;
        }

        case WM_ERASEBKGND:
        {
            HDC dc = (HDC)wParam;
            RECT rc; GetClientRect(window, &rc);
            FillRect(dc, &rc, g_brBg);
            return 1;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps; HDC dc = BeginPaint(window, &ps);
            RECT rc; GetClientRect(window, &rc);
            const int headH = Scale(72);
            RECT head = rc; head.bottom = headH;
            FillRect(dc, &head, g_brHeader);
            if (g_appIcon48)
                DrawIconEx(dc, Scale(20), Scale(14), g_appIcon48,
                    Scale(44), Scale(44), 0, nullptr, DI_NORMAL);
            HPEN pen = CreatePen(PS_SOLID, Scale(1), kBorder);
            HPEN oldPen = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, 0, headH, nullptr);
            LineTo(dc, rc.right, headH);
            SelectObject(dc, oldPen); DeleteObject(pen);
            EndPaint(window, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC)wParam; HWND ctl = (HWND)lParam;
            SetBkMode(dc, TRANSPARENT);
            const int id = GetDlgCtrlID(ctl);
            if (id == 3001) { SetTextColor(dc, kText); return (LRESULT)g_brHeader; }
            if (id == kPathControl) { SetTextColor(dc, kSubText); return (LRESULT)g_brHeader; }
            SetTextColor(dc, kSubText);
            return (LRESULT)g_brBg;
        }

        case WM_DRAWITEM:
            DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;

        case WM_SIZE: LayoutControls(window); return 0;

        case WM_GETMINMAXINFO:
        {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = Scale(620);
            mmi->ptMinTrackSize.y = Scale(470);
            return 0;
        }

        case WM_IMPORT_DONE:
        {
            g_busy = false;
            if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
            std::unique_ptr<std::wstring> err(reinterpret_cast<std::wstring*>(lParam));
            RefreshAllButtons();
            if (wParam == 1)
            {
                SetStatus(L"初始化完成，wb-proxy 已就绪");
                MessageBoxW(window, L"wb-proxy 导入并解压完成，可以开始使用了。",
                    L"WB2API 启动器", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                const std::wstring msg = err ? *err : L"解压失败。";
                SetStatus(msg);
                MessageBoxW(window, msg.c_str(), L"导入失败", MB_OK | MB_ICONSTOP);
            }
            return 0;
        }

        case WM_SERVICE_WATCHED:
        {
            g_busy = false;
            if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
            RefreshAllButtons();
            if (wParam == 1)
            {
                SetStatus(L"服务已就绪，正在打开管理面板…");
                ShellExecuteW(window, L"open", kDashboardUrl, nullptr, nullptr, SW_SHOWNORMAL);
                DestroyWindow(window);
            }
            else
            {
                SetStatus(L"等待超时：端口 8788 未就绪，请查看控制台窗口中的服务日志。");
            }
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                const int id = LOWORD(wParam);
                if (Action* a = FindActionById(id))
                {
                    if (LaunchBatchFile(*a))
                    {
                        if (a->openDashboard)
                            StartServiceWatch();
                        else
                            SetStatus(std::wstring(L"已执行：") + a->label);
                    }
                    return 0;
                }
                switch (id)
                {
                case kButtonImport: StartImport(); return 0;
                case kButtonGoto:
                    ShellExecuteW(window, L"open",
                        (std::wstring(kProjectUrl) + L"/releases").c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                case kButtonOpenDir:
                {
                    const std::wstring dir = CoreScriptReady()
                        ? g_wbProxyDirectory : g_exeDirectory;
                    ShellExecuteW(window, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                default: break;
                }
            }
            break;

        case WM_DESTROY:
            DestroyFontsAndBrushes();
            PostQuitMessage(0);
            return 0;

        default: break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    g_instance = instance;

    INITCOMMONCONTROLSEX icc{ sizeof(icc),
        ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    g_exeDirectory = GetExecutableDirectory();
    if (g_exeDirectory.empty())
    {
        MessageBoxW(nullptr, L"无法确定程序目录。", L"WB2API 启动器", MB_OK | MB_ICONSTOP);
        return 1;
    }
    g_wbProxyDirectory = g_exeDirectory + L"\\wb-proxy";

    g_appIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1),
        IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    g_appIcon48 = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1),
        IMAGE_ICON, 48, 48, 0));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = g_appIcon ? g_appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = g_appIcon ? g_appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    if (RegisterClassExW(&wc) == 0)
    {
        MessageBoxW(nullptr, L"无法注册窗口类。", L"WB2API 启动器", MB_OK | MB_ICONSTOP);
        return 1;
    }

    g_window = CreateWindowExW(0, kWindowClassName, L"WB2API 启动器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 660, 520,
        nullptr, nullptr, instance, nullptr);
    if (!g_window)
    {
        MessageBoxW(nullptr, L"无法创建主窗口。", L"WB2API 启动器", MB_OK | MB_ICONSTOP);
        return 1;
    }

    ShowWindow(g_window, showCommand);
    UpdateWindow(g_window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    WSACleanup();
    return static_cast<int>(msg.wParam);
}
