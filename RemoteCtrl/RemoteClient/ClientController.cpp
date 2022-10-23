#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"


//��̬��Ա��ͷ�ļ�û��ֻ������û��ʵ�֣��ʴ�Ҫ��Դ�ļ���ʼ��
//����֮�⣬m_instance��Ψһ�Ķ���ӿڣ�������ÿ�ι��춼������
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


bool CClientController::SendCommandPacket(HWND hWnd, int nCmd, bool bAutoClose, BYTE* pData, size_t nLength, WPARAM wParam){
	TRACE("cmd:%d %s start %lld \r\n", nCmd, __FUNCTION__, GetTickCount64());
	CClientSocket* pClient = CClientSocket::getInstance();
	bool ret = pClient->SendPacket(hWnd, CPacket(nCmd, pData, nLength), bAutoClose, wParam);
	return ret;
}


void CClientController::DownloadEnd() {
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("�������!!!"), _T("���"));
}

int CClientController::DownFile(CString strPath) {
	CFileDialog dlg(FALSE, NULL, strPath,
	OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
	NULL, &m_remoteDlg);	//���������е��鷳

	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();
		FILE* pFile = fopen(m_strLocal, "wb+");
		if (pFile == NULL) {
			AfxMessageBox(_T("����û��Ȩ�ޱ�����ļ������ļ��޷�����!!!"));
			return -1;
		}
		SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);

		m_remoteDlg.BeginWaitCursor();
		m_statusDlg.m_info.SetWindowText(_T("��������ִ���У�"));
		m_statusDlg.ShowWindow(SW_SHOW);
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow();
	}

	return 0;
}


void CClientController::StartWatchScreen() {
	m_isClose = false;
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
			if (ret == 1) {
				//TRACE("�ɹ���������ͼƬ����\r\n");
			}
			else {
				TRACE("ͼƬ��ȡʧ�ܣ�ret=%d\r\n", ret);
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


void CClientController::threadFunc() {
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_SEND_MESSAGE) {
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
			HANDLE hEvent = (HANDLE)msg.lParam;

			//��������Ϣѭ���������¾͸���Ų��ȥ
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			} else {
				//-1��ʾ������
				pmsg->result = -1;
			}
			SetEvent(hEvent);
		} else {
			//û�ж�Ӧ��Ϣ�򲻴���
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
* ׼�����¼�����Ϣ��result
* ͨ��PostThreadMessage���͵���Ϣѭ�����ȴ�result������ݣ�ͬʱwait�ȴ�setEvent
* eventִ����ɼ����ȴ���һ����Ϣ
* ���С�᣺��Ϊ���߳���������Ϣִ��˳����һ�����룬
* ������Ҫ֪���Ƿ���ɣ�������Ҫ�¼�������
*/