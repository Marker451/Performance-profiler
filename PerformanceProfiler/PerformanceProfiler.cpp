#include "PerformanceProfiler.h"

//////////////////////////////////////////////////////////////
// ��Դͳ������
//
//static void RecordErrorLog(const char* errMsg, int line)
//{
//#ifdef _WIN32
//	printf("%s. ErrorId:%d, Line:%d\n", errMsg, GetLastError(), line);
//#else
//	printf("%s. ErrorId:%d, Line:%d\n", errMsg, errno, line);
//#endif
//}
//
//#define RECORD_ERROR_LOG(errMsg)		\
//	RecordErrorLog(errMsg, __LINE__);

void ResourceInfo::Update(LongType value)
{
	// value < 0ʱ��ֱ�ӷ��ز��ٸ��¡�
	if (value < 0)
		return;

	if (value > _peak)
		_peak = value;

	//
	// ���㲻׼ȷ��ƽ��ֵ�ܱ仯Ӱ�첻����
	// ���Ż����ɲο����绬�����ڷ������ڼ��㡣
	//
	_total += value;
	_avg = _total / (++_count);
}

ResourceStatistics::ResourceStatistics()
	:_refCount(0)
	,_statisticsThread(&ResourceStatistics::_Statistics, this)
{
	// ��ʼ��ͳ����Դ��Ϣ
#ifdef _WIN32
	_lastKernelTime = -1;
	_lastSystemTime = -1;
	_processHandle = ::GetCurrentProcess();
	_cpuCount = _GetCpuCount();
#else
	_pid = getpid();
#endif
}

ResourceStatistics::~ResourceStatistics()
{
#ifdef _WIN32
	CloseHandle(_processHandle);
#endif
}

void ResourceStatistics::StartStatistics()
{
	//
	// ����̲߳�������һ������εĳ�����ʹ�����ü�������ͳ�ơ�
	// ��һ���߳̽���������ʱ��ʼͳ�ƣ����һ���̳߳�������ʱ
	// ֹͣͳ�ơ�
	//
	if (_refCount++ == 0)
	{
		unique_lock<std::mutex> lock(_lockMutex);
		_condVariable.notify_one();
	}
}

void ResourceStatistics::StopStatistics()
{
	if (_refCount > 0)
	{
		--_refCount;
		_lastKernelTime = -1;
		_lastSystemTime = -1;
	}
}

const ResourceInfo& ResourceStatistics::GetCpuInfo()
{
	return _cpuInfo;
}

const ResourceInfo& ResourceStatistics::GetMemoryInfo()
{
	return _memoryInfo;
}

///////////////////////////////////////////////////
// ResourceStatistics

static const int CPU_TIME_SLICE_UNIT = 100;

void ResourceStatistics::_Statistics()
{
	while (1)
	{
		//
		// δ��ʼͳ��ʱ����ʹ����������������
		//
		if (_refCount == 0)
		{
			unique_lock<std::mutex> lock(_lockMutex);
			_condVariable.wait(lock);
		}

		// ����ͳ����Ϣ
		_UpdateStatistics();

		// ÿ��CPUʱ��Ƭ��Ԫͳ��һ��
		this_thread::sleep_for(std::chrono::milliseconds(CPU_TIME_SLICE_UNIT));
	}
}

#ifdef _WIN32
// FILETIME->long long
static LongType FileTimeToLongType(const FILETIME& fTime)
{
	LongType time = 0;

	time = fTime.dwHighDateTime;
	time <<= 32;
	time += fTime.dwLowDateTime;

	return time;
}

