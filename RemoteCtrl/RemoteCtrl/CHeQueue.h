#pragma once
#include "pch.h"
#include <list>
#include <atomic>

template<class T>
class CHeQueue {
public:
	enum {
		HQNone,
		HQPush,
		HQPop,
		HQSize,
		HQClear
	};

	typedef struct IocpParam {
		size_t nOperator;			//操作 附带记录size
		T Data;						//数据
		HANDLE hEvent;				//pop时机
		
		
		IocpParam(int op, typename const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = HQNone;
		}
	}PPARAM;	//Post Parameter 用于投递信息的结构体
	
//线程安全的队列(利用IOCP实现)
public:
	CHeQueue() {
		m_lock = false;
		m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompeletionPort != NULL) {
			m_hThread = (HANDLE)_beginthread(
				&CHeQueue<T>::theradEntry, 
				0, m_hCompeletionPort
			);
		}
	}
	~CHeQueue() {
		if (m_lock)return;
		m_lock = true;
		HANDLE hTemp = m_hCompeletionPort;
		PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		m_hCompeletionPort = NULL;
		CloseHandle(hTemp);
	}
	bool PushBack(const T& data) {
		IocpParam* pParam = new IocpParam(HQPush, data);
		if (m_lock) {
			delete pParam;
			return false;
		}
		BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == FALSE)delete pParam;
		return ret;
	}
	bool PopFront(T& data) {
		//pop的时候期望要标记这个值，老办法用事件
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(HQPop, data, hEvent);
		if (m_lock) {
			if (hEvent) CloseHandle(hEvent);
			return false;
		}
		BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == FALSE) {
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}
	size_t Size() {
		//确保线程之中能取到正确的size，也要考虑安全
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(HQSize, T(), hEvent);
		if (m_lock) {
			if (hEvent) CloseHandle(hEvent);
			return -1;
		}
		BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == FALSE) {
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return -1;
	}
	bool Clear() {
		if (m_lock) return false;
		IocpParam* pParam = new IocpParam(HQClear, T());
		BOOL ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == FALSE)delete pParam;
		return ret;
	}
private:
	static void theradEntry(void* arg) {
		CHeQueue<T>* thiz = (CHeQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}

	void DealParam(PPARAM* pParam) {
		switch (pParam->nOperator) {
			case HQPush:
				m_lstData.push_back(pParam->Data);
				delete pParam;
				break;
			case HQPop:
				if (m_lstData.size() > 0) {
					pParam->Data = m_lstData.front();
					m_lstData.pop_front();
				}
				if (pParam->hEvent != NULL) SetEvent(pParam->hEvent);
				break;
			case HQSize:
				pParam->nOperator = m_lstData.size();
				if (pParam->hEvent != NULL) SetEvent(pParam->hEvent);
				break;
			case HQClear:
				m_lstData.clear();
				delete pParam;
				break;
			default:
				TRACE("[%s]:threadMain error!\r\n", __FUNCTION__);
				break;
		}
	}

	void threadMain() {
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {
			if ((dwTransferred == 0) && (CompletionKey == NULL)) {
				printf("thread is prepare to exit!\r\n");
				break;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}
		//预防超时
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, 0)) {
			if ((dwTransferred == 0) || (CompletionKey == NULL)) {
				printf("thread is prepare to exit!\r\n");
				continue;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}
		CloseHandle(m_hCompeletionPort);
	}

private:
	std::list<T> m_lstData;
	HANDLE m_hCompeletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;				//队列析构
};

