#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrInfo(int wsaErrCode) {
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL
	);
	ret = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return ret;
}

void Dump(BYTE* pData, size_t nSize) {
	std::string strOut;
	for (size_t i = 0; i < nSize; i++) {
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0)) strOut += "\n";
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}

bool CClientSocket::InitSocket() {
	if (m_sock != INVALID_SOCKET) CloseSocket();
	m_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_sock == -1)return false;

	//网络参数
	sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	//TRACE("addr:%08X nIP:08X\r\n", inet_addr("127.0.0.1"), nIP);
	serv_adr.sin_addr.s_addr = htonl(m_nIP);	//需要转换，否则顺序倒置
	serv_adr.sin_port = htons(m_nPort);

	if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
		AfxMessageBox("指定的IP地址不存在！");
		return false;
	}

	int ret = connect(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr));
	if (ret == -1) {
		AfxMessageBox("连接失败!");
		TRACE("连接失败:%d %s\r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
		return false;
	}

	return true;
}

CClientSocket::CClientSocket(const CClientSocket& ss) {
	m_hThread = INVALID_HANDLE_VALUE;
	m_bAutoClose = ss.m_bAutoClose;
	m_sock = ss.m_sock;
	m_nIP = ss.m_nIP;
	m_nPort = ss.m_nPort;
	std::map<UINT, MSGFUNC>::const_iterator it = ss.m_mapFunc.begin();
	for (; it != ss.m_mapFunc.end(); it++) {
		m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
	}
}

CClientSocket::CClientSocket() :m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET),
m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE) {
	if (InitSockEnv() == FALSE) {
		MessageBox(NULL, _T("无法初始化套接字环境，请检查网络设置！"), _T("初始化错误!"), MB_OK | MB_ICONERROR);
		exit(0);
	}
	m_buffer.resize(BUFFER_SIZE);
	memset(m_buffer.data(), 0, BUFFER_SIZE);

	struct {
		UINT message;
		MSGFUNC func;
	}funcs[] = {
		{WM_SEND_PACK,&CClientSocket::SendPack},
		{0,NULL}
	};
	for (int i = 0; funcs[i].message != 0; i++) {
		if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false) {
			TRACE("插入失败，消息值：%d，函数值：%08X，序号：%d\r\n", funcs[i].message, funcs[i].func, i);
		}
	}
}

bool CClientSocket::SendPacket(HWND hWnd, CPacket& pack, bool isAutoClosed) {
	if (m_hThread == INVALID_HANDLE_VALUE) {
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
	}
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);
	return PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)new PACKET_DATA(strOut.c_str(), strOut.size(), nMode), (LPARAM)hWnd);
}

/*
bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed ) {
	if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
		//if (InitSocket() == false) return false;
		m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
	}
	//利用互斥体保证数据push_bak完整有序
	m_lock.lock();
	auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
	//添加状态队列
	m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
	m_lstSend.push_back(pack);
	m_lock.unlock();
	WaitForSingleObject(pack.hEvent, INFINITE);
	std::map<HANDLE, std::list<CPacket>&>::iterator it;
	it = m_mapAck.find(pack.hEvent);
	if (it != m_mapAck.end()) {
		m_lock.lock();
		m_mapAck.erase(it);
		m_lock.unlock();
		return true;
	}
	return false;
}
*/

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区长度*/) {
	//防止多层导致数据不对头或者泄露,预先保留成局部变量供后续使用
	PACKET_DATA data = *(PACKET_DATA*)wParam;
	delete (PACKET_DATA*)wParam;
	HWND hWnd = (HWND)lParam;
	if (InitSocket() == true) {
		int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
		if (ret > 0) {
			size_t index = 0;
			std::string strBuffer;
			strBuffer.resize(BUFFER_SIZE);
			char* pBuffer = (char*)strBuffer.c_str();

			while (m_sock != INVALID_SOCKET) {
				int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
				if (length > 0 || (index > 0)) {
					index += (size_t)length;
					size_t nLen = index;
					CPacket pack((BYTE*)pBuffer, nLen);
					//解包成功
					if (nLen > 0) {
						//TODO: 期许回调函数的应答交给HWND解决
						::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), 0);
						if (data.nMode & CSM_AUTOCLOSE) {
							CloseSocket();
							return;
						}
					}
					//缓冲区往后挪
					index -= nLen;
					memmove(pBuffer, pBuffer + index, nLen);
				} 
				else {
					//意料之外就噶了他
					CloseSocket();
					::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, 1);
				}
			}
		} 
		else {
			CloseSocket();
			::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
		}
	} 
	else {
		::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
	}
}

unsigned CClientSocket::threadEntry(void* arg) {
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc2();
	_endthreadex(0);
	return 0;
}

/*
void CClientSocket::threadFunc() {
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;

	InitSocket();
	while (m_sock != INVALID_SOCKET) {
		if (m_lstSend.size() > 0) {
			TRACE("lstSend size=%d\r\n", m_lstSend.size());
			
			m_lock.lock();
			CPacket& head = m_lstSend.front();
			m_lock.unlock();
			if (Send(head) == false) {
				TRACE("threadFunc Send Error!\r\n");
				continue;
			}		
			
			std::map<HANDLE, std::list<CPacket>&>::iterator it;
			it = m_mapAck.find(head.hEvent);
			//还要预防it挪到end之后
			if (it != m_mapAck.end()) {
				std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					if (length > 0 || (index > 0)) {
						index += length;
						size_t size = (size_t)index;
						CPacket pack((BYTE*)pBuffer, size);

						if (size > 0) {
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);
							//监视缓冲区，图片处于其中，当index指向没有改变，
							//就一直重复第一张图，但是数据是一直在接受，因为没有在缓冲区移动
							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;
							if (it0->second) {
								SetEvent(head.hEvent);
								break;
							}
						}
					} else if (length <= 0 && index <= 0) {
						CloseSocket();
						//等到服务器结束命令之后在通知事件完成
						SetEvent(head.hEvent);

						if (it0 != m_mapAutoClosed.end()) {
							//m_mapAutoClosed.erase(it0);
						} else {
							TRACE("error, erase not found pair！\r\n");
						}
						break;
					}
					//只要close没有false掉就持续recv
				} while (it0->second == false);
			}
			m_lock.lock();
			m_lstSend.pop_front();
			m_mapAutoClosed.erase(head.hEvent);
			m_lock.unlock();
			if (InitSocket() == false) {
				InitSocket();
			}
		} else {
			Sleep(1);
		}
	}
	CloseSocket();
}
*/

void CClientSocket::threadFunc2() {
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
			
		}
	}
}
