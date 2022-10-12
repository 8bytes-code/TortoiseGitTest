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
//#pragma warning(disable:4966) Ϊһ�ֽ��þ��棬�����Щ�Ϻ�������ȫ��ʱ��

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
				TRACE("ִ������ʧ�ܣ�%d ret=%d\r\n", status, ret);
			}
		} else {
			MessageBox(NULL, _T("�޷����������û������Ժ������Զ����ԣ�"), _T("�����û�ʧ��"), MB_OK | MB_ICONERROR);
		}
			
	}

protected:
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket& inPacket);			//��Ա����ָ��
	std::map<int, CMDFUNC> m_mapFunction;		//������ŵ����ܵ�ӳ��
	CLockDialog dlg;
	unsigned threadid;

protected:
	//��������Ŀ¼
	int MakeDriverInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//1==>A  2==>B ��ǰ�ϰ汾Ϊ���̣��ʴ����ڵķ������Ǵ�C��ʼ...26==>Z
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

	//�����ļ�Ŀ¼
	int MakeDirecteryInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//����getfilepathʵ���Ͼ��Ǹ�������ȡ��packet��strdata������ʱ�Ѿ���������ʴ�ֱ��=
		std::string strPath = inPacket.strData;

		TRACE("cd C is %d\r\n", _chdir("C:"));
		if (_chdir(strPath.c_str()) != 0) {
			TRACE("cd strPath:[%s] is %d\r\n", strPath.c_str(), _chdir(strPath.c_str()));
			FILEINFO finfo;
			finfo.HasNext = FALSE;
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			OutputDebugString(_T("û��Ȩ�޷���Ŀ¼!!"));
			return -2;
		}

		_finddata_t fdata;
		int hfind = 0;
		if ((hfind = _findfirst("*", &fdata)) == -1) {
			//*��ʾƥ�������ļ����������-1���ʾû���ҵ�
			OutputDebugString(_T("û���ҵ��κ��ļ�!!"));
			FILEINFO finfo;
			finfo.HasNext = FALSE;
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			return -3;
		}

		int Count = 0;	//����ʱ�ļ���
		do {
			FILEINFO finfo;
			finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
			memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
			TRACE("%s\r\n", finfo.szFileName);
			lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			Count++;
		} while (!_findnext(hfind, &fdata));
		TRACE("server:count=%d\r\n", Count);

		//������Ϣ�����ƶ�
		FILEINFO finfo;
		finfo.HasNext = FALSE;
		lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));

		return 0;
	}


	//���ļ�
	int RunFIle(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//����getfilepathʵ���Ͼ��Ǹ�������ȡ��packet��strdata������ʱ�Ѿ���������ʴ�ֱ��=
		std::string strPath = inPacket.strData;
		ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
		lstPacket.push_back(CPacket(3, NULL, 0));

		return 0;
	}


	//�����ļ�
	int DownloadFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		/*
		 * ���Ȼ�ȡ·��
		 * Ȼ����ļ������ʧ�ܾͽ���
		 * �ܴ򿪾Ϳ�ʼ��ȡȻ����ֱ��Ϊ��
		 * ���ر�
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
			//�Ż��������������ļ�����ʱ�յ�
			fseek(pFile, 0, SEEK_END);
			data = _ftelli64(pFile);
			lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
			fseek(pFile, 0, SEEK_SET);

			char buffer[1024] = "";
			size_t rlen = 0;
			do {
				rlen = fread(buffer, 1, 1024, pFile);				//ÿ�ζ�1�ֽڣ���1024��
				lstPacket.push_back(CPacket(4, (BYTE*)&buffer, rlen));	//����֮����
			} while (rlen >= 1024);
			fclose(pFile);
		} else {
			//ѭ�������������ǿմ������
			lstPacket.push_back(CPacket(4, NULL, 0));
		}

		return 0;
	}


	//������
	int MouseEvent(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		MOUSEEV mouse;
		//����ԭ��server��getmouseevent
		memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
	
		DWORD nFlags = 0;
		switch (mouse.nButton) {
			case 0:		//���
				nFlags = 1;
				break;
			case 1:		//�Ҽ�
				nFlags = 2;
				break;
			case 2:		//�м�
				nFlags = 4;
				break;
			case 4:		//û�а���
				nFlags = 8;
				break;
		}

		if (nFlags != 8) SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
		switch (mouse.nAction) {
			case 0:		//����
				nFlags |= 0x10;
				break;
			case 1:		//˫��
				nFlags |= 0x20;
				break;
			case 2:		//����
				nFlags |= 0x40;
				break;
			case 3:		//�ſ�
				nFlags |= 0x80;
				break;
			default:break;
		}
		TRACE("mouse event: %08X x%d y %d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);
		switch (nFlags) {
			case 0x21:	//���˫��
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x11:	//�������
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x41:	//�������
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x81:	//����ɿ�
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x22:	//�Ҽ�˫��
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x12:	//�Ҽ�����
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x42:	//�Ҽ�����
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x82:	//�Ҽ��ɿ�
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x24:	//�м�˫��
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			case 0x14:	//�м�����
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x44:	//�м�����
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x84:	//�м��ɿ�
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
				break;
			case 0x08:	//����������ƶ�
				mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
				break;
		}

		lstPacket.push_back(CPacket(5, NULL, 0));

		return 0;
	}


	//ͨ��������ȡԶ������
	int SendScreen(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		CImage screen;	//GDI
		HDC hScreen = ::GetDC(NULL);

		//��ȡλ��, RGB/888=24 ARGB/8888=32
		int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);
		int nWidth = GetDeviceCaps(hScreen, HORZRES);
		int nHeight = GetDeviceCaps(hScreen, VERTRES);

		//�������ڣ����ջ�ȡ�Ŀ�͸߻���λ��
		screen.Create(nWidth, nHeight, nBitPerPixel);
		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
		ReleaseDC(NULL, hScreen);

		//����ʱ�õ���ͼƬ�ļ���������������Ҫת��������������
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


	//�����̴߳�����������
	static unsigned _stdcall threadLockDlg(void* arg) {
		CCommand* thiz = (CCommand*)arg;
		thiz->threadLockDlgMain();
		_endthreadex(0);
		return 0;
	}


	//�����������Ĵ���
	void threadLockDlgMain() {
		//�����Ի�����ʾ�����ɵ�����С|�����ƶ�
		dlg.Create(IDD_DIALOG_INFO, NULL);
		dlg.ShowWindow(SW_SHOW);
		//�ڱκ����Ӧ��
		CRect rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
		rect.bottom = LONG(rect.bottom * 1.10);
		dlg.MoveWindow(rect);

		//��������΢�������м�
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

		//�����ö�
		dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
		//�������
		ShowCursor(false);
		//����������
		::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);

		dlg.GetWindowRect(rect);
		rect.left = 0;
		rect.top = 0;
		rect.right = 1;
		rect.bottom = 1;
		ClipCursor(rect);			//�������Ļ��Χ

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_KEYDOWN) {
				TRACE("msg:%08X wparam:%08X lparam:%08X", msg.message, msg.wParam, msg.lParam);
				if (msg.wParam == 0x41) {	//����a���Ƴ� 0x1bΪesc
					break;
				}
			}
		}
		ClipCursor(NULL);
		ShowCursor(true);
		::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
		dlg.DestroyWindow();
	}


	//����
	int LockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		if ((dlg.m_hWnd != NULL) || (dlg.m_hWnd != INVALID_HANDLE_VALUE)) {
			//_beginthread(threadLockDlg, 0, NULL);
			_beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
			TRACE("threadid:%d\r\n", threadid);
		}

		lstPacket.push_back(CPacket(7, NULL, 0));
		return 0;
	}


	//������
	int UnlockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		//dlg.SendMessage(WM_KEYDOWN, 0x41, 0x01E0001);
		PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
		lstPacket.push_back(CPacket(8, NULL, 0));

		return 0;
	}


	//��������
	int TestConnect(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		lstPacket.push_back(CPacket(1981, NULL, 0));
		return 0;
	}


	//ɾ���ļ�
	int DeleteLocalFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		TCHAR sPath[MAX_PATH] = _T("");
		//�����������룬������Ҫת��
		MultiByteToWideChar(
			CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
			sizeof(sPath) / sizeof(TCHAR));
		TRACE("DeleteFile[%s]\r\n", sPath);
		DeleteFile(sPath);

		lstPacket.push_back(CPacket(9, NULL, 0));
		return 0;
	}
};

