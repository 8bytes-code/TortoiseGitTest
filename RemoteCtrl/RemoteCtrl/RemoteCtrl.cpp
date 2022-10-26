// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "HeTool.h"
#include "Command.h"
#include "CHeQueue.h"
#include <conio.h>

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
#define IOCP_LIST_EMPTY 0
#define IOCP_LIST_PUSH 1
#define IOCP_LIST_POP 2

enum {
	IocpListEmpty,
	IocpListPush,
	IocpListPop
};

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

typedef struct IocpParm {
	int nOperator;					//操作
	std::string strData;			//数据
	_beginthread_proc_type cbFunc;	//回调

	IocpParm(int op, const char* sData, _beginthread_proc_type cb = NULL) {
		nOperator = op;
		strData = sData;
		cbFunc = cb;
	}
	IocpParm() {
		nOperator = -1;
	}
}IOCP_PARAM;

void threadmain(HANDLE hIOCP) {
	std::list<std::string> lstString;
	//获取完成端口状态
	int count = 0, count0 = 0;
	DWORD dwTransferred = 0;
	ULONG_PTR CompletionKey = 0;
	OVERLAPPED* pOverlapped = NULL;
	while (GetQueuedCompletionStatus(hIOCP, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {
		if ((dwTransferred == 0) && (CompletionKey == NULL)) {
			printf("thread is prepare to exit!\r\n");
			break;
		}
		IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;
		if (pParam->nOperator == IocpListPush) {
			lstString.push_back(pParam->strData);
			count0++;
		}
		else if (pParam->nOperator == IocpListPop) {
			std::string Str;
			if (lstString.size() > 0) {
				Str = lstString.front();
				lstString.pop_front();
			}
			if (pParam->cbFunc) {
				pParam->cbFunc(&Str);
			}
			count++;
		}
		else if (pParam->nOperator == IocpListEmpty) {
			lstString.clear();
		}
		delete pParam;
	}
	//lstString.clear();
	printf("thread exit count %d count0 %d\r\n", count, count0);
}

void threadQueueEntry(HANDLE hIOCP) {
	threadmain(hIOCP);
	_endthread();	//结束线程到此之后代码就终止了，会导致本地对象没法调用析构函数，从而使得内存发生泄漏
}

void func(void* arg) {
	std::string* pstr = (std::string*)arg;
	if (pstr != NULL) {
		printf("pop from list:%s\r\n", pstr->c_str());
		//delete pstr;
	}
	else {
		printf("list is empty,not data!\r\n");
	}
}

void test() {
	CHeQueue<std::string> lstStrings;
	ULONGLONG tick = GetTickCount64();
	ULONGLONG tick0 = GetTickCount64();
	ULONGLONG total = GetTickCount64();
	//while (_kbhit() == 0) {
		//完成端口把请求和实现分离
	while (GetTickCount64() - total <= 1000) {
		//if (GetTickCount64() - tick0 > 1300) {
		lstStrings.PushBack("hello wrold!");
		tick0 = GetTickCount64();
		//}
	}
	size_t count = lstStrings.Size();
	printf("lstString done! size:%d\r\n", count);
	total = GetTickCount64();
	while(GetTickCount64()-total <= 1000){
		//if (GetTickCount64() - tick > 2000) {
			std::string str;
			lstStrings.PopFront(str);
			tick = GetTickCount64();
			//printf("pop from queue:%s\r\n", str.c_str());
		//}
		//Sleep(1);
	}
	printf("exit done! size:%d\r\n", count-lstStrings.Size());
	lstStrings.Clear();

	std::list<std::string> lstData;
	total = GetTickCount64();
	while (GetTickCount64() - total <= 1000) {
		lstData.push_back("hello wrold!");
	}
	count = lstData.size();
	printf("lstData push done! size:%d\r\n", lstData.size());
	total = GetTickCount64();
	while (GetTickCount64() - total <= 250) {
		if (lstData.size() > 0)lstData.pop_front();
	}
	printf("lstData pop done! size:%d\r\n", (count - lstData.size()) * 4);
}

int main() {
	if (!CHeTool::Init()) return 1;
	for (int i = 0; i < 10; i++) {
		test();
	}
	
// 	printf("press any key to exit...\r\n");
// 	CHeQueue<std::string> lstStrings;
// 	ULONGLONG tick = GetTickCount64();
// 	ULONGLONG tick0 = GetTickCount64();
// 	while(_kbhit() == 0){
// 		//完成端口把请求和实现分离
// 		if (GetTickCount64() - tick0 > 1300) {
// 			lstStrings.PushBack("hello wrold!");
// 			tick0 = GetTickCount64();
// 		}
// 		if (GetTickCount64() - tick > 2000) {
// 			std::string str;
// 			lstStrings.PopFront(str);
// 			tick = GetTickCount64();
// 			printf("pop from queue:%s\r\n", str.c_str());
// 		}
// 		Sleep(1);
// 	}
// 	printf("exit done! size:%d\r\n",lstStrings.Size());
// 	lstStrings.Clear();
// 	printf("exit done! size:%d\r\n", lstStrings.Size());
	

	/*
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
	*/
	return 0;
}
