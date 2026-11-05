#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

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

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr COLORREF kBrand     = RGB(0xE8, 0x3E, 0x4E);
    constexpr COLORREF kBrandDark = RGB(0xC4, 0x2A, 0x3A);
    constexpr COLORREF kAccent    = RGB(0x2B, 0x6C, 0xE0);  // 次要操作蓝
    constexpr COLORREF kBg        = RGB(0xF4, 0xF6, 0xF8);
    constexpr COLORREF kCard      = RGB(0xFF, 0xFF, 0xFF);
    constexpr COLORREF kText      = RGB(0x1A, 0x20, 0x24);
    constexpr COLORREF kSubText   = RGB(0x6B, 0x74, 0x7C);
    constexpr COLORREF kBorder    = RGB(0xE2, 0xE6, 0xEA);

    constexpr wchar_t kWindowClassName[] = L"WB2API.Launcher.Window";
    constexpr wchar_t kProjectUrl[] = L"https://github.com/ardeyouxipianyi/workbuddy2api-hub";

    constexpr int kButtonImport = 1004;
    constexpr int kButtonGoto   = 1005;
    constexpr int kStatusControl = 2001;
    constexpr int kProgressBar   = 2002;
    constexpr int kPathControl   = 2003;
    constexpr int kHeaderText    = 2004;

    constexpr UINT WM_IMPORT_DONE = WM_APP + 1;

    // 按钮类型：primary=品牌红主操作, secondary=次要, link=头部链接
    enum class BtnKind { Primary, Secondary, Link };

    struct Action
    {
        const wchar_t* label;
        const wchar_t* sub;
        const wchar_t* fileName;   // 为 nullptr 表示特殊动作
        BtnKind kind;
        int id;
        HWND button = nullptr;
    };

    HINSTANCE g_instance = nullptr;
    HWND g_window = nullptr;
    HWND g_status = nullptr;
    HWND g_progress = nullptr;
    HICON g_appIcon = nullptr;
    HICON g_appIconSm = nullptr;
    int g_dpi = 96;

    std::wstring g_exeDirectory;
    std::wstring g_wbProxyDirectory;
    std::atomic<bool> g_busy{false};

    HFONT g_fontTitle = nullptr;
    HFONT g_fontBtn = nullptr;
    HFONT g_fontBtnSub = nullptr;
    HFONT g_fontSmall = nullptr;
    HBRUSH g_brBg = nullptr;
    HBRUSH g_brCard = nullptr;

    // 启动类按钮（主操作，占主区域）
    std::vector<Action> g_startActions = {
        {L"本地启动",       L"仅本机访问 · 推荐",       L"start-wb-proxy.bat",                 BtnKind::Primary,   1101},
        {L"本地·可见窗口",  L"前台控制台便于看日志",     L"start-wb-proxy-visible.bat",         BtnKind::Secondary, 1102},
        {L"局域网启动",     L"允许局域网设备访问",       L"start-wb-proxy-lan.bat",             BtnKind::Primary,   1103},
        {L"局域网·可见窗口",L"共享模式 + 前台日志",      L"start-wb-proxy-lan-visible.bat",     BtnKind::Secondary, 1104},
    };
    // 维护类按钮（次要操作）
    std::vector<Action> g_maintainActions = {
        {L"重启服务",   L"停止后重新启动代理",   L"restart-wb-proxy.bat",  BtnKind::Secondary, 1201},
        {L"防火墙放行", L"允许局域网端口通过",   L"allow-firewall.bat",     BtnKind::Secondary, 1202},
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

    bool WbProxyReady()
    {
        // 只要核心启动脚本存在即认为就绪
        return ScriptExists(g_startActions[0]);
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
        g_fontBtn = MakeFont(12, FW_SEMIBOLD);
        g_fontBtnSub = MakeFont(9, FW_NORMAL);
        g_fontSmall = MakeFont(9, FW_NORMAL);
        g_brBg = CreateSolidBrush(kBg);
        g_brCard = CreateSolidBrush(kCard);
    }

    void DestroyFontsAndBrushes()
    {
        for (HFONT f : {g_fontTitle, g_fontBtn, g_fontBtnSub, g_fontSmall})
            if (f) DeleteObject(f);
        if (g_brBg) DeleteObject(g_brBg);
        if (g_brCard) DeleteObject(g_brCard);
        g_fontTitle = g_fontBtn = g_fontBtnSub = g_fontSmall = nullptr;
        g_brBg = g_brCard = nullptr;
    }

    void RefreshAllButtons()
    {
        auto refresh = [](std::vector<Action>& v) {
            for (Action& a : v)
            {
                EnableWindow(a.button, (ScriptExists(a) && !g_busy) ? TRUE : FALSE);
                InvalidateRect(a.button, nullptr, TRUE);
            }
        };
        refresh(g_startActions);
        refresh(g_maintainActions);
        HWND imp = GetDlgItem(g_window, kButtonImport);
        if (imp) { EnableWindow(imp, g_busy ? FALSE : TRUE); InvalidateRect(imp, nullptr, TRUE); }
        if (g_progress) ShowWindow(g_progress, g_busy ? SW_SHOW : SW_HIDE);
    }

    // ---------- 启动 bat ----------
    bool LaunchBatchFile(const Action& action)
    {
        const std::wstring scriptPath = g_wbProxyDirectory + L"\\" + action.fileName;
        if (!IsRegularFile(scriptPath))
        {
            const std::wstring msg = L"未找到脚本：\n" + scriptPath;
            SetStatus(msg);
            MessageBoxW(g_window, msg.c_str(), L"无法启动", MB_OK | MB_ICONERROR);
            return false;
        }
        wchar_t sysDir[MAX_PATH] = {};
        const UINT sysLen = GetSystemDirectoryW(sysDir, static_cast<UINT>(_countof(sysDir)));
        if (sysLen == 0 || sysLen >= _countof(sysDir))
        {
            const std::wstring msg = L"无法定位 cmd.exe。";
            SetStatus(msg);
            MessageBoxW(g_window, msg.c_str(), L"无法启动", MB_OK | MB_ICONERROR);
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
            MessageBoxW(g_window, msg.c_str(), L"无法启动", MB_OK | MB_ICONERROR);
            return false;
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        SetStatus(L"已启动：" + std::wstring(action.label));
        return true;
    }

    // ---------- 解压 ZIP ----------
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
        { errorOut = L"解压失败，tar 退出码 " + std::to_wstring(exitCode) +
                     L"。\n请确认选择的是有效的 ZIP 压缩包。"; return false; }
        return true;
    }

    void ImportThreadProc(std::wstring zipPath)
    {
        std::wstring err; bool ok = false;
        if (ExtractZip(zipPath, g_exeDirectory, err))
        {
            ok = WbProxyReady();
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
        SetStatus(L"正在解压导入...");
        if (g_progress) SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 50);
        std::thread(ImportThreadProc, std::wstring(filePath)).detach();
    }

    // ---------- 布局 ----------
    void LayoutControls(HWND window)
    {
        RECT rc{}; GetClientRect(window, &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        const int m = Scale(22);
        const int cw = width - m * 2;

        // 头部
        HWND title = GetDlgItem(window, 3001);
        if (title) MoveWindow(title, m + Scale(60), Scale(20), cw - Scale(60) - Scale(180), Scale(26), TRUE);
        HWND path = GetDlgItem(window, kPathControl);
        if (path) MoveWindow(path, m + Scale(60), Scale(48), cw - Scale(60) - Scale(180), Scale(18), TRUE);

        // 头部右侧：导入 + 前往下载 小链接
        const int linkW = Scale(82);
        const int linkGap = Scale(6);
        const int linkTop = Scale(28);
        HWND go = GetDlgItem(window, kButtonGoto);
        if (go) MoveWindow(go, rc.right - m - linkW, linkTop, linkW, Scale(24), TRUE);
        HWND imp = GetDlgItem(window, kButtonImport);
        if (imp) MoveWindow(imp, rc.right - m - linkW * 2 - linkGap, linkTop, linkW, Scale(24), TRUE);

        const int headH = Scale(84);
        const int statusH = Scale(38);
        const int contentTop = headH + Scale(14);
        const int contentBottom = height - statusH - Scale(10);
        const int gap = Scale(12);

        // 启动区：4 个按钮 2x2 网格（占主要面积）
        const int startRows = 2, startCols = 2;
        const int startBtnW = (cw - gap) / startCols;
        const int startBtnH = ((contentBottom - contentTop) * 2 / 3 - gap) / startRows;
        for (size_t i = 0; i < g_startActions.size(); ++i)
        {
            const int col = (int)(i % startCols);
            const int row = (int)(i / startCols);
            const int left = m + col * (startBtnW + gap);
            const int top = contentTop + row * (startBtnH + gap);
            MoveWindow(g_startActions[i].button, left, top, startBtnW, startBtnH, TRUE);
        }

        // 维护区：2 个按钮一行（底部，较矮）
        const int maintTop = contentTop + startRows * (startBtnH + gap) + Scale(4);
        const int maintBtnW = (cw - gap) / 2;
        const int maintBtnH = contentBottom - maintTop;
        for (size_t i = 0; i < g_maintainActions.size(); ++i)
        {
            const int left = m + (int)i * (maintBtnW + gap);
            MoveWindow(g_maintainActions[i].button, left, maintTop, maintBtnW, maintBtnH, TRUE);
        }

        // 进度条
        if (g_progress) MoveWindow(g_progress, m, contentBottom - Scale(2), cw, Scale(6), TRUE);

        // 状态栏
        if (g_status) MoveWindow(g_status, m, height - statusH, cw, Scale(22), TRUE);
    }

    HWND Mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, HWND parent)
    {
        return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    }

    // 通过 HWND 找 Action
    Action* FindAction(HWND btn)
    {
        for (auto& a : g_startActions) if (a.button == btn) return &a;
        for (auto& a : g_maintainActions) if (a.button == btn) return &a;
        return nullptr;
    }
    Action* FindActionById(int id)
    {
        for (auto& a : g_startActions) if (a.id == id) return &a;
        for (auto& a : g_maintainActions) if (a.id == id) return &a;
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

        // 头部小链接
        if (id == kButtonImport || id == kButtonGoto)
        {
            const wchar_t* text = (id == kButtonImport) ? L"导入 ZIP" : L"前往下载";
            const int radius = Scale(12);
            if (!disabled && (hover || pressed))
                FillRoundRect(dis->hDC, rc, radius,
                    pressed ? RGB(0xEC, 0xF1, 0xF4) : RGB(0xF0, 0xF2, 0xF5));
            SetTextColor(dis->hDC, disabled ? kSubText : kBrand);
            HFONT old = (HFONT)SelectObject(dis->hDC, g_fontSmall);
            RECT tr = rc;
            DrawTextW(dis->hDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return;
        }

        Action* a = FindAction(dis->hwndItem);
        if (!a) return;

        const bool isPrimary = (a->kind == BtnKind::Primary);
        const int radius = Scale(14);

        COLORREF bg;
        if (disabled) bg = RGB(0xE9, 0xEC, 0xEF);
        else if (isPrimary) bg = pressed ? kBrandDark : (hover ? RGB(0xF0, 0x5A, 0x68) : kBrand);
        else bg = pressed ? RGB(0xE4, 0xE9, 0xEE) : (hover ? RGB(0xF0, 0xF4, 0xF8) : kCard);
        FillRoundRect(dis->hDC, rc, radius, bg);

        if (!isPrimary)
        {
            HPEN pen = CreatePen(PS_SOLID, Scale(1),
                disabled ? kBorder : (hover ? kAccent : kBorder));
            HBRUSH oldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);
            RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
            SelectObject(dis->hDC, oldBr); SelectObject(dis->hDC, oldPen);
            DeleteObject(pen);
        }

        const int btnH = rc.bottom - rc.top;
        const bool tall = btnH > Scale(70);   // 启动大卡显示标题+副标题，维护小卡只显示标题

        const COLORREF main = disabled ? kSubText
            : (isPrimary ? RGB(255, 255, 255) : kText);
        const COLORREF sub = disabled ? kSubText
            : (isPrimary ? RGB(255, 228, 232) : kSubText);

        HFONT old = (HFONT)SelectObject(dis->hDC, g_fontBtn);
        SetTextColor(dis->hDC, main);
        if (tall)
        {
            RECT tr = rc; tr.top += (btnH - Scale(44)) / 2; tr.left += Scale(8); tr.right -= Scale(8);
            DrawTextW(dis->hDC, a->label, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);
            SelectObject(dis->hDC, g_fontBtnSub);
            SetTextColor(dis->hDC, sub);
            RECT sr = rc; sr.top = tr.top + Scale(26); sr.left += Scale(8); sr.right -= Scale(8);
            DrawTextW(dis->hDC, a->sub, -1, &sr, DT_CENTER | DT_TOP | DT_SINGLELINE);
        }
        else
        {
            RECT tr = rc; tr.top += Scale(2);
            DrawTextW(dis->hDC, a->label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
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

            for (auto& a : g_startActions)
                a.button = Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, a.id, window);
            for (auto& a : g_maintainActions)
                a.button = Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, a.id, window);

            Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, kButtonImport, window);
            Mk(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, kButtonGoto, window);

            g_progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
                WS_CHILD | PBS_SMOOTH | PBS_MARQUEE, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kProgressBar)),
                g_instance, nullptr);
            SendMessageW(g_progress, PBM_SETBARCOLOR, 0, kBrand);
            SendMessageW(g_progress, PBM_SETBKCOLOR, 0, kBorder);
            ShowWindow(g_progress, SW_HIDE);

            g_status = Mk(L"STATIC", L"就绪", SS_LEFT | SS_ENDELLIPSIS, kStatusControl, window);
            SendMessageW(g_status, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            RefreshAllButtons();
            if (!WbProxyReady())
                SetStatus(L"未检测到 wb-proxy，点右上角「导入 ZIP」初始化");
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
            RECT head = rc; head.bottom = Scale(84);
            FillRect(dc, &head, g_brCard);
            if (g_appIconSm)
                DrawIconEx(dc, Scale(22), Scale(20), g_appIconSm,
                    Scale(40), Scale(40), 0, nullptr, DI_NORMAL);
            HPEN pen = CreatePen(PS_SOLID, Scale(1), kBorder);
            HPEN oldPen = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, 0, head.bottom, nullptr);
            LineTo(dc, rc.right, head.bottom);
            SelectObject(dc, oldPen); DeleteObject(pen);
            EndPaint(window, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC)wParam; HWND ctl = (HWND)lParam;
            SetBkMode(dc, TRANSPARENT);
            const int id = GetDlgCtrlID(ctl);
            if (id == 3001) { SetTextColor(dc, kText); return (LRESULT)g_brCard; }
            if (id == kPathControl) { SetTextColor(dc, kSubText); return (LRESULT)g_brCard; }
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
            mmi->ptMinTrackSize.x = Scale(640);
            mmi->ptMinTrackSize.y = Scale(480);
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
                MessageBoxW(window, msg.c_str(), L"导入失败", MB_OK | MB_ICONERROR);
            }
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                const int id = LOWORD(wParam);
                if (id == kButtonImport) { StartImport(); return 0; }
                if (id == kButtonGoto)
                {
                    ShellExecuteW(window, L"open",
                        (std::wstring(kProjectUrl) + L"/releases").c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                if (Action* a = FindActionById(id)) { LaunchBatchFile(*a); return 0; }
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

    g_exeDirectory = GetExecutableDirectory();
    if (g_exeDirectory.empty())
    {
        MessageBoxW(nullptr, L"无法确定程序目录。", L"WB2API 启动器", MB_OK | MB_ICONERROR);
        return 1;
    }
    g_wbProxyDirectory = g_exeDirectory + L"\\wb-proxy";

    g_appIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1),
        IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    g_appIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1),
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
        MessageBoxW(nullptr, L"无法注册窗口类。", L"WB2API 启动器", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_window = CreateWindowExW(0, kWindowClassName, L"WB2API 启动器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, Scale(760), Scale(560),
        nullptr, nullptr, instance, nullptr);
    if (!g_window)
    {
        MessageBoxW(nullptr, L"无法创建主窗口。", L"WB2API 启动器", MB_OK | MB_ICONERROR);
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
    return static_cast<int>(msg.wParam);
}
