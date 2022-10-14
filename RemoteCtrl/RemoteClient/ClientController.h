#pragma once
#include "ClientSocket.h"
#include "WatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include "resource.h"
#include <map>
#include "HeTool.h"

#define WM_SEND_PACK (WM_USER+1)	//���Ͱ�������Ϣ
#define WM_SEND_DATA (WM_USER+2)	//����������Ϣ
#define WM_SHOW_STATUS (WM_USER+3)	//չʾ״̬��Ϣ
#define WM_SHOW_WATCH (WM_USER+4)	//Զ�̼����Ϣ
#define WM_SEND_MESSAGE (WM_USER+0x1000) //�Զ�����Ϣ����

class CClientController {
public:
	//��ȡȫ��Ψһ����
	static CClientController* getInstance();
	//��ʼ��
	int InitController();
	//����
	int Invoke(CWnd*& pMainWnd);
	//������Ϣ
	LRESULT SendMessage(MSG msg);
	//���������������ַ
	void UpdateAddress(int nIP, int nPort) {
		CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
	}
	//�ͻ��˽ӿ��ƽ����Ʋ�
	int DealCommand() {
		return CClientSocket::getInstance()->DealCommand();
	}

	void CloseSocket() {
		CClientSocket::getInstance()->CloseSocket();
	}

	bool SendPacket(const CPacket& pack) {
		CClientSocket* pClient = CClientSocket::getInstance();
		if (pClient->InitSocket() == false)return false;
		pClient->Send(pack);
	}

	//1.�鿴���̷���
	//2.�鿴�鿴ָ��Ŀ¼�µ��ļ�
	//3.���ļ�
	//4.�����ļ�
	//5.������
	//6.������Ļ����
	//7.����
	//8.������
	//9.ɾ���ļ�   
	//����ֵΪ�����,С��0���ʾ�д�
	int SendCommandPacket(int nCmd, bool bAutoClose = true, BYTE* pData = NULL, size_t nLength = 0);

	int GetImage(CImage& image) {
		CClientSocket* pClient = CClientSocket::getInstance();
		return CHeTool::BytesToImage(image, pClient->GetPacket().strData);
	}

	int DownFile(CString strPath);

	void StartWatchScreen();
protected:
	//����߳�
	void threadWatchScreen();
	static void threadWatchScreenE(void* arg);
	//�����ļ��߳�
	void threadDownloadFile();
	static void threadDownloadEntry(void* arg);

	CClientController():
		m_statusDlg(&m_remoteDlg),
		m_watchDlg(&m_remoteDlg)
	{
		m_isClose = true;
		m_hThread = INVALID_HANDLE_VALUE;
		m_hThreadDownload = INVALID_HANDLE_VALUE;
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_nThreadID = -1;
	}

	~CClientController() {
		WaitForSingleObject(m_hThread, 100);
	}

	void threadFunc();
	static unsigned __stdcall threadEntry(void* arg);

	static void releaseInstance() {
		if (m_instance != NULL) {
			delete m_instance;
			m_instance = NULL;
		}
	}

	//�Զ�����Ϣ��Ӧ��������
	LRESULT OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam);

private:
	typedef struct MsgInfo{
		MSG msg;
		LRESULT result;

		MsgInfo(MSG m) {
			result = 0;
			memcpy(&msg, &m, sizeof(MSG));
		}
		MsgInfo(const MsgInfo& m) {
			result = m.result;
			memcpy(&msg, &m.msg, sizeof(MSG));
		}
		MsgInfo& operator=(const MsgInfo& m) {
			if (this != &m) {
				result = m.result;
				memcpy(&msg, &m.msg, sizeof(MSG));
			}
			return *this;
		}
	}MSGINFO;
	//��Ϣ��Ӧ����
	typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
	static std::map<UINT, MSGFUNC> m_mapFunc;
	//����dlg�������Ʋ�
	CWatchDialog m_watchDlg;
	CRemoteClientDlg m_remoteDlg;
	CStatusDlg m_statusDlg;
	//�̺߳��߳�id
	HANDLE m_hThread;
	unsigned m_nThreadID;
	HANDLE m_hThreadDownload;
	HANDLE m_hThreadWatch;
	//����Ƿ�ر�
	bool m_isClose;	
	static CClientController* m_instance;
	//�����ļ���Զ��·��
	CString m_strRemote;
	//�����ļ��ı��ر���·��
	CString m_strLocal;

	class CHelper {
	public:
		CHelper() {
			//CClientController::getInstance();
		}
		~CHelper() {
			CClientController::releaseInstance();
		}
	};

	static CHelper m_helper;
};