// ��ȡCPU����
int ResourceStatistics::_GetCpuCount()
{
	SYSTEM_INFO info;
	::GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

// ��ȡ�ں�ʱ��
LongType ResourceStatistics::_GetKernelTime()
{
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;

	if (false == GetProcessTimes(GetCurrentProcess(),
		&createTime, &exitTime, &kernelTime, &userTime))
	{
		RECORD_ERROR_LOG("GetProcessTimes Error");
	}

	return (FileTimeToLongType(kernelTime) + FileTimeToLongType(userTime)) / 10000;
}

// ��ȡCPUռ����
LongType ResourceStatistics::_GetCpuUsageRate()
{
	LongType cpuRate = -1;

	// 1.��������������¿�ʼ�ĵ�һ��ͳ�ƣ������������ں�ʱ���ϵͳʱ��
	if (_lastSystemTime == -1 && _lastKernelTime == -1)
	{
		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
		return cpuRate;
	}

	LongType systemTimeInterval = GetTickCount() - _lastSystemTime;
	LongType kernelTimeInterval = _GetKernelTime() - _lastKernelTime;

	// 2.���ķѵ�ϵͳʱ��ֵС���趨��ʱ��Ƭ��CPU_TIME_SLICE_UNIT�����򲻼���ͳ�ơ�
	if (systemTimeInterval > CPU_TIME_SLICE_UNIT)
	{
		cpuRate = kernelTimeInterval * 100 / systemTimeInterval;
		cpuRate /= _cpuCount;

		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
	}

	return cpuRate;
}

// ��ȡ�ڴ�ʹ����Ϣ
LongType ResourceStatistics::_GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS PMC;
	if (false == GetProcessMemoryInfo(_processHandle, &PMC, sizeof(PMC)))
	{
		RECORD_ERROR_LOG("GetProcessMemoryInfo Error");
	}

	return PMC.PagefileUsage;
}

// ��ȡIOʹ����Ϣ
void ResourceStatistics::_GetIOUsage(LongType& readBytes, LongType& writeBytes)
{
	IO_COUNTERS IOCounter;
	if (false == GetProcessIoCounters(_processHandle, &IOCounter))
	{
		RECORD_ERROR_LOG("GetProcessIoCounters Error");
	}

	readBytes = IOCounter.ReadTransferCount;
	writeBytes = IOCounter.WriteTransferCount;
}

// ����ͳ����Ϣ
void ResourceStatistics::_UpdateStatistics()
{
	_cpuInfo.Update(_GetCpuUsageRate());
	_memoryInfo.Update(_GetMemoryUsage());

	/*
	LongType readBytes, writeBytes;
	GetIOUsage(readBytes, writeBytes);
	_readIOInfo.Update(readBytes);
	_writeIOInfo.Update(writeBytes);
	*/
}

#else // Linux
void ResourceStatistics::_UpdateStatistics()
{
	char buf[1024] = { 0 };
	char cmd[256] = { 0 };
	sprintf(cmd, "ps -o pcpu,rss -p %d | sed 1,1d", _pid);

	//
	// �� "ps" �������� ͨ���ܵ���ȡ ("pid" ����) �� FILE* stream
	// ���ո� stream ����������ȡ��buf��
	// http://www.cnblogs.com/caosiyang/archive/2012/06/25/2560976.html
	FILE *stream = ::popen(cmd, "r");
	::fread(buf, sizeof (char), 1024, stream);
	::pclose(stream);

	double cpu = 0.0;
	int rss = 0;
	sscanf(buf, "%lf %d", &cpu, &rss);

	_cpuInfo.Update(cpu);
	_memoryInfo.Update(rss);
}
#endif

//////////////////////////////////////////////////////////////////////
// IPCMonitorServer

int GetProcessId()
{
	int processId = 0;

#ifdef _WIN32
	processId = GetProcessId(GetCurrentProcess());
#else
	processId = getpid();
#endif

	return processId;
}

#ifdef _WIN32
const char* SERVER_PIPE_NAME = "\\\\.\\Pipe\\PPServerPipeName";
#else
const char* SERVER_PIPE_NAME = "tmp/performance_profiler/_fifo";
#endif

string GetServerPipeName()
{
	string name = SERVER_PIPE_NAME;
	char idStr[10];
	_itoa(GetProcessId(), idStr, 10);
	name += idStr;
	return name;
}

IPCMonitorServer::IPCMonitorServer()
	:_onMsgThread(&IPCMonitorServer::OnMessage, this)
{
	printf("%s IPC Monitor Server Start\n", GetServerPipeName().c_str());

	_cmdFuncsMap["state"] = GetState;
	_cmdFuncsMap["save"] = Save;
	_cmdFuncsMap["disable"] = Disable;
	_cmdFuncsMap["enable"] = Enable;
}

void IPCMonitorServer::Start()
{}

void IPCMonitorServer::OnMessage()
{
	const int IPC_BUF_LEN = 1024;
	char msg[IPC_BUF_LEN] = { 0 };
	IPCServer server(GetServerPipeName().c_str());

	while (1)
	{
		server.ReceiverMsg(msg, IPC_BUF_LEN);
		printf("Receiver Cmd Msg: %s\n", msg);

		string reply;
		string cmd = msg;
		CmdFuncMap::iterator it = _cmdFuncsMap.find(cmd);
		if (it != _cmdFuncsMap.end())
		{
			CmdFunc func = it->second;
			func(reply);
		}
		else
		{
			reply = "Invalid Command";
		}

		server.SendReplyMsg(reply.c_str(), reply.size());
	}
}

