#pragma once
template<class T>
class CHeQueue {
	//�̰߳�ȫ�Ķ���(����IOCPʵ��)
public:
	CHeQueue();
	~CHeQueue();
	bool PushBack(const T& data);
	bool PopFront(T& data);
	size_t Size();
	void Clear();
private:
	static void theradEntry(void* arg);
	void threadMain();
private:
	std::list<T> m_lstData;
	HANDLE m_hCompeletionPort;
	HANDLE n_thread;
public:
	typedef struct IocpParam {
		int nOperator;					//����
		T strData;						//����
		_beginthread_proc_type cbFunc;	//�ص�
		HANDLE hEvent;					//popʱ��

		IocpParm(int op, const char* sData, _beginthread_proc_type cb = NULL) {
			nOperator = op;
			strData = sData;
			cbFunc = cb;
		}
		IocpParm() {
			nOperator = -1;
		}
	}PPARAM;	//Post Parameter ����Ͷ����Ϣ�Ľṹ��

	enum {
		HQPush,
		HQPop,
		HQSize,
		HQClear
	};
};

