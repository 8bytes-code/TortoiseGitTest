﻿// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "HeTool.h"
#include "Command.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 下列为不熟悉项目属性修改的时候可使用直接源码指定的情况
// #pragma comment(linker,"/subsystem:windows /entry:WinMainCRTStartup")
// #pragma comment(linker,"/subsystem:windows /entry:mainCRTStartup")
// #pragma comment(linker,"/subsystem:console /entry:mainCRTStartup")
// #pragma comment(linker,"/subsystem:console /entry:WinMainCRTStartup")


// 唯一的应用程序对象
CWinApp theApp;
using namespace std;

void WriteRegisterTable(const CString& strPath) {
	CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	char sPath[MAX_PATH] = "";
	char sSys[MAX_PATH] = "";
	std::string strExe = "\\RemoteCtrl.exe ";
	GetCurrentDirectoryA(MAX_PATH, sPath);
	GetSystemDirectoryA(sSys, sizeof(sSys));
	//构造软链接命令
	std::string strCmd = "mklink " + std::string(sSys) + strExe + std::string(sPath) + strExe;
	system(strCmd.c_str());

	//打开注册表
	HKEY hKey = NULL;
	int ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		MessageBox(NULL, _T("设置开机启动失败，是否权限不足? \r\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		exit(0);
	}

	//写入注册表
	ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		MessageBox(NULL, _T("设置开机启动失败，是否权限不足? \r\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		exit(0);
	}
	RegCloseKey(hKey);
}

void WriteStartupDir(const CString& strPath) {
	CString strCmd = GetCommandLine();
	//消除getcommandline所产生的头尾双引号
	strCmd.Replace(_T("\""), _T(""));
	BOOL ret = CopyFile(strCmd, strPath, FALSE);
	if (ret == FALSE) {
		MessageBox(NULL, _T("复制文件失败，是否权限不足或文件不存在？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		exit(0);
	}
}

void ChooseAutoInvoke() {
	//验证是否存在，存在就不用重写了
	//CString strPath = CString(_T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe"));
	CString strPath = _T("C:\\Users\\test\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe");
	if (PathFileExists(strPath)) {
		return;
	}

	CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	CString strInfo = _T("该程序只允许用于合法的用途!\n");
	strInfo += _T("继续运行该程序将使得这台机器处于监控状态!\n");
	strInfo += _T("如果您不希望如此，请按“取消”按钮，退出程序\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到您的机器上，并随着系统启动而自动运行!\n");
	strInfo += _T("按下“否”按钮，则程序只会运行一次，不会再系统内留下任何东西!\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);

	if (ret == IDYES) {
		//WriteRegisterTable(strPath);
		WriteStartupDir(strPath);
	}
	else if (ret == IDCANCEL) {
		exit(0);
	}
}

void ShowError() {
	LPWSTR lpMessageBuf = NULL;
	//strerror();	C语言库
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&lpMessageBuf, 0, NULL
	);
	OutputDebugString(lpMessageBuf);
	MessageBox(NULL, lpMessageBuf, _T("发生错误"), 0);
	LocalFree(lpMessageBuf);
}

//检测提权
bool IsAdmin() {
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		ShowError();
		return false;
	}

	TOKEN_ELEVATION eve;
	DWORD len = 0;
	if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == FALSE) {
		ShowError();
		return false;
	}
	CloseHandle(hToken);

	if (len == sizeof(eve)) {
		//提权成功返回值大于0
		return eve.TokenIsElevated;
	}
	printf("length of tokeninformation is %d\r\n", len);
	return false;
}

void RunAsAdmin() {
	//管理员是否能登录
	HANDLE hToken = NULL;
	BOOL ret = LogonUser(L"Administrator", NULL, NULL, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
	if (!ret) {
		ShowError();
		MessageBox(NULL, _T("登录失败"), _T("程序错误"), 0);
		exit(0);
	}
	OutputDebugString(L"Logon administratos success!\r\n");

	//创建进程
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	TCHAR sPath[MAX_PATH] = _T("");
	GetCurrentDirectory(MAX_PATH, sPath);
	CString strCmd = sPath;
	strCmd += _T("\\RemoteCtrl.exe");
	//ret = CreateProcessWithTokenW(hToken, LOGON_WITH_PROFILE, NULL, (LPWSTR)(LPCTSTR)strCmd, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
	ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, (LPWSTR)(LPCWSTR)strCmd, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
	CloseHandle(hToken);
	if (!ret) {
		ShowError();
		MessageBox(NULL, strCmd, _T("程序错误"), 0);
		exit(0);
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

int main() {
	int nRetCode = 0;
	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr) {
		// 初始化 MFC 并在失败时显示错误
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
			wprintf(L"错误: MFC 初始化失败\n");
			nRetCode = 1;
		} 
		else {
			if (IsAdmin()) {
				OutputDebugString(L"current is run as administrator!\r\n");
				MessageBox(NULL, _T("管理员"), _T("用户状态"), 0);
			}
			else {
				OutputDebugString(L"current is run as normal user!\r\n");
				//如果还不是管理员权限就要再去提权
				RunAsAdmin();
				MessageBox(NULL, _T("普通用户"), _T("用户状态"), 0);
				return nRetCode;
			}

			CCommand cmd;
			ChooseAutoInvoke();
			int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
			switch (ret) {
				case -1:
					MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状况！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
					exit(0);
					break;
				case -2:
					MessageBox(NULL, _T("多次重连仍无法接入，结束本程序！"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
					exit(0);
					break;
				default:break;
			}
		}
	} 
	else {
		wprintf(L"错误: GetModuleHandle 失败\n");
		nRetCode = 1;
	}

	return nRetCode;
}
