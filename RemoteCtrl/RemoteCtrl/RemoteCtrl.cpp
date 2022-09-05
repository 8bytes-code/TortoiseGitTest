// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"

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

int main() {
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr) {
		// 初始化 MFC 并在失败时显示错误
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0)) {
			// TODO: 在此处为应用程序的行为编写代码。
			wprintf(L"错误: MFC 初始化失败\n");
			nRetCode = 1;
		} else {
			// TODO: 在此处为应用程序的行为编写代码。
			CServerSocket* pserver = CServerSocket::getInstance();
			int count = 0;

			if (pserver->InitSocket() == false) {
				MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状况！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
				exit(0);
			}
			while (CServerSocket::getInstance() != NULL) {
				if (pserver->AcceptClient() == false) {
					if (count <= 3) {
						MessageBox(NULL, _T("多次重连仍无法接入，结束本程序！"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
						exit(0);
					}
					MessageBox(NULL, _T("无法正常接入用户，请稍后正在自动重试！"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
					count++;
				}

				int ret = pserver->DealCommand();
				//
			}
		}
	} else {
		// TODO: 更改错误代码以符合需要
		wprintf(L"错误: GetModuleHandle 失败\n");
		nRetCode = 1;
	}

	return nRetCode;
}
