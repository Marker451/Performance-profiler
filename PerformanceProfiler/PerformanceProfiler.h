#pragma once

#include <iostream>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <string>
#include <map>
#include <algorithm>

// C++11
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#ifdef _WIN32
#include <Windows.h>
#include<Psapi.h>
#pragma comment(lib,"Psapi.lib")
#else
#include <pthread.h>
#endif // _WIN32

using namespace std;

#include "../IPC/IPCManager.h"

typedef long long LongType;


// �����̬�⵼����̬���������⣬���������Ϊ��̬����
#if(defined(_WIN32) && defined(IMPORT))
#define API_EXPORT _declspec(dllimport)
#elif _WIN32
#define API_EXPORT _declspec(dllexport)
#else
#define API_EXPORT
#endif

//
// ��ȡ��ǰ�߳�id
//
static int GetThreadId()
{
#ifdef _WIN32
	return ::GetCurrentThreadId();
#else
	return ::thread_self();
#endif
}

// �����������������
class SaveAdapter
{
public:
	virtual int Save(char* format, ...) = 0;
};

// ����̨����������
class ConsoleSaveAdapter : public SaveAdapter
{
public:
	virtual int Save(char* format, ...)
	{
		va_list argPtr;
		int cnt;

		va_start(argPtr, format);
		cnt = vfprintf(stdout, format, argPtr);
		va_end(argPtr);

		return cnt;
	}
};

// �ļ�����������
class FileSaveAdapter : public SaveAdapter
{
public:
	FileSaveAdapter(const char* path)
		:_fOut(0)
	{
		_fOut = fopen(path, "w");
	}

	~FileSaveAdapter()
	{
		if (_fOut)
		{
			fclose(_fOut);
		}
	}

	virtual int Save(char* format, ...)
	{
		if (_fOut)
		{
			va_list argPtr;
			int cnt;

			va_start(argPtr, format);
			cnt = vfprintf(_fOut, format, argPtr);
			va_end(argPtr);

			return cnt;
		}

		return 0;
	}
private:
	FileSaveAdapter(const FileSaveAdapter&);
	FileSaveAdapter& operator==(const FileSaveAdapter&);

private:
	FILE* _fOut;
};

// ��������
//template<class T>
//class Singleton
//{
//public:
//	static T* GetInstance()
//	{
//		return _sInstance;
//	}
//protected:
//	Singleton()
//	{}
//
//	static T* _sInstance;
//};
//
//// ��̬����ָ���ʼ������֤�̰߳�ȫ�� 
//template<class T>
//T* Singleton<T>::_sInstance = new T();

// ��������
//template<class T>
//class Singleton
//{
//public:
//	static T* GetInstance()
//	{
//		return _sInstance;
//	}
//protected:
//	Singleton()
//	{}
//
//	static T* _sInstance;
//};
//
//// ��̬����ָ���ʼ������֤�̰߳�ȫ�� 
//template<class T>
//T* Singleton<T>::_sInstance = new T();

//
// ���������Ǿ�̬�������ڴ����ĵ��������캯���л�����IPC����Ϣ�����̡߳�
// ������ɶ�̬��ʱ������main���֮ǰ�ͻ��ȼ��ض�̬�⣬��ʹ������ķ�ʽ����ʱ
// �ͻᴴ������������ʱ����IPC����Ϣ�����߳̾ͻῨ��������ʹ������ĵ���ģʽ��
// �ڵ�һ��ȡ��������ʱ��������������
//

// ��������
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		//
		// 1.˫�ؼ�鱣���̰߳�ȫ��Ч�ʡ�
		//
		if (_sInstance == NULL)
		{
			unique_lock<mutex> lock(_mutex);
			if (_sInstance == NULL)
			{
				_sInstance = new T();
			}
		}

		return _sInstance;
	}
protected:
	Singleton()
	{}

	static T* _sInstance;	// ��ʵ������
	static mutex _mutex;	// ����������
};

template<class T>
T* Singleton<T>::_sInstance = NULL;

template<class T>
mutex Singleton<T>::_mutex;

enum PP_CONFIG_OPTION
{
	PPCO_NONE = 0,					// ��������
	PPCO_PROFILER = 2,				// ��������
	PPCO_SAVE_TO_CONSOLE = 4,		// ���浽����̨
	PPCO_SAVE_TO_FILE = 8,			// ���浽�ļ�
	PPCO_SAVE_BY_CALL_COUNT = 16,	// �����ô������򱣴�
	PPCO_SAVE_BY_COST_TIME = 32,	// �����û���ʱ�併�򱣴�
};

//
// ���ù���
//
class API_EXPORT ConfigManager : public Singleton<ConfigManager>
{
public:
	void SetOptions(int flag)
	{
		_flag = flag;
	}
	int GetOptions()
	{
		return _flag;
	}

	ConfigManager()
		:_flag(PPCO_NONE)
	{}
private:
	int _flag;
};

