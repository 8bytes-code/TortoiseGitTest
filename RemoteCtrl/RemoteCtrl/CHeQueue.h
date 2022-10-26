#pragma once
template<class T>
class CHeQueue {
	//线程安全的队列(利用IOCP实现)
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
		int nOperator;					//操作
		T strData;						//数据
		_beginthread_proc_type cbFunc;	//回调
		HANDLE hEvent;					//pop时机

		IocpParm(int op, const char* sData, _beginthread_proc_type cb = NULL) {
			nOperator = op;
			strData = sData;
			cbFunc = cb;
		}
		IocpParm() {
			nOperator = -1;
		}
	}PPARAM;	//Post Parameter 用于投递信息的结构体

	enum {
		HQPush,
		HQPop,
		HQSize,
		HQClear
	};
};

