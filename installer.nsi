; ============================================================
; WB2API Launcher NSIS 安装脚本
; 编译：makensis installer.nsi   （或右键 Compile NSIS Script）
;
; 设计：当前用户安装（无需 UAC），wb-proxy 由用户自行导入，不随安装包分发。
; ============================================================

!include "MUI2.nsh"
!include "LogicLib.nsh"
!define IDYES 6
!define IDNO 7
!define IDCANCEL 2

; ---------- 基本信息 ----------
!define APP_NAME      "WB2API Launcher"
!define APP_VERSION   "1.0.0"
!define APP_PUBLISHER "MinOS-WJ"
!define APP_EXE       "WB2APILauncher.exe"
!define PROJECT_URL   "https://github.com/ardeyouxipianyi/workbuddy2api-hub"

Name "${APP_NAME}"
OutFile "WB2APILauncher-Setup.exe"
Unicode True

; 当前用户安装目录，无需管理员权限
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
RequestExecutionLevel user

; 覆盖安装时读取上次目录
InstallDirRegKey HKCU "Software\${APP_NAME}" "InstallDir"

; ---------- 界面 ----------
!define MUI_ABORTWARNING
!define MUI_ICON   "logo.ico"
!define MUI_UNICON "logo.ico"

; 完成页：提供"运行"和"访问下载页"两个选项
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "运行 ${APP_NAME}"
!define MUI_FINISHPAGE_LINK "前往项目主页下载 wb-proxy"
!define MUI_FINISHPAGE_LINK_LOCATION "${PROJECT_URL}/releases"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

; ---------- 安装 ----------
Section "主程序" SecMain
    SectionIn RO

    ; 若旧版本正在运行，先尝试关闭
    FindWindow $0 "WB2API.Launcher.Window"
    ${If} $0 != 0
        SendMessage $0 ${WM_CLOSE} 0 0
        Sleep 1000
    ${EndIf}

    SetOutPath "$INSTDIR"
    File "WB2APILauncher.exe"
    File "LICENSE.txt"

    ; 记录安装目录
    WriteRegStr HKCU "Software\${APP_NAME}" "InstallDir" "$INSTDIR"

    ; 开始菜单
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"

    ; 桌面快捷方式
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

    ; “应用和功能”卸载信息
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "URLInfoAbout" "${PROJECT_URL}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1

    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

; ---------- 卸载 ----------
; 备份 wb-proxy 到桌面（xcopy 递归），返回 $1 = 0 成功 / 非 0 失败
Function un.BackupWbProxy
    StrCpy $1 0
    IfFileExists "$INSTDIR\wb-proxy" 0 backup_done
    CreateDirectory "$DESKTOP\WB2API-Backup"
    ; 用 nsExec 执行并捕获退出码到 $1
    nsExec::Exec 'xcopy /E /I /H /Y "$INSTDIR\wb-proxy\*" "$DESKTOP\WB2API-Backup\wb-proxy\"'
    Pop $1
    ${If} $1 == 0
        DetailPrint "已备份到：$DESKTOP\WB2API-Backup"
    ${Else}
        DetailPrint "备份失败（xcopy 退出码 $1）"
    ${EndIf}
    backup_done:
FunctionEnd

Section "Uninstall"
    ; ---------- 静默卸载：保守策略，仅删程序，保留用户数据 ----------
    ${If} ${Silent}
        Goto un_remove_program
    ${EndIf}

    ; ---------- 交互卸载：用变量接收返回值再判断 ----------
    ; 第一步：是否删除用户数据
    MessageBox MB_YESNOCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2 "是否删除用户数据（wb-proxy 网关与配置）？$\n$\n[是] 删除用户数据$\n[否] 保留用户数据，只卸载程序$\n[取消] 中止卸载"
    Pop $2
    ${If} $2 == ${IDCANCEL}
        SetAutoClose false
        Abort
    ${ElseIf} $2 == ${IDNO}
        Goto un_keep
    ${EndIf}
    ; 走到这里 = IDYES，继续问是否备份

    ; 第二步：删除前是否备份
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON1 "删除前是否先将 wb-proxy 备份到桌面？$\n$\n[是] 备份到桌面后删除$\n[否] 直接完全删除"
    Pop $2
    ${If} $2 == ${IDYES}
        Call un.BackupWbProxy
        ${If} $1 == 0
            MessageBox MB_ICONINFORMATION|MB_OK "用户数据已备份到：$\n$DESKTOP\WB2API-Backup"
            RMDir /r "$INSTDIR\wb-proxy"
        ${Else}
            MessageBox MB_ICONSTOP|MB_OK "备份失败！wb-proxy 已保留。"
        ${EndIf}
    ${Else}
        RMDir /r "$INSTDIR\wb-proxy"
    ${EndIf}
    Goto un_remove_program

    un_keep:
    DetailPrint "保留用户数据：$INSTDIR\wb-proxy"

    un_remove_program:
    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\LICENSE.txt"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\${APP_NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${APP_NAME}"

    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKCU "Software\${APP_NAME}"
SectionEnd