void IPCMonitorServer::GetState(string& reply)
{
	reply += "State:";
	int flag = ConfigManager::GetInstance()->GetOptions();

	if (flag == PPCO_NONE)
	{
		reply += "None\n";
		return;
	}

	if (flag & PPCO_PROFILER)
	{
		reply += "Performance Profiler \n";
	}

	if (flag & PPCO_SAVE_TO_CONSOLE)
	{
		reply += "Save To Console\n";
	}

	if (flag & PPCO_SAVE_TO_FILE)
	{
		reply += "Save To File\n";
	}
}

void IPCMonitorServer::Enable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(PPCO_PROFILER | PPCO_SAVE_TO_FILE);

	reply += "Enable Success";
}

void IPCMonitorServer::Disable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(PPCO_NONE);

	reply += "Disable Success";
}

void IPCMonitorServer::Save(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(
		ConfigManager::GetInstance()->GetOptions() | PPCO_SAVE_TO_FILE);
	PerformanceProfiler::GetInstance()->OutPut();

	reply += "Save Success";
}

//////////////////////////////////////////////////////////////
// �����Ч������

//��ȡ·���������ļ�����
static string GetFileName(const string& path)
{
	char ch = '/';

#ifdef _WIN32
	ch = '\\';
#endif

	size_t pos = path.rfind(ch);
	if (pos == string::npos)
	{
		return path;
	}
	else
	{
		return path.substr(pos + 1);
	}
}

// PerformanceNode
PerformanceNode::PerformanceNode(const char* fileName, const char* function,
	int line, const char* desc)
	:_fileName(GetFileName(fileName))
	,_function(function)
	,_line(line)
	,_desc(desc)
{}

bool PerformanceNode::operator<(const PerformanceNode& p) const
{
	if (_line > p._line)
		return false;

	if (_line < p._line)
		return true;

	if (_fileName > p._fileName)
		return false;
	
	if (_fileName < p._fileName)
		return true;

	if (_function > p._function)
		return false;

	if (_function < p._function)
		return true;

	return false;
}

bool PerformanceNode::operator == (const PerformanceNode& p) const
{
	return _fileName == p._fileName
		&&_function == p._function
		&&_line == p._line;
}

void PerformanceNode::Serialize(SaveAdapter& SA) const
{
	SA.Save("FileName:%s, Fuction:%s, Line:%d\n",
		_fileName.c_str(), _function.c_str(), _line);
}

///////////////////////////////////////////////////////////////
//PerformanceProfilerSection
void PerformanceProfilerSection::Serialize(SaveAdapter& SA)
{
	// ���ܵ����ü���������0�����ʾ�����β�ƥ��
	if (_totalRef)
		SA.Save("Performance Profiler Not Match!\n");

	// ���л�Ч��ͳ����Ϣ
	auto costTimeIt = _costTimeMap.begin();
	for (; costTimeIt != _costTimeMap.end(); ++costTimeIt)
	{
		LongType callCount = _callCountMap[costTimeIt->first];
		SA.Save("Thread Id:%d, Cost Time:%.2f, Call Count:%d\n",
			 costTimeIt->first, 
			(double)costTimeIt->second / CLOCKS_PER_SEC,
			callCount);
	}

	SA.Save("Total Cost Time:%.2f, Total Call Count:%d\n",
		(double)_totalCostTime / CLOCKS_PER_SEC, _totalCallCount);

	// ���л���Դͳ����Ϣ
	if (_rsStatistics)
	{
		ResourceInfo cpuInfo = _rsStatistics->GetCpuInfo();
		SA.Save("��Cpu�� Peak:%lld%%, Avg:%lld%%\n", cpuInfo._peak, cpuInfo._avg);

		ResourceInfo memoryInfo = _rsStatistics->GetMemoryInfo();
		SA.Save("��Memory�� Peak:%lldK, Avg:%lldK\n", memoryInfo._peak / 1024, memoryInfo._avg / 1024);
	}
}

