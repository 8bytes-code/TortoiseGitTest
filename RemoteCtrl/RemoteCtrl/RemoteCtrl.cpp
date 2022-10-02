﻿// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <corecrt_io.h>
#include <list>
#include <atlimage.h>
#include "LockDialog.h"

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
void Dump(BYTE* pData, size_t nSize) {
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

int MakeDriverInfo() {
	//1==>A  2==>B 此前老版本为软盘，故此现在的分区都是从C开始...26==>Z
	std::string result;
	for (int i = 1; i <= 26; i++) {
		if (_chdrive(i) == 0) {
			if (result.size() > 0) result += ',';
			result += 'A' + i - 1;
		}
	}
	result += ',';

	CPacket pack(1, (BYTE*)result.c_str(), result.size());
	Dump((BYTE*)pack.Data(), pack.Size());
	CServerSocket::getInstance()->Send(pack);
	Sleep(100);

	return 0;
}

int MakeDirecteryInfo() {
	std::string strPath;
	//std::list<FILEINFO> lstFileInfos;
	if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
		OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误!"));
		return -1;
	}

	TRACE("cd C is %d\r\n", _chdir("C:"));
	if (_chdir(strPath.c_str()) != 0) {
		TRACE("cd strPath:[%s] is %d\r\n", strPath.c_str(), _chdir(strPath.c_str()));
		FILEINFO finfo;
		finfo.HasNext = FALSE;
		CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
		CServerSocket::getInstance()->Send(pack);
		OutputDebugString(_T("没有权限访问目录!!"));
		return -2;
	}

	_finddata_t fdata;
	int hfind = 0;
	if ((hfind = _findfirst("*", &fdata)) == -1) {
		//*表示匹配所有文件，如果返回-1则表示没有找到
		OutputDebugString(_T("没有找到任何文件!!"));
		FILEINFO finfo;
		finfo.HasNext = FALSE;
		CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
		CServerSocket::getInstance()->Send(pack);
		return -3;
	}

	int Count = 0;	//发送时文件数
	do {
		FILEINFO finfo;
		finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
		memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
		TRACE("%s\r\n", finfo.szFileName);
		CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
		CServerSocket::getInstance()->Send(pack);
		Count++;
	} while (!_findnext(hfind, &fdata));
	TRACE("server:count=%d\r\n", Count);

	//发送信息到控制端
	FILEINFO finfo;
	finfo.HasNext = FALSE;
	CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
	CServerSocket::getInstance()->Send(pack);

	return 0;
}

int RunFIle() {
	std::string strPath;
	CServerSocket::getInstance()->GetFilePath(strPath);
	ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
	
	CPacket pack(3, NULL, 0);
	CServerSocket::getInstance()->Send(pack);

	return 0;
}

//#pragma warning(disable:4966) 为一种禁用警告，针对有些老函数不安全的时候

int DownloadFile() {
	/*
	 * 首先获取路径
	 * 然后打开文件，如果失败就结束	
	 * 能打开就开始读取然后发送直至为空
	 * 最后关闭
	*/
	std::string strPath;
	CServerSocket::getInstance()->GetFilePath(strPath);
	TRACE("server getPath[%s]\r\n", strPath.c_str());

	long long data = 0;
	FILE* pFile = NULL;
	errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
	TRACE("server getErr:%d\r\n", err);
	if (err != 0) {
		CPacket pack(4, (BYTE*)&data, 8);
		CServerSocket::getInstance()->Send(pack);
		return -1;
	}

	if (pFile != NULL) {
		//优化进度条，以免文件过大时空等
		fseek(pFile, 0, SEEK_END);
		data = _ftelli64(pFile);
		CPacket head(4, (BYTE*)&data, 8);
		CServerSocket::getInstance()->Send(head);
		fseek(pFile, 0, SEEK_SET);

		char buffer[1024] = "";
		size_t rlen = 0;
		do {
			//每次读1字节，读1024次
			rlen = fread(buffer, 1, 1024, pFile);
			//读到之后发送
			CPacket pack(4, (BYTE*)buffer, rlen);
			CServerSocket::getInstance()->Send(pack);
		} while (rlen >= 1024);
		fclose(pFile);
	}
	
	//循环发送完后最后是空代表结束
	CPacket pack(4, NULL, 0);
	CServerSocket::getInstance()->Send(pack);

	return 0;
}

