#pragma once
#include "afxdialogex.h"


// CWatchDialog 对话框

class CWatchDialog : public CDialog
{
	DECLARE_DYNAMIC(CWatchDialog)

public:
	CWatchDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CWatchDialog();
	POINT old;
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_WATCH };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	CStatic m_picture;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	void ReSize();
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);

public:
	//自定义，当窗口缩放在监控的时候坐标才需要转换，而鼠标消息在窗口中是不需要在进行缩放
	CPoint UserPointRemoteScreenPoint(CPoint& point, bool isScreen = false);
	afx_msg void OnStnClickedWatch();
	int m_nObjWidth;
	int m_nObjHeight;
	virtual void OnOK();
	afx_msg void OnBnClickedBtnLock();
	afx_msg void OnBnClickedBtnUnlock();
};
