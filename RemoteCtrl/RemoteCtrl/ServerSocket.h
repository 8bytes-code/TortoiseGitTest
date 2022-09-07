#pragma once
#include "pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
class CPacket {
public:
	CPacket():sHead(0),nLength(0),sCmd(0),sSum(0) {}
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
		nLength = *(WORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {		//包未完全接受，解析失败就返回
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);  i += 2;

		if (nLength > 4) {
			strData.resize(nLength - sCmd - 2 - 2);
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
	~CPacket(){}
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

	const char* Data() {
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
	std::string strOut;		//整个包数据
};
#pragma pack(pop)

class CServerSocket {
public:
	static CServerSocket* getInstance() {
		//由于静态函数没有this指针无法直接调用
		if (m_instance == NULL) {
			m_instance = new CServerSocket();
		}
		return m_instance;
	}

	bool InitSocket() {
		if (m_sock == -1)return false;

		//网络参数
		sockaddr_in serv_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		serv_adr.sin_addr.S_un.S_addr = INADDR_ANY;
		serv_adr.sin_port = htons(9527);

		//bind
		if (bind(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
			return false;
		}

		//listen
		if (listen(m_sock, 1) == -1) {
			return false;
		}

		return true;
	}

	bool AcceptClient() {
		sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);
		m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
		if (m_client == -1)return false;
		
		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_client == -1)return -1;
		//char buffer[1024] = "";
		
		char* buffer = new char[BUFFER_SIZE];
		size_t index = 0;
		memset(buffer, 0, BUFFER_SIZE);
		while (true) {
			size_t len = recv(m_client, buffer, BUFFER_SIZE - index, 0);
			if (len <= 0) {
				return -1;
			}
			index += len;
			len = index;

			// TODO:处理命令，来自客户端的buffer
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				return m_packet.sCmd;
			}
		}
		return -1;
	}

	bool Send(const char* pData, int nSize) {
		if (m_client == -1)return false;
		return send(m_client, pData, nSize, 0) > 0;
	}

	bool Send(CPacket& pack) {
		if (m_client == -1)return false;
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}

	bool GetFilePath(std::string& strPath) {
		//当前命令为2才是去执行获取文件列表
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
private:
	SOCKET m_client;
	SOCKET m_sock;
	CPacket m_packet;

	CServerSocket() {
		m_client = INVALID_SOCKET;
		if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境，请检查网络设置！"), _T("初始化错误!"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_sock = socket(PF_INET, SOCK_STREAM, 0);
	}
	CServerSocket(const CServerSocket& ss) {
		m_sock = ss.m_sock;
		m_client = ss.m_client;
	}
	CServerSocket& operator=(const CServerSocket&ss){}

	~CServerSocket() {
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
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}

	static CServerSocket* m_instance;

	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {
			CServerSocket::releaseInstance();
		}
	};

	static CHelper m_helper;
};