///////////////////////////////////////////////////////////////////////////
// ��Դͳ��

// ��Դͳ����Ϣ
struct ResourceInfo
{
	LongType _peak;	 // ����ֵ
	LongType _avg;	 // ƽ��ֵ

	LongType _total;  // ��ֵ
	LongType _count;  // ����

	ResourceInfo()
		: _peak(0)
		,_avg(0)
		, _total(0)
		,_count(0)
	{}

	void Update(LongType value);
	void Serialize(SaveAdapter& SA) const;
};

// ��Դͳ��
class ResourceStatistics
{
public:
	ResourceStatistics();
	~ResourceStatistics();

	// ��ʼͳ��
	void StartStatistics();

	// ֹͣͳ��
	void StopStatistics();

	// ��ȡCPU/�ڴ���Ϣ 
	const ResourceInfo& GetCpuInfo();
	const ResourceInfo& GetMemoryInfo();

private:
	void _Statistics();

// Windows��Linux��ʵ����Դͳ��
#ifdef _WIN32
	// ��ȡCPU���� / ��ȡ�ں�ʱ��
	int _GetCpuCount();
	LongType _GetKernelTime();

	// ��ȡCPU/�ڴ�/IO��Ϣ
	LongType _GetCpuUsageRate();
	LongType _GetMemoryUsage();
	void _GetIOUsage(LongType& readBytes, LongType& writeBytes);

	// ����ͳ����Ϣ
	void _UpdateStatistics();
#else // Linux
	void _UpdateStatistics();
#endif

public:
#ifdef _WIN32
	int	_cpuCount;				// CPU����
	HANDLE _processHandle;		// ���̾��

	LongType _lastSystemTime;	// �����ϵͳʱ��
	LongType _lastKernelTime;	// ������ں�ʱ��
#else
	int _pid;					// ����ID
#endif // _WIN32

	ResourceInfo _cpuInfo;				// CPU��Ϣ
	ResourceInfo _memoryInfo;			// �ڴ���Ϣ
	//ResourceInfo _readIOInfo;			// ��������Ϣ
	//ResourceInfo _writeIOInfo;		// д���ݵ���Ϣ

	atomic<int> _refCount;				// ���ü���
	mutex _lockMutex;					// �̻߳�����
	condition_variable _condVariable;	// �����Ƿ����ͳ�Ƶ���������
	thread _statisticsThread;			// ͳ���߳�
};

//////////////////////////////////////////////////////////////////////
// IPC���߿��Ƽ�������

class IPCMonitorServer : public Singleton<IPCMonitorServer>
{
	friend class Singleton<IPCMonitorServer>;
	typedef void(*CmdFunc) (string& reply);
	typedef map<string, CmdFunc> CmdFuncMap;

public:
	// ����IPC��Ϣ��������߳�
	void Start();

protected:
	// IPC�����̴߳�����Ϣ�ĺ���
	void OnMessage();

	//
	// ���¾�Ϊ�۲���ģʽ�У���Ӧ������Ϣ�ĵĴ�����
	//
	static void GetState(string& reply);
	static void Enable(string& reply);
	static void Disable(string& reply);
	static void Save(string& reply);

	IPCMonitorServer();
private:
	thread	_onMsgThread;			// ������Ϣ�߳�
	CmdFuncMap _cmdFuncsMap;		// ��Ϣ���ִ�к�����ӳ���
};

///////////////////////////////////////////////////////////////////////////
// ���������

//
// ���������ڵ�
//
struct API_EXPORT PerformanceNode
{
	string _fileName;	// �ļ���
	string _function;	// ������
	int	   _line;		// �к�
	string _desc;		// ��������

	PerformanceNode(const char* fileName, const char* function,
		int line, const char* desc);

	//
	// ��map��ֵ��������Ҫ����operator<
	// ��unorder_map��ֵ��������Ҫ����operator==
	//
	bool operator<(const PerformanceNode& p) const;
	bool operator==(const PerformanceNode& p) const;

	// ���л��ڵ���Ϣ������
	void Serialize(SaveAdapter& SA) const;
};

// hash�㷨
static size_t BKDRHash(const char *str)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313
	unsigned int hash = 0;

	while (*str)
	{
		hash = hash * seed + (*str++);
	}

	return (hash & 0x7FFFFFFF);
}

// ʵ��PerformanceNodeHash�º�����unorder_map�ıȽ���
class PerformanceNodeHash
{
public:
	size_t operator() (const PerformanceNode& p)
	{
		string key = p._function;
		key += p._fileName;
		key += p._line;

		return BKDRHash(key.c_str());
	}
};

//
// ����������
//
class API_EXPORT PerformanceProfilerSection
{
	//typedef unordered_map<thread::id, LongType> StatisMap;
	typedef unordered_map<int, LongType> StatisMap;