int MouseEvent() {
	MOUSEEV mouse;
	if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {
		
		DWORD nFlags = 0;
		switch (mouse.nButton) {
			case 0:		//左键
				nFlags = 1;
				break;
			case 1:		//右键
				nFlags = 2;
				break;
			case 2:		//中键
				nFlags = 4;
				break;
			case 4:		//没有按键
				nFlags = 8;
				break;
		}

		if (nFlags != 8) SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
		switch (mouse.nAction) {
			case 0:		//单击
				nFlags |= 0x10;
				break;
			case 1:		//双击
				nFlags |= 0x20;
				break;
			case 2:		//按下
				nFlags |= 0x40;
				break;
			case 3:		//放开
				nFlags |= 0x80;
				break;
			default:break;
		}
		TRACE("mouse event: %08X x%d y %d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);
		switch (nFlags) {
			case 0x21:	//左键双击
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x11:	//左键单击
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x41:	//左键按下
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x81:	//左键松开
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x22:	//右键双击
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x12:	//右键单击
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x42:	//右键按下
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x82:	//右键松开
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x24:	//中键双击
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x14:	//中键单击
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x44:	//中键按下
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x84:	//中键松开
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x08:	//单纯的鼠标移动
				mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
				break;
		}

		CPacket pack(5, NULL, 0);
		CServerSocket::getInstance()->Send(pack);

	} else {
		OutputDebugString(_T("获取鼠标操作参数失败!!"));
		return -1;
	}

	return 0;
}

int SendScreen() {
	CImage screen;	//GDI
	HDC hScreen = ::GetDC(NULL);

	//获取位宽, RGB/888=24 ARGB/8888=32
	int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);	
	int nWidth = GetDeviceCaps(hScreen, HORZRES);
	int nHeight = GetDeviceCaps(hScreen, VERTRES);

	//创建窗口，按照获取的宽和高还有位宽
	screen.Create(nWidth, nHeight, nBitPerPixel);	
	BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
	ReleaseDC(NULL, hScreen);

	//DWORD tick = GetTickCount64();
	//TRACE("png %d\r\n", GetTickCount64() - tick);
	//tick = GetTickCount64();
	//screen.Save(_T("test2022.jpg"), Gdiplus::ImageFormatJPEG);
	//TRACE("jpg %d\r\n", GetTickCount64() - tick);
	
	//测试时用的是图片文件，但是网络中需要转换成数据流发送
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
	if (hMem == NULL)return -1;
	IStream* pStream = NULL;
	HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
	if (ret == S_OK) {
		screen.Save(pStream, Gdiplus::ImageFormatPNG);
		LARGE_INTEGER bg = { 0 };
		pStream->Seek(bg, STREAM_SEEK_SET, NULL);
		PBYTE pData = (PBYTE)GlobalLock(hMem);
		SIZE_T nSize = GlobalSize(hMem);
		CPacket pack(6, pData, nSize);
		CServerSocket::getInstance()->Send(pack);
		GlobalUnlock(hMem);
	}

	pStream->Release();
	GlobalFree(hMem);
	//screen.Save(_T("2022.png"), Gdiplus::ImageFormatPNG);
	screen.ReleaseDC();
	return 0;
}

CLockDialog dlg;
unsigned threadid = 0;