//////////////////////////////////////////////////////////////
// PerformanceProfiler
PerformanceProfilerSection* PerformanceProfiler::CreateSection(const char* fileName,
	const char* function, int line, const char* extraDesc, bool isStatistics)
{
	PerformanceProfilerSection* section = NULL;
	PerformanceNode node(fileName, function, line, extraDesc);

	unique_lock<mutex> Lock(_mutex);
	auto it = _ppMap.find(node);
	if (it != _ppMap.end())
	{
		section = it->second;
	}
	else
	{
		section = new PerformanceProfilerSection;
		if (isStatistics)
		{
			section->_rsStatistics = new ResourceStatistics();
		}
		_ppMap[node] = section;
	}

	return section;
}

void PerformanceProfilerSection::Begin(int threadId)
{
	unique_lock<mutex> Lock(_mutex);

	// ���µ��ô���ͳ��
	++_callCountMap[threadId];

	// ���ü��� == 0 ʱ���¶ο�ʼʱ��ͳ�ƣ���������ݹ��������⡣
	auto& refCount = _refCountMap[threadId];
	if (refCount == 0)
	{
		_beginTimeMap[threadId] = clock();

		// ��ʼ��Դͳ��
		if (_rsStatistics)
		{
			_rsStatistics->StartStatistics();
		}
	}

	// ���������ο�ʼ���������ü���ͳ��
	++refCount;
	++_totalRef;
	++_totalCallCount;
}

void PerformanceProfilerSection::End(int threadId)
{
	unique_lock<mutex> Lock(_mutex);

	// �������ü���
	LongType refCount = --_refCountMap[threadId];
	--_totalRef;

	//
	// ���ü��� <= 0 ʱ���������λ���ʱ�䡣
	// ��������ݹ���������������β�ƥ�������
	//
	if (refCount <= 0)
	{
		auto it = _beginTimeMap.find(threadId);
		if (it != _beginTimeMap.end())
		{
			LongType costTime = clock() - it->second;
			if (refCount == 0)
				_costTimeMap[threadId] += costTime;
			else
				_costTimeMap[threadId] = costTime;

			_totalCostTime += costTime;
		}

		// ֹͣ��Դͳ��
		if (_rsStatistics)
		{
			_rsStatistics->StopStatistics();
		}
	}
}

PerformanceProfiler::PerformanceProfiler()
{
	// �������ʱ����������
	atexit(OutPut);

	time(&_beginTime);

	IPCMonitorServer::GetInstance()->Start();
}

void PerformanceProfiler::OutPut()
{
	int flag = ConfigManager::GetInstance()->GetOptions();
	if (flag & PPCO_SAVE_TO_CONSOLE)
	{
		ConsoleSaveAdapter CSA;
		PerformanceProfiler::GetInstance()->_OutPut(CSA);
	}

	if (flag & PPCO_SAVE_TO_FILE)
	{
		FileSaveAdapter FSA("PerformanceProfilerReport.txt");
		PerformanceProfiler::GetInstance()->_OutPut(FSA);
	}
}

bool PerformanceProfiler::CompareByCallCount(PerformanceProfilerMap::iterator lhs,
	PerformanceProfilerMap::iterator rhs)
{
	return lhs->second->_totalCallCount > rhs->second->_totalCallCount;
}

bool PerformanceProfiler::CompareByCostTime(PerformanceProfilerMap::iterator lhs,
	PerformanceProfilerMap::iterator rhs)
{
	return lhs->second->_totalCostTime > rhs->second->_totalCostTime;
}

void PerformanceProfiler::_OutPut(SaveAdapter& SA)
{
	SA.Save("=============Performance Profiler Report==============\n\n");
	SA.Save("Profiler Begin Time: %s\n", ctime(&_beginTime));

	unique_lock<mutex> Lock(_mutex);

	vector<PerformanceProfilerMap::iterator> vInfos;
	auto it = _ppMap.begin();
	for (; it != _ppMap.end(); ++it)
	{
		vInfos.push_back(it);
	}

	// ������������������������������
	int flag = ConfigManager::GetInstance()->GetOptions();
	if (flag & PPCO_SAVE_BY_COST_TIME)
		sort(vInfos.begin(), vInfos.end(), CompareByCostTime);
	else if (flag & PPCO_SAVE_BY_CALL_COUNT)
		sort(vInfos.begin(), vInfos.end(), CompareByCallCount);

	for (int index = 0; index < vInfos.size(); ++index)
	{
		SA.Save("NO%d. Description:%s\n", index + 1, vInfos[index]->first._desc.c_str());
		vInfos[index]->first.Serialize(SA);
		vInfos[index]->second->Serialize(SA);
		SA.Save("\n");
	}

	SA.Save("==========================end========================\n\n");
}

