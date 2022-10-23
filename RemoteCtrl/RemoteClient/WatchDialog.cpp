// WatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "WatchDialog.h"
#include "ClientController.h"

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
{
	m_nObjHeight = -1;
	m_nObjWidth = -1;
}

CWatchDialog::~CWatchDialog()
{
}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WATCH, m_picture);
}


BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock)
	ON_MESSAGE(WM_SEND_PACK_ACK,&CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序


BOOL CWatchDialog::OnInitDialog() {
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
	//SetTimer(0, 45, NULL);
	//缓存状态
	m_isFull = false;

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}


void CWatchDialog::OnTimer(UINT_PTR nIDEvent) {
// 	if (nIDEvent == 0) {
// 		CClientController* pParent = CClientController::getInstance();
// 		//有数据才显示
// 		if (m_isFull) {	
// 			CRect rect;
// 			m_picture.GetWindowRect(rect);
// 			m_nObjWidth = m_image.GetWidth();
// 			m_nObjHeight = m_image.GetHeight();
// 
// 			//窗口缩放
// 			m_image.StretchBlt(
// 				m_picture.GetDC()->GetSafeHdc(), 0, 0, 
// 				rect.Width(), rect.Height(), SRCCOPY);
// 			//通知窗口进行重绘
// 			m_picture.InvalidateRect(NULL);
// 			//用完销毁
// 			m_image.Destroy();
// 			//状态置为false
// 			m_isFull = false;
// 		}
// 	}

	CDialog::OnTimer(nIDEvent);
}

void CWatchDialog::ReSize() {
	float fsp[2];
	POINT Newp; //获取现在对话框的大小  
	CRect recta;
	GetClientRect(&recta);     //取客户区大小    
	Newp.x = recta.right - recta.left;
	Newp.y = recta.bottom - recta.top;
	fsp[0] = (float)Newp.x / old.x;
	fsp[1] = (float)Newp.y / old.y;
	CRect Rect;
	int woc;
	CPoint OldTLPoint, TLPoint; //左上角  
	CPoint OldBRPoint, BRPoint; //右下角  
	HWND  hwndChild = ::GetWindow(m_hWnd, GW_CHILD);  //列出所有控件    
	while (hwndChild) {
		woc = ::GetDlgCtrlID(hwndChild);//取得ID  
		GetDlgItem(woc)->GetWindowRect(Rect);
		ScreenToClient(Rect);
		OldTLPoint = Rect.TopLeft();
		TLPoint.x = long(OldTLPoint.x * fsp[0]);
		TLPoint.y = long(OldTLPoint.y * fsp[1]);
		OldBRPoint = Rect.BottomRight();
		BRPoint.x = long(OldBRPoint.x * fsp[0]);
		BRPoint.y = long(OldBRPoint.y * fsp[1]);
		Rect.SetRect(TLPoint, BRPoint);
		GetDlgItem(woc)->MoveWindow(Rect, TRUE);
		hwndChild = ::GetWindow(hwndChild, GW_HWNDNEXT);
	}
	old = Newp;
}

LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam) {
	if (lParam == -1 || lParam == -2) {
		//错误
	}
	else if (lParam == 1) {
		//对方关闭套接字
	}
	else {
		CPacket* pPacket = (CPacket*)wParam;
		if (pPacket != NULL) {
			CPacket head = *(CPacket*)wParam;
			delete (CPacket*)wParam;
			switch (head.sCmd) {
				case 6:
				{
					CHeTool::BytesToImage(m_image, head.strData);
					CRect rect;
					m_picture.GetWindowRect(rect);
					m_nObjWidth = m_image.GetWidth();
					m_nObjHeight = m_image.GetHeight();
					//窗口缩放
					m_image.StretchBlt(m_picture.GetDC()->GetSafeHdc(), 0, 0,
						rect.Width(), rect.Height(), SRCCOPY);
					//通知窗口进行重绘
					m_picture.InvalidateRect(NULL);
					//用完销毁
					m_image.Destroy();
					//状态置为false
					m_isFull = false;
				}
				break;

				case 5:
					TRACE("鼠标操作应答!\r\n");
					break;
				case 7:
				case 8:
				default:
					break;
			}
		}
	}
	
	return 0;
}

void CWatchDialog::OnSize(UINT nType, int cx, int cy) {
	CDialog::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
	//ReSize();

}


void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;	//左键
		event.nAction = 2;	//双击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnLButtonDblClk(nFlags, point);
}


void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;	//左键
		event.nAction = 2;	//按下

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDown(nFlags, point);
}


void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;	//左键
		event.nAction = 3;	//弹起
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonUp(nFlags, point);
}


void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;	//右键
		event.nAction = 1;	//双击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonDblClk(nFlags, point);
}


void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;	//右键
		event.nAction = 2;	//按下
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonDown(nFlags, point);
}


void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;	//右键
		event.nAction = 3;	//弹起
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonUp(nFlags, point);
}


void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point) {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 8;	//没有按键，鼠标移动
		event.nAction = 0;	//移动
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnMouseMove(nFlags, point);
}

CPoint CWatchDialog::UserPointRemoteScreenPoint(CPoint& point, bool isScreen) {
//client dlgSize 800*450
	CRect clientRect;
	if (!isScreen) {
		ClientToScreen(&point);				//先将坐标转换成监视窗口的坐标
	}
	m_picture.ScreenToClient(&point);	//再将坐标转换成控件区域的坐标
	TRACE("x=%d y=%d\r\n", point.x, point.y);

	//本地坐标到远程坐标
	m_picture.GetWindowRect(clientRect);
	//TRACE("x=%d y=%d\r\n", clientRect.Width(), clientRect.Height());

	return CPoint(point.x * m_nObjWidth / clientRect.Width(), point.y * m_nObjHeight / clientRect.Height());
}


void CWatchDialog::OnStnClickedWatch() {
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint point;
		GetCursorPos(&point);
		//坐标转换
		CPoint remote = UserPointRemoteScreenPoint(point, true);
		//封装	参照结构体
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;	//左键
		event.nAction = 0;	//单击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
}


void CWatchDialog::OnOK() {
	// TODO: 在此添加专用代码和/或调用基类

	//CDialog::OnOK();
}


void CWatchDialog::OnBnClickedBtnLock() {
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);
}


void CWatchDialog::OnBnClickedBtnUnlock() {
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);
}
