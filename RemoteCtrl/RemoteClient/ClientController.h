#pragma once
#include "ClientSocket.h"
#include "WatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include "resource.h"
#include <map>
#include "HeTool.h"

#define WM_SEND_PACK (WM_USER+1)	//发送包数据消息
#define WM_SEND_DATA (WM_USER+2)	//发送数据消息
#define WM_SHOW_STATUS (WM_USER+3)	//展示状态消息
#define WM_SHOW_WATCH (WM_USER+4)	//远程监控消息
#define WM_SEND_MESSAGE (WM_USER+0x1000) //自定义消息处理

class CClientController {
public:
	//获取全局唯一对象
	static CClientController* getInstance();
	//初始化
	int InitController();
	//启动
	int Invoke(CWnd*& pMainWnd);
	//发送消息
	LRESULT SendMessage(MSG msg);
	//更新网络服务器地址
	void UpdateAddress(int nIP, int nPort) {
		CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
	}
	//客户端接口移交控制层
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

	//1.查看磁盘分区
	//2.查看查看指定目录下的文件
	//3.打开文件
	//4.下载文件
	//5.鼠标操作
	//6.发送屏幕内容
	//7.锁机
	//8.解锁机
	//9.删除文件   
	//返回值为命令号,小于0则表示有错
	int SendCommandPacket(int nCmd, bool bAutoClose = true, BYTE* pData = NULL, size_t nLength = 0);

	int GetImage(CImage& image) {
		CClientSocket* pClient = CClientSocket::getInstance();
		return CHeTool::BytesToImage(image, pClient->GetPacket().strData);
	}

	int DownFile(CString strPath);

	void StartWatchScreen();
protected:
	//监控线程
	void threadWatchScreen();
	static void threadWatchScreenE(void* arg);
	//下载文件线程
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

	//自定义消息响应函数声明
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
	//消息响应队列
	typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
	static std::map<UINT, MSGFUNC> m_mapFunc;
	//三个dlg交给控制层
	CWatchDialog m_watchDlg;
	CRemoteClientDlg m_remoteDlg;
	CStatusDlg m_statusDlg;
	//线程和线程id
	HANDLE m_hThread;
	unsigned m_nThreadID;
	HANDLE m_hThreadDownload;
	HANDLE m_hThreadWatch;
	//监控是否关闭
	bool m_isClose;	
	static CClientController* m_instance;
	//下载文件的远程路径
	CString m_strRemote;
	//下载文件的本地保存路径
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

