#pragma once
#include "pch.h"
#include "framework.h"
#include "Packet.h"
#include <list>


//typedef void(*SOCKET_CALLBACK)(void* arg, int status, std::list<CPacket>& lstPacket, CPacket& inPacket);
typedef void(*SOCKET_CALLBACK)(void* , int , std::list<CPacket>& , CPacket& );
//对于这种情况，形参名称可填可不填，填了也是被忽略，只是为了自己能认清


class CServerSocket {
public:
	static CServerSocket* getInstance() {
		//由于静态函数没有this指针无法直接调用
		if (m_instance == NULL) {
			m_instance = new CServerSocket();
		}
		return m_instance;
	}


	int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527) {
		//1.进度的可控性 2.对接的便捷性 3.可行性评估，提早暴露风险
		//socket、bind、listen、accept、read、write、close
		bool ret = InitSocket(port);
		if (ret == false) return -1;
		std::list<CPacket> lstPackets;
		m_callback = callback;
		m_arg = arg;

		int count = 0;
		while (true) {
			if (AcceptClient() == false) {
				if (count >= 3) {
					return -2;
				}
				count++;
			}
			int ret = DealCommand();
			if (ret > 0) {
				m_callback(m_arg, ret, lstPackets, m_packet);
				while (lstPackets.size() > 0) {
					//利用容器，将所需要执行的命令功能从头部依次发出，发出后弹出这个命令
					Send(lstPackets.front());
					lstPackets.pop_front();
				}
			}
			CloseClient();
		}

		return 0;
	}


protected:
	bool InitSocket(short port) {
		if (m_sock == -1)return false;

		//网络参数
		sockaddr_in serv_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		serv_adr.sin_addr.s_addr = INADDR_ANY;
		serv_adr.sin_port = htons(port);

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
		if (buffer == NULL) {
			TRACE("内存不足!\r\n");
			return -2;
		}
		size_t index = 0;
		memset(buffer, 0, BUFFER_SIZE);
		while (true) {
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0) {
				delete[] buffer;
				return -1;
			}
			TRACE("recv:%d\r\n", len);
			index += len;
			len = index;

			// TODO:处理命令，来自客户端的buffer
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				delete[] buffer;
				TRACE("%s", m_packet.strData.c_str());
				return m_packet.sCmd;
			}
		}
		delete[] buffer;
		return -1;
	}

	bool Send(const char* pData, int nSize) {
		if (m_client == -1)return false;
		return send(m_client, pData, nSize, 0) > 0;
	}

	bool Send(CPacket& pack) {
		if (m_client == -1)return false;
		//Dump((BYTE*)pack.Data(), pack.Size());
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}

	void CloseClient() {
		if (m_client != INVALID_SOCKET) {
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
	}
private:
	SOCKET_CALLBACK m_callback;
	void* m_arg;
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

