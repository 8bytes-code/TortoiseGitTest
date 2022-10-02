
// RemoteClientDlg.h: 头文件
//

#pragma once
#include "ClientSocket.h"
#include "StatusDlg.h"

#define WM_SEND_PACKET (WM_USER + 1)	//发送数据包的消息

// CRemoteClientDlg 对话框
class CRemoteClientDlg : public CDialogEx
{
// 构造
public:
	CRemoteClientDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REMOTECLIENT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

//自定义
private:
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
	CString GetPath(HTREEITEM hTree);
	void DeleteTreeChildrenItem(HTREEITEM hTree);
	void LoadFileInfo();
	//应用为刷新目录
	void LoadFileCurrent();
	//线程优化
	static void threadEntryForDownFile(void* arg);
	void threadDownFile();
	//监控启用线程
	static void threadEntryForWatchData(void* arg);
	void threadWatchData();

private:
	//自定义缓存
	CImage m_image;
	bool m_isFull;		//校验缓存是否有数据，true有，false无
	bool m_isClosed;	//监视是否关闭
public:
	//为了别的类能够访问私有成员的缓存
	bool isFull() const {
		return m_isFull;
	}
	CImage& GetImage() {
		return m_image;
	}
	void SetImageStatus(bool isFull = false) {
		m_isFull = isFull;
	}

// 实现
protected:
	HICON m_hIcon;
	CStatusDlg m_dlgStatus;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedBtnTest();
    DWORD m_server_address;
    CString m_nPort;
    afx_msg void OnBnClickedBtnFileinfo();
	CTreeCtrl m_Tree;
    afx_msg void OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult);
	// 显示文件
	CListCtrl m_List;
	afx_msg void OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDownloadFile();
	afx_msg void OnDeleteFile();
	afx_msg void OnOpenFile();
	afx_msg LRESULT OnSendPacket(WPARAM wParam, LPARAM lParm);
	afx_msg void OnBnClickedBtnStratWatch();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
