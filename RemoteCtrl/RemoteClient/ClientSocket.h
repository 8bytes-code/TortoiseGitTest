#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>

#pragma pack(push)
#pragma pack(1)
class CPacket {
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4;
		sCmd = nCmd;

		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		} else {
			strData.clear();
		}

		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	CPacket(const BYTE* pData, size_t& nSize) {
		size_t i = 0;
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				//i+=2 避免空包时读到后面的数据
				i += 2;
				break;
			}
		}

		//此处长度参考包的设计
		if ((i + 4 + 2 + 2) > nSize) {	//包数据可能不全，或者包头未能全部接收到
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {		//包未完全接受，解析失败就返回
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);  i += 2;

		if (nLength > 4) {
			strData.resize(nLength - 2 - 2);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
		}

		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sum += BYTE(strData[j]) & 0xFF;
		}

		if (sum == sSum) {
			nSize = i;	//head length...
			return;
		}

		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator=(const CPacket& pack) {
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}

	int Size() {	//包数据的大小
		return nLength + 2 + 4;
	}

	const char* Data(std::string& strOut) const{
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)pData = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;

		return strOut.c_str();
	}
public:
	WORD sHead;				//包头			2
	DWORD nLength;			//包长度			4	
	WORD sCmd;				//控制命令		2
	std::string strData;	//包数据
	WORD sSum;				//校验：和校验	2
	//std::string strOut;		//整个包数据
};
#pragma pack(pop)

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}

	WORD nAction;	//点击、移动、双击
	WORD nButton;	//左键、右键、中键
	POINT ptXY;		//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	//c++中结构体也有构造函数，与类不同在于结构体默认都是public，而类默认是private
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;			//是否有效
	BOOL IsDirectory;		//是否为目录 0否 1是
	BOOL HasNext;			//是否还有后续文件
	char szFileName[256];	//文件名 0无 1有
}FILEINFO, * PFILEINFO;


std::string GetErrInfo(int wsaErrCode);

class CClientSocket {
public:
	static CClientSocket* getInstance() {
		//由于静态函数没有this指针无法直接调用
		if (m_instance == NULL) {
			m_instance = new CClientSocket();
		}
		return m_instance;
	}

	bool InitSocket() {
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


#define BUFFER_SIZE 4194304		//1024*1024*4扩大缓冲区
	int DealCommand() {
		if (m_sock == -1)return -1;

		char* buffer = m_buffer.data();
		static size_t index = 0;

		while (true) {
			size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
			if ((len <= 0) && (index <= 0)) {
				return -1;
			}
			//TRACE("recv:%d\r\n", len);
			index += len;
			len = index;

			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, index - len);
				index -= len;
				return m_packet.sCmd;
			}
		}
		return -1;
	}

	bool Send(const char* pData, int nSize) {
		if (m_sock == -1)return false;
		return send(m_sock, pData, nSize, 0) > 0;
	}

	bool Send(const CPacket& pack) {
		if (m_sock == -1)return false;
		std::string strOut;
		pack.Data(strOut);
		return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
	}

	bool GetFilePath(std::string& strPath) {
		//当前命令为2-4才是去执行获取文件列表
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}

	bool GetMouseEvent(MOUSEEV& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}

		return false;
	}

	CPacket& GetPacket() {
		return m_packet;
	}
	
	void CloseSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	void UpdateAddress(int nIP, int nPort) {
		m_nIP = nIP;
		m_nPort = nPort;
	}
private:
	int m_nIP;		//地址
	int m_nPort;	//端口
	std::vector<char> m_buffer;
	SOCKET m_sock;
	CPacket m_packet;

	CClientSocket():m_nIP(INADDR_ANY), m_nPort(0) {
		if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境，请检查网络设置！"), _T("初始化错误!"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_buffer.resize(BUFFER_SIZE);
		memset(m_buffer.data(), 0, BUFFER_SIZE);
	}
	CClientSocket(const CClientSocket& ss){
		m_sock = ss.m_sock;
		m_nIP = ss.m_nIP;
		m_nPort = ss.m_nPort;
	}
	CClientSocket& operator=(const CClientSocket& ss) {}

	~CClientSocket() {
		closesocket(m_sock);
		WSACleanup();
	}

	BOOL InitSockEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
			return FALSE;
		}
		return TRUE;
	}

	static void releaseInstance() {
		if (m_instance != NULL) {
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}

	static CClientSocket* m_instance;

	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releaseInstance();
		}
	};

	static CHelper m_helper;
};