unsigned _stdcall threadLockDlg(void* arg) {
	//创建对话框，显示，不可调整大小|不可移动
	dlg.Create(IDD_DIALOG_INFO, NULL);
	dlg.ShowWindow(SW_SHOW);
	//遮蔽后面的应用
	CRect rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = GetSystemMetrics(SM_CXFULLSCREEN);
	rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
	rect.bottom = LONG(rect.bottom * 1.10);
	dlg.MoveWindow(rect);
	
	//将字体稍微调整至中间
	CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
	if (pText) {
		CRect rtText;
		pText->GetWindowRect(rtText);
		int nWidth = rtText.Width();
		int x = (rect.right - nWidth) / 2;
		int nHeight = rtText.Height();
		int y = (rect.bottom - nHeight) / 2;
		pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
	}

	//窗口置顶
	dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	//隐藏鼠标
	ShowCursor(false);
	//隐藏任务栏
	::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
	
	dlg.GetWindowRect(rect);
	rect.left = 0;
	rect.top = 0;
	rect.right = 1;
	rect.bottom = 1;
	ClipCursor(rect);			//限制鼠标的活动范围

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_KEYDOWN) {
			TRACE("msg:%08X wparam:%08X lparam:%08X", msg.message, msg.wParam, msg.lParam);
			if (msg.wParam == 0x41) {	//按下a键推出 0x1b为esc
				break;
			}
		}
	}
	ClipCursor(NULL);
	ShowCursor(true);
	::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
	dlg.DestroyWindow();
	_endthreadex(0);

	return 0;
}

int LockMachine() {
	if ((dlg.m_hWnd != NULL) || (dlg.m_hWnd != INVALID_HANDLE_VALUE)) {
		//_beginthread(threadLockDlg, 0, NULL);
		_beginthreadex(NULL, 0, threadLockDlg, NULL, 0, &threadid);
		TRACE("threadid:%d\r\n", threadid);
	}
	
	CPacket pack(7, NULL, 0);
	CServerSocket::getInstance()->Send(pack);
	return 0;
}

int UnlockMachine() {
	//dlg.SendMessage(WM_KEYDOWN, 0x41, 0x01E0001);
	PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
	CPacket pack(8, NULL, 0);
	CServerSocket::getInstance()->Send(pack);

	return 0;
}


int TestConnect() {
	CPacket pack(1981, NULL, 0);
	bool ret = CServerSocket::getInstance()->Send(pack);
	TRACE("Send ret:%d\r\n", ret);

	return 0;
}

int DeleteLocalFile() {
	std::string strPath;
	CServerSocket::getInstance()->GetFilePath(strPath);
	TCHAR sPath[MAX_PATH] = _T("");
	//中文容易乱码，导致需要转换
	MultiByteToWideChar(
		CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
		sizeof(sPath) / sizeof(TCHAR));
	TRACE("DeleteFile[%s]\r\n", sPath);
	DeleteFile(sPath);
	
	CPacket pack(9, NULL, 0);
	bool ret = CServerSocket::getInstance()->Send(pack);
	TRACE("Send ret:%d\r\n", ret);
	return 0;
}

int ExcuteCommand(int nCmd) {
	//dlg.Create(IDD_DIALOG_INFO, NULL);
	int ret = 0;
	switch (nCmd) {
	case 1:		//查看磁盘分区
		ret = MakeDriverInfo();
		break;
	case 2:		//查看指定目录下的文件
		ret = MakeDirecteryInfo();
		break;
	case 3:		//打开文件
		ret = RunFIle();
		break;
	case 4:		//下载文件
		ret = DownloadFile();
		break;
	case 5:		//鼠标事件
		ret = MouseEvent();
		break;
	case 6:		//发送屏幕内容，本质与传屏幕快照类似
		ret = SendScreen();
		break;
	case 7:		//锁机
		ret = LockMachine();
		break;
	case 8:		//解锁机
		ret = UnlockMachine();
		break;
	case 9:		//删除文件
		ret = DeleteLocalFile();
		break;
	case 1981:
		ret = TestConnect();
		break;
	}
	return ret;
}

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
				TRACE("AcceptClient return true\r\n");
				int ret = pserver->DealCommand();
				TRACE("DealCommand ret %d\r\n", ret);
				//TODO:
				if (ret > 0) {
					ret = ExcuteCommand(ret);
					if (ret != 0) {
						TRACE("执行命令失败：%d ret=%d\r\n", pserver->GetPacket().sCmd, ret);
					}
					pserver->CloseClient();
					TRACE("Command bas done!\r\n");
				}
				
			} 
			
			
		}
	} else {
		// TODO: 更改错误代码以符合需要
		wprintf(L"错误: GetModuleHandle 失败\n");
		nRetCode = 1;
	}
	return nRetCode;
}
