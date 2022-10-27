#include "pch.h"
#include "HeServer.h"
#pragma warning(disable:4407)

template<HeOperator op>
AcceptOverlapped<op>::AcceptOverlapped() {
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = HAccept;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024);
	m_server = NULL;
}

template<HeOperator op>
int AcceptOverlapped<op>::AcceptWorker() {
	//连接到一个客户端就发起一次accept
	if (*(LPDWORD)*m_client.get() > 0) {
		INT lLength = 0, rLength = 0;
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)m_client->GetLocalAddr(), &lLength,
			(sockaddr**)m_client->GetRemoteAddr(), &rLength
		);

		if (!m_server->NewAccept()) {
			return -2;
		}
	}
	return -1;
}


HeClient::HeClient() :m_isbusy(false), m_pOverlapped(new ACCEPTOVERLAPPED()) {
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}
void HeClient::SetOverlapped(PCLIENT& ptr) {
	m_pOverlapped->m_client = ptr;
}
HeClient::operator LPOVERLAPPED() {
	return &m_pOverlapped->m_overlapped;
}