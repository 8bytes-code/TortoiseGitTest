#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"


//静态成员在头文件没有只有声明没有实现，故此要在源文件初始化
//除此之外，m_instance是唯一的对象接口，不能再每次构造都被重置
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;
CClientController* CClientController::m_instance = NULL;
CClientController::CHelper CClientController::m_helper;

CClientController* CClientController::getInstance() {
	if (m_instance == NULL) {
		m_instance = new CClientController();
		TRACE("CClientController size is %d\r\n", sizeof(*m_instance));
		struct {
			UINT nMsg;
			MSGFUNC func;
		}MsgFuncs[]={
			//{WM_SEND_PACK, &CClientController::OnSendPack},
			//{WM_SEND_DATA, &CClientController::OnSendData},
			{WM_SHOW_STATUS, &CClientController::OnShowStatus},
			{WM_SHOW_WATCH, &CClientController::OnShowWatch},
			{(UINT)-1, NULL}
		};

		for (int i = 0; MsgFuncs[i].func != NULL; i++) {
			m_mapFunc.insert(std::pair<UINT, MSGFUNC>(MsgFuncs[i].nMsg, MsgFuncs[i].func));
		}
	}
	return m_instance;
}


int CClientController::InitController() {
	m_hThread = (HANDLE)_beginthreadex(
		NULL, 0, &CClientController::threadEntry,
		this, 0, &m_nThreadID
	);	//CreateThread

	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);

	return 0;
}


int CClientController::Invoke(CWnd*& pMainWnd) {
	pMainWnd = &m_remoteDlg;
	return m_remoteDlg.DoModal();
}


LRESULT CClientController::SendMessage(MSG msg) {
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL)return-2;
	MSGINFO info(msg);
	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)hEvent);
	//一直等待信号
	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);
	return info.result;
}


bool CClientController::SendCommandPacket(HWND hWnd, int nCmd, bool bAutoClose, BYTE* pData, size_t nLength, WPARAM wParam){
	TRACE("cmd:%d %s start %lld \r\n", nCmd, __FUNCTION__, GetTickCount64());
	CClientSocket* pClient = CClientSocket::getInstance();
	bool ret = pClient->SendPacket(hWnd, CPacket(nCmd, pData, nLength), bAutoClose, wParam);
	return ret;
}


void CClientController::DownloadEnd() {
	m_statusDlg.ShowWindow(SW_SHOW);
	m_statusDlg.CenterWindow(&m_remoteDlg);
	m_statusDlg.SetActiveWindow();
}

int CClientController::DownFile(CString strPath) {
	CFileDialog dlg(FALSE, NULL, strPath,
	OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
	NULL, &m_remoteDlg);	//构造起来有点麻烦

	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();
		FILE* pFile = fopen(m_strLocal, "wb+");
		if (pFile == NULL) {
			AfxMessageBox(_T("本地没有权限保存该文件或者文件无法创建!!!"));
			return -1;
		}
		SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
// 		//获取线程，以免强制关闭
// 		m_hThreadDownload = (HANDLE)_beginthread(&CClientController::threadDownloadEntry, 0, this);
// 		//通过等待信号判断线程是否成功创建
// 		if (WaitForSingleObject(m_hThreadDownload, 0) != WAIT_TIMEOUT) {
// 			return -1;
// 		}

		m_remoteDlg.BeginWaitCursor();
		m_statusDlg.m_info.SetWindowText(_T("命令正在执行中！"));
		m_statusDlg.ShowWindow(SW_SHOW);
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow();
	}

	return 0;
}


void CClientController::StartWatchScreen() {
	m_isClose = false;
	//设置父类窗口
	//m_watchDlg.SetParent(&m_remoteDlg);
	m_hThreadWatch = (HANDLE)_beginthread(&CClientController::threadWatchScreen, 0, this);
	m_watchDlg.DoModal();
	m_isClose = true;
	WaitForSingleObject(m_hThreadWatch, 500);
}


void CClientController::threadWatchScreen() {
	Sleep(50);
	ULONGLONG nTick = GetTickCount64();
	while (!m_isClose) {
		if (m_watchDlg.isFull() == false) {
			if (GetTickCount64() - nTick < 200) {
				Sleep(200 - DWORD(GetTickCount64() - nTick));
			}
			nTick = GetTickCount64();
			int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(), 6, true, NULL, 0);
			//TODO: 添加消息响应函数WM_SEND_PACK_ACK
			if (ret == 1) {
				//TRACE("成功发送请求图片命令\r\n");
			}
			else {
				TRACE("图片获取失败，ret=%d\r\n", ret);
			}
		}
		Sleep(1);
	}
	TRACE("thread end %d\r\n", m_isClose);
}


void CClientController::threadWatchScreen(void* arg) {
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}


void CClientController::threadDownloadFile() {
	FILE* pFile = fopen(m_strLocal, "wb+");
	if (pFile == NULL) {
		AfxMessageBox(_T("本地没有权限保存该文件或者文件无法创建!!!"));
		m_statusDlg.ShowWindow(SW_HIDE);	//隐藏窗口
		m_remoteDlg.EndWaitCursor();		//让光标结束等待状态
		return;
	}
	CClientSocket* pClient = CClientSocket::getInstance();
	do {
		int ret = SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
		
		long long nLength = *(long long*)pClient->GetPacket().strData.c_str();
		if (nLength == 0) {
			AfxMessageBox("文件长度为0或者无法读取文件!!!");
			break;
		}
		long long nCount = 0;
		while (nCount < nLength) {
			ret = pClient->DealCommand();
			if (ret < 0) {
				AfxMessageBox("传输失败");
				TRACE("文件传输失败:ret=%d\r\n", ret);
				break;
			}
			fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
			nCount += pClient->GetPacket().strData.size();
		}
	} while (false);

	fclose(pFile);
	pClient->CloseSocket();
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成!!"), _T("完成"));
}


void CClientController::threadDownloadEntry(void* arg) {
	CClientController* thiz = (CClientController*)arg;
	thiz->threadDownloadFile();
	_endthread();
}


void CClientController::threadFunc() {
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_SEND_MESSAGE) {
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
			HANDLE hEvent = (HANDLE)msg.lParam;

			//老样子消息循环队列完事就给他挪出去
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			} else {
				//-1表示不存在
				pmsg->result = -1;
			}
			SetEvent(hEvent);
		} else {
			//没有对应消息则不处理
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end()) {
				(this->*it->second)(msg.message, msg.wParam, msg.lParam);
			}
		}

	}
}


unsigned __stdcall CClientController::threadEntry(void* arg) {
	CClientController* thiz = (CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);

	return 0;
}


/*
LRESULT CClientController::OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam) {
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket* pPacket = (CPacket*)wParam;
	return pClient->Send(*pPacket);
}


LRESULT CClientController::OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam) {
	CClientSocket* pClient = CClientSocket::getInstance();
	char* pBuffer = (char*)wParam;
	return pClient->Send(pBuffer, (int)lParam);
}
*/


LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam) {
	return m_statusDlg.ShowWindow(SW_SHOW);
}


LRESULT CClientController::OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam) {
	return m_watchDlg.DoModal();
}


/*
* 准备了事件、消息、result
* 通过PostThreadMessage发送到消息循环，等待result填充内容，同时wait等待setEvent
* event执行完成继续等待下一个消息
* 设计小结：因为由线程启动，消息执行顺序有一定出入，
* 但是需要知道是否完成，所以需要事件做处理
*/