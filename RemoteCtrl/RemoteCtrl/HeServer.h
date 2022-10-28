#pragma once
#include "HeThread.h"
#include "HeTool.h"
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
typedef std::shared_ptr<HeClient> PCLIENT;

class HeOverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;			//���� ���ö��
	std::vector<char> m_buffer;	//������
	ThreadWorker m_worker;		//������
	HeServer* m_server;			//����������
	HeClient* m_client;			//��Ӧ�ͻ���
	WSABUF m_wsabuffer;
	virtual ~HeOverlapped() {
		m_buffer.clear();
	}
};

template<HeOperator> class AcceptOverlapped;
typedef AcceptOverlapped<HAccept> ACCEPTOVERLAPPED;
template<HeOperator> class RecvOverlapped;
typedef RecvOverlapped<HRecv> RECVOVERLAPPED;
template<HeOperator> class SendOverlapped;
typedef SendOverlapped<HSend> SENDOVERLAPPED;


class HeClient:public ThreadFuncBase {
public:
	HeClient();
	~HeClient() {
		m_buffer.clear();
		closesocket(m_sock);
		m_recv.reset();
		m_send.reset();
		m_pOverlapped.reset();
		m_vecSend.Clear();
	}
	void SetOverlapped(PCLIENT& ptr);

	operator SOCKET() {
		return m_sock;
	}
	operator PVOID() {
		//���ػ������׵�ַ
		return &m_buffer[0];
	}
	operator LPOVERLAPPED();
	operator LPDWORD() {
		return &m_received;
	}
	LPWSABUF RecvWSABuffer();
	LPWSABUF SendWSABuffer();
	DWORD& flags() { return m_flags; }
	sockaddr_in* GetLocalAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }
	size_t GetBufferSize()const { return m_buffer.size(); }
	int Recv();
	int Send(void* buffer, size_t nSize);
	int SendData(std::vector<char>& data);
private:
	SOCKET m_sock;
	DWORD m_received;
	DWORD m_flags;
	std::shared_ptr<ACCEPTOVERLAPPED> m_pOverlapped;
	std::shared_ptr<RECVOVERLAPPED> m_recv;
	std::shared_ptr<SENDOVERLAPPED> m_send;
	std::vector<char> m_buffer;		//������
	size_t m_used;					//��ʹ�õĻ�������С
	sockaddr_in m_laddr;			//���ص�ַ
	sockaddr_in m_raddr;			//Զ�̵�ַ
	bool m_isbusy;					//״̬
	HeSendQueue<std::vector<char>> m_vecSend;	//send����
};

template<HeOperator>
class AcceptOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	AcceptOverlapped();
	int AcceptWorker();
};


template<HeOperator>
class RecvOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	RecvOverlapped();
	int RecvWorker() {
		int ret = m_client->Recv();
		return ret;
	}
	
};

template<HeOperator>
class SendOverlapped :public HeOverlapped, ThreadFuncBase {
public:
	SendOverlapped();
	int SendWorker() {
		/*
		* 1.send���û����ô�죬�������Ƚϴ��ʱ��
		*/
		return -1;
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
		return -1;
	}
};
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
	~HeServer();
	bool StartService();
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

	int threadIocp();
private:
	HeThreadPool m_pool;										//�̳߳ض���
	HANDLE m_hIOCP;												//��ɶ˿ڶ���
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<HeClient>> m_client;		//ά�����ӵĿͻ���
};

