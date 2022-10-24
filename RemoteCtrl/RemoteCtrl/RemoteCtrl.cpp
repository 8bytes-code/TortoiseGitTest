// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
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

//_T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")
#define INVOKE_PATH _T("C:\\Users\\test\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe")

// 唯一的应用程序对象
CWinApp theApp;
using namespace std;


bool ChooseAutoInvoke(const CString& strPath) {
	if (PathFileExists(strPath)) {
		return false;
	}

	CString strInfo = _T("该程序只允许用于合法的用途!\n");
	strInfo += _T("继续运行该程序将使得这台机器处于监控状态!\n");
	strInfo += _T("如果您不希望如此，请按“取消”按钮，退出程序\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到您的机器上，并随着系统启动而自动运行!\n");
	strInfo += _T("按下“否”按钮，则程序只会运行一次，不会再系统内留下任何东西!\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);

	if (ret == IDYES) {
		//WriteRegisterTable(strPath);
		if (!CHeTool::WriteStartupDir(strPath)) {
			MessageBox(NULL, _T("复制文件失败，是否权限不足或文件不存在？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
	}
	else if (ret == IDCANCEL) {
		return false;
	}
	return true;
}

int main() {
	if (CHeTool::IsAdmin()) {
		if (!CHeTool::Init()) return 1;
		if (ChooseAutoInvoke(INVOKE_PATH)) {
			CCommand cmd;
			int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
			switch (ret) {
				case -1:
					MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状况！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
					break;
				case -2:
					MessageBox(NULL, _T("多次重连仍无法接入，结束本程序！"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
					break;
				default:break;
			}
		}
	}
	else {
		//如果还不是管理员权限就要再去提权
		if (CHeTool::RunAsAdmin() == false) {
			CHeTool::ShowError();
			return 1;
		}
	}

	return 0;
}
