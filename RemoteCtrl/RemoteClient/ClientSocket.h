#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#define WM_SEND_PACK (WM_USER+1)		//���Ͱ�������Ϣ
#define WM_SEND_PACK_ACK (WM_USER+2)	//���Ͱ�����Ӧ��

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
				//i+=2 ����հ�ʱ�������������
				i += 2;
				break;
			}
		}

		//�˴����Ȳο��������
		if ((i + 4 + 2 + 2) > nSize) {	//�����ݿ��ܲ�ȫ�����߰�ͷδ��ȫ�����յ�
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {		//��δ��ȫ���ܣ�����ʧ�ܾͷ���
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

	int Size() {	//�����ݵĴ�С
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
	WORD sHead;				//��ͷ			2
	DWORD nLength;			//������			4	
	WORD sCmd;				//��������		2
	std::string strData;	//������
	WORD sSum;				//У�飺��У��	2
	//std::string strOut;		//����������
};
#pragma pack(pop)

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}

	WORD nAction;	//������ƶ���˫��
	WORD nButton;	//������Ҽ����м�
	POINT ptXY;		//����
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	//c++�нṹ��Ҳ�й��캯�������಻ͬ���ڽṹ��Ĭ�϶���public������Ĭ����private
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;			//�Ƿ���Ч
	BOOL IsDirectory;		//�Ƿ�ΪĿ¼ 0�� 1��
	BOOL HasNext;			//�Ƿ��к����ļ�
	char szFileName[256];	//�ļ��� 0�� 1��
}FILEINFO, * PFILEINFO;

enum {
	CSM_AUTOCLOSE = 1,	//csm=client socket mode�Զ��ر�ģʽ
};

typedef struct PacketData {
	std::string strData;
	UINT nMode;
	WPARAM wParam;
	PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
		strData.resize(nLen);
		memcpy((char*)strData.c_str(), pData, nLen);
		nMode = mode;
		wParam = nParam;
	}
	PacketData(const PacketData& data) {
		strData = data.strData;
		nMode = data.nMode;
		wParam = data.wParam;
	}
	PacketData& operator=(const PacketData& data) {
		if (this != &data) {
			strData = data.strData;
			nMode = data.nMode;
			wParam = data.wParam;
		}
		return *this;
	}
}PACKET_DATA;


std::string GetErrInfo(int wsaErrCode);

class CClientSocket {
public:
	static CClientSocket* getInstance() {
		//���ھ�̬����û��thisָ���޷�ֱ�ӵ���
		if (m_instance == NULL) {
			m_instance = new CClientSocket();
		}
		return m_instance;
	}

	bool InitSocket();


#define BUFFER_SIZE 4194304		//1024*1024*4���󻺳���
	int DealCommand() {
		if (m_sock == -1)return -1;

		char* buffer = m_buffer.data();
		static size_t index = 0;

		while (true) {
			size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
			//�޲�����Ϊsize_tԭ����unsigned int���޷��Źʴ�-1��ʱ��������
			//����Ϊ����Ʋ��ܴ�ģ�ֻ����if��ʱ��ǿת��
			if (((int)len <= 0) && ((int)index <= 0)) {
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

	//���·�װ��������Ϊ�������¼�����Ҫƥ��
	bool SendPacket(HWND hWnd, CPacket& pack, bool isAutoClosed = true, WPARAM wParam = 0);

	bool GetFilePath(std::string& strPath) {
		//��ǰ����Ϊ2-4����ȥִ�л�ȡ�ļ��б�
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
		if ((m_nIP != nIP) || (nPort != nPort)) {
			m_nIP = nIP;
			m_nPort = nPort;
		}
	}

private:
	UINT m_nThreadID;
	typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);	//��Ϣ�ص�
	std::map<UINT, MSGFUNC> m_mapFunc;
	HANDLE m_hThread;
	std::mutex m_lock;	//�����壬����߳���������
	bool m_bAutoClose;
	std::list<CPacket> m_lstSend;
	/*
	* vectorò�ƴ�����ʱ�����ݻ�����õĴ�һ�㣬�������ݣ����������̫��vector
	* ������ҪƵ�������ݣ��ƶ�������ʱ�临�Ӷȶ��Դ�ʱvector�������ã���Ϊ���׷����ٰ�Ҳ�����
	* list����˫������ֻ��Ҫ�ı�ָ����У�����������ɵ��ڴ���Ƭ�Ƚϸߣ��ռ������ʱȽϵ�
	*/
	std::map<HANDLE, std::list<CPacket>&> m_mapAck;
	std::map<HANDLE, bool> m_mapAutoClosed;
	int m_nIP;		//��ַ
	int m_nPort;	//�˿�
	std::vector<char> m_buffer;
	SOCKET m_sock;
	CPacket m_packet;

	CClientSocket();
	CClientSocket(const CClientSocket& ss);
	CClientSocket& operator=(const CClientSocket& ss) {}

	~CClientSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup();
	}

	void SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam);

	bool Send(const char* pData, int nSize) {
		if (m_sock == -1)return false;
		return send(m_sock, pData, nSize, 0) > 0;
	}

	bool Send(const CPacket& pack) {
		TRACE("m_sock = %d\r\n", m_sock);
		if (m_sock == -1)return false;
		std::string strOut;
		pack.Data(strOut);
		return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
	}

	//�̴߳����
	static unsigned __stdcall threadEntry(void* arg);
	void threadFunc();
	void threadFunc2();	//�ص�����

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

