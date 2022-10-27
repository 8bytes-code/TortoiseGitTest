#pragma once
#include "HeThread.h"
#include <map>
#include "CHeQueue.h"
#include <MSWSock.h>


enum HeOperator{
	HNone,
	HAccept,
	HRecv,
	HSend,
	HError
};

class HeServer;
class HeClient;

class HeOverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;			//操作，具体看枚举
	std::vector<char> m_buffer;	//缓冲区
	ThreadWorker m_worker;		//处理函数
	HeServer* m_server;			//服务器对象
};

template<HeOperator> class AcceptOverlapped;
typedef AcceptOverlapped<HAccept> ACCEPTOVERLAPPED;
typedef std::shared_ptr<HeClient> PCLIENT;

class HeClient {
public:
	HeClient();
	~HeClient() {
		closesocket(m_sock);
	}
	void SetOverlapped(PCLIENT& ptr);
	operator SOCKET() {
		return m_sock;
	}
	operator PVOID() {
		//返回缓冲区首地址
		return &m_buffer[0];
	}
	operator LPOVERLAPPED();
	operator LPDWORD() {
		return &m_received;
	}
	sockaddr_in* GetLocalAddr() {
		return &m_laddr;
	}
	sockaddr_in* GetRemoteAddr() {
		return &m_raddr;
	}
private:
	SOCKET m_sock;
	DWORD m_received;
	std::shared_ptr<ACCEPTOVERLAPPED> m_pOverlapped;
	std::vector<char> m_buffer;		//缓冲区
	sockaddr_in m_laddr;			//本地地址
	sockaddr_in m_raddr;			//远程地址
	bool m_isbusy;					//状态
};

template<HeOperator>
class AcceptOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	AcceptOverlapped();
	int AcceptWorker();
	PCLIENT m_client;
};


template<HeOperator>
class RecvOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	RecvOverlapped() :m_operator(HRecv), m_worker(this, &RecvOverlapped::RecvWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024 * 256);
	}
	int RecvWorker() {

	}
};


template<HeOperator>
class SendOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	SendOverlapped() :m_operator(HAccept), m_worker(this, &SendOverlapped::SendWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024 * 256);
	}
	int SendWorker() {

	}
};


template<HeOperator>
class ErrorOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	ErrorOverlapped() :m_operator(HError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);
	}
	int ErrorWorker() {

	}
};

typedef RecvOverlapped<HRecv> RECVOVERLAPPED;
typedef SendOverlapped<HSend> SENDOVERLAPPED;
typedef ErrorOverlapped<HError> ERROROVERLAPPED;


class HeServer :
    public ThreadFuncBase {
public:
	HeServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
		m_hIOCP = INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}
	~HeServer(){}

	bool StartService() {
		CreateSocket();

		if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		if (listen(m_sock, 3) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
		if (m_hIOCP == NULL) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
		m_pool.Invoke();
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&HeServer::threadIocp));
		if (!NewAccept())return false;
		
		return true;
	}

	bool NewAccept() {
		PCLIENT pClient(new HeClient());
		pClient->SetOverlapped(pClient);
		m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
		if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient)) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		return true;
	}
private:
	void CreateSocket() {
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		int opt = 1;
		setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	}

	int threadIocp() {
		DWORD tranferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* lpOverlapped = NULL;
		if (GetQueuedCompletionStatus(m_hIOCP, &tranferred, &CompletionKey, &lpOverlapped, INFINITE)) {
			if (tranferred > 0 && (CompletionKey != 0)) {
				HeOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, HeOverlapped, m_overlapped);
				switch (pOverlapped->m_operator) {
					case HAccept:
					{
						ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}
						break;
					case HRecv:
					{
						RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}
						break;
					case HSend:
					{
						SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}
						break;
					case HError:
					{
						ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}
						break;
				}
			}
			else {
				return -1;
			}
		}
		return 0;
	}
private:
	HeThreadPool m_pool;										//线程池对象
	HANDLE m_hIOCP;												//完成端口对象
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<HeClient>> m_client;		//维护连接的客户端
};