	friend class PerformanceProfiler;
public:
	PerformanceProfilerSection()
		:_totalRef(0)
		, _rsStatistics(0)
		, _totalCallCount(0)
		, _totalCostTime(0)
	{}

	void Begin(int threadId);
	void End(int threadId);

	void Serialize(SaveAdapter& SA);
private:
	mutex _mutex;					// ������
	StatisMap _beginTimeMap;		// ��ʼʱ��ͳ��
	StatisMap _costTimeMap;			// ����ʱ��ͳ��
	LongType _totalCostTime;		// �ܻ���ʱ��

	StatisMap _refCountMap;			// ���ü���(�����������β��ƥ�䣬�ݹ麯���ڲ�������)
	LongType _totalRef;				// �ܵ����ü���

	StatisMap _callCountMap;		// ���ô���ͳ��
	LongType _totalCallCount;		// �ܵĵ��ô���

	ResourceStatistics* _rsStatistics;	// ��Դͳ���̶߳���
};

class API_EXPORT PerformanceProfiler : public Singleton<PerformanceProfiler>
{
public:

	friend class Singleton<PerformanceProfiler>;

	//
	// unordered_map�ڲ�ʹ��hash_tableʵ�֣�ʱ�临�Ӷ�Ϊ����map�ڲ�ʹ�ú������
	// unordered_map������ʱ�临�Ӷ�ΪO(1)��mapΪO(lgN)��so! unordered_map����Ч��
	// http://blog.chinaunix.net/uid-20384806-id-3055333.html
	//
	typedef unordered_map<PerformanceNode, PerformanceProfilerSection*, PerformanceNodeHash> PerformanceProfilerMap;
	//typedef map<PerformanceNode, PerformanceProfilerSection*> PerformanceProfilerMap;

	//
	// ����������
	//
	PerformanceProfilerSection* CreateSection(const char* fileName,
		const char* funcName, int line, const char* desc, bool isStatistics);

	static void OutPut();
protected:

	static bool CompareByCallCount(PerformanceProfilerMap::iterator lhs,
		PerformanceProfilerMap::iterator rhs);
	static bool CompareByCostTime(PerformanceProfilerMap::iterator lhs,
		PerformanceProfilerMap::iterator rhs);

	PerformanceProfiler();

	// ������л���Ϣ
	void _OutPut(SaveAdapter& SA);
private:
	time_t  _beginTime;
	mutex _mutex;
	PerformanceProfilerMap _ppMap;
};

// ������������ο�ʼ
#define ADD_PERFORMANCE_PROFILE_SECTION_BEGIN(sign, desc, isStatistics) \
	PerformanceProfilerSection* PPS_##sign = NULL;						\
	if (ConfigManager::GetInstance()->GetOptions()&PPCO_PROFILER)		\
	{																	\
		PPS_##sign = PerformanceProfiler::GetInstance()->CreateSection(__FILE__, __FUNCTION__, __LINE__, desc, isStatistics);\
		PPS_##sign->Begin(GetThreadId());								\
	}

// ������������ν���
#define ADD_PERFORMANCE_PROFILE_SECTION_END(sign)	\
	do{												\
		if(PPS_##sign)								\
			PPS_##sign->End(GetThreadId());			\
	}while(0);

//
// ������Ч�ʡ���ʼ
// @sign��������Ψһ��ʶ�������Ψһ�������α���
// @desc������������
//
#define PERFORMANCE_PROFILER_EE_BEGIN(sign, desc)	\
	ADD_PERFORMANCE_PROFILE_SECTION_BEGIN(sign, desc, false)

//
// ������Ч�ʡ�����
// @sign��������Ψһ��ʶ
//
#define PERFORMANCE_PROFILER_EE_END(sign)	\
	ADD_PERFORMANCE_PROFILE_SECTION_END(sign)

//
// ������Ч��&��Դ����ʼ��
// ps���Ǳ������������������Դͳ�ƶΣ���Ϊÿ����Դͳ�ƶζ��Ὺһ���߳̽���ͳ��
// @sign��������Ψһ��ʶ�������Ψһ�������α���
// @desc������������
//
#define PERFORMANCE_PROFILER_EE_RS_BEGIN(sign, desc)	\
	ADD_PERFORMANCE_PROFILE_SECTION_BEGIN(sign, desc, true)

//
// ������Ч��&��Դ������
// ps���Ǳ������������������Դͳ�ƶΣ���Ϊÿ����Դͳ�ƶζ��Ὺһ���߳̽���ͳ��
// @sign��������Ψһ��ʶ
//
#define PERFORMANCE_PROFILER_EE_RS_END(sign)		\
	ADD_PERFORMANCE_PROFILE_SECTION_END(sign)

//
// ��������ѡ��
//
#define SET_PERFORMANCE_PROFILER_OPTIONS(flag)		\
	ConfigManager::GetInstance()->SetOptions(flag)
