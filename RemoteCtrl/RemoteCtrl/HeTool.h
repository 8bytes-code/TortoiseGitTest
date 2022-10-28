#pragma once

class CHeTool {
public:
	//使用静态方法，转变成一个相对的全局函数
	static void Dump(BYTE* pData, size_t nSize) {
		std::string strOut;
		for (size_t i = 0; i < nSize; i++) {
			char buf[8] = "";
			if (i > 0 && (i % 16 == 0)) strOut += "\n";
			snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
			strOut += buf;
		}
		strOut += "\n";
		OutputDebugStringA(strOut.c_str());
	}

	//检测提权
	static bool IsAdmin() {
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

	static bool RunAsAdmin() {
		//创建进程 前提是本地策略组有开启管理员账户 和 取消禁止空密码登录
		STARTUPINFO si = { 0 };
		PROCESS_INFORMATION pi = { 0 };
		TCHAR sPath[MAX_PATH] = _T("");
		GetModuleFileName(NULL, sPath, MAX_PATH);
		bool ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, sPath, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
		if (!ret) {
			ShowError();
			MessageBox(NULL, sPath, _T("创建进程失败"), 0);
			return false;
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}

	static void ShowError() {
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

	//通过修改开机启动文件夹来实现开机启动
	static BOOL WriteStartupDir(const CString& strPath) {
		TCHAR sPath[MAX_PATH] = _T("");
		GetModuleFileName(NULL, sPath, MAX_PATH);
		return CopyFile(sPath, strPath, FALSE);
	}

	//通过修改注册表来实现开机启动
	static bool WriteRegisterTable(const CString& strPath) {
		CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
		TCHAR sPath[MAX_PATH] = _T("");
		GetModuleFileName(NULL, sPath, MAX_PATH);
		BOOL ret = CopyFile(sPath, strPath, FALSE);
		if (ret == FALSE) {
			MessageBox(NULL, _T("复制文件失败，是否权限不足或文件不存在？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}

		//打开注册表
		HKEY hKey = NULL;
		ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
		if (ret != ERROR_SUCCESS) {
			RegCloseKey(hKey);
			MessageBox(NULL, _T("设置开机启动失败，是否权限不足? \r\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}

		//写入注册表
		ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
		if (ret != ERROR_SUCCESS) {
			RegCloseKey(hKey);
			MessageBox(NULL, _T("设置开机启动失败，是否权限不足? \r\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
		RegCloseKey(hKey);
		return true;
	}

	//适用于MFC命令行项目初始化
	static bool Init() {
		HMODULE hModule = ::GetModuleHandle(nullptr);
		if (hModule == nullptr) {
			wprintf(L"错误: GetModuleHandle 失败\n");
			return false;
		}
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
			wprintf(L"错误: MFC初始化 失败\n");
			return false;
		}
		return true;
	}
};

