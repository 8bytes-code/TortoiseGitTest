// WatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "WatchDialog.h"
#include "RemoteClientDlg.h"

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
{

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
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序


BOOL CWatchDialog::OnInitDialog() {
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
	SetTimer(0, 45, NULL);

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}


void CWatchDialog::OnTimer(UINT_PTR nIDEvent) {
	if (nIDEvent == 0) {
		CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent();
		//有数据才显示
		if (pParent->isFull()) {	
			CRect rect;
			m_picture.GetWindowRect(rect);

			//pParent->GetImage().BitBlt(m_picture.GetDC()->GetSafeHdc(), 0, 0, SRCCOPY);
			//窗口缩放
			pParent->GetImage().StretchBlt(
				m_picture.GetDC()->GetSafeHdc(), 0, 0, 
				rect.Width(), rect.Height(), SRCCOPY);
			//通知窗口进行重绘
			m_picture.InvalidateRect(NULL);
			//用完销毁
			pParent->GetImage().Destroy();
			//状态置为false
			pParent->SetImageStatus();
		}
	}

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

void CWatchDialog::OnSize(UINT nType, int cx, int cy) {
	CDialog::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
	ReSize();

}


void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point) {
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 0;	//左键
	event.nAction = 2;	//双击
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);

	CDialog::OnLButtonDblClk(nFlags, point);
}


void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point) {
	//坐标转换
// 	CPoint remote = UserPointRemoteScreenPoint(point);
// 	//封装	参照结构体
// 	MouseEvent event;
// 	event.ptXY = remote;
// 	event.nButton = 0;	//左键
// 	event.nAction = 3;	//按下
// 	CClientSocket* pClient = CClientSocket::getInstance();
// 	CPacket pack(5, (BYTE*)&event, sizeof(event));
// 	pClient->Send(pack);

	CDialog::OnLButtonDown(nFlags, point);
}


void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point) {
	//坐标转换
// 	CPoint remote = UserPointRemoteScreenPoint(point);
// 	//封装	参照结构体
// 	MouseEvent event;
// 	event.ptXY = remote;
// 	event.nButton = 0;	//左键
// 	event.nAction = 4;	//弹起
// 	CClientSocket* pClient = CClientSocket::getInstance();
// 	CPacket pack(5, (BYTE*)&event, sizeof(event));
// 	pClient->Send(pack);

	CDialog::OnLButtonUp(nFlags, point);
}


void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point) {
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 2;	//右键
	event.nAction = 2;	//双击
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);

	CDialog::OnRButtonDblClk(nFlags, point);
}


void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point) {
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 0;	//右键
	event.nAction = 3;	//单击
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);

	CDialog::OnRButtonDown(nFlags, point);
}


void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point) {
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 0;	//右键
	event.nAction = 4;	//弹起
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);

	CDialog::OnRButtonUp(nFlags, point);
}


void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point) {
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 0;	//
	event.nAction = 1;	//移动
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);

	CDialog::OnMouseMove(nFlags, point);
}

CPoint CWatchDialog::UserPointRemoteScreenPoint(CPoint& point) {
//client dlgSize 800*450
	CRect clientRect;
	ScreenToClient(&point);	//屏幕坐标到客户端坐标
//本地坐标到远程坐标
	m_picture.GetWindowRect(clientRect);
	//float width0 = clientRect.Width();
	//float height0 = clientRect.Height();
	//int width = 1920, hight = 1080;
	////转换
	//int x = point.x * width / width0;
	//int y = point.y * hight / height0;


	return CPoint(point.x * 1920 / clientRect.Width(), point.y * 1080 / clientRect.Height());
}


void CWatchDialog::OnStnClickedWatch() {
	CPoint point;
	GetCursorPos(&point);
	//坐标转换
	CPoint remote = UserPointRemoteScreenPoint(point);
	//封装	参照结构体
	MouseEvent event;
	event.ptXY = remote;
	event.nButton = 0;	//右键
	event.nAction = 3;	//单击
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket pack(5, (BYTE*)&event, sizeof(event));
	pClient->Send(pack);
}
