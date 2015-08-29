#include<iostream>
using namespace std;

// C++11
#include<thread>

#ifdef _WIN32
//
// �����̬�⵼����̬���������⣬UMM�еĵ�����Ϊ��̬����
// ��̬�ⷽʽʹ��ʱ�����IMPORT����̬��ʱȥ���������޷������ӳ���
//
	#ifndef IMPORT
		#define IMPORT
	#endif // !IMPORT
	#pragma comment(lib, "../Debug/PerformanceProfiler.lib")
#endif // _WIN32

#include "../PerformanceProfiler/PerformanceProfiler.h"

// 1.���Ի�������
void Test1()
{
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	PERFORMANCE_PROFILER_EE_END(PP1);

	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	PERFORMANCE_PROFILER_EE_END(PP2);
}

void MutilTreadRun(int count)
{
	while (count--)
	{
		PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		PERFORMANCE_PROFILER_EE_END(PP1);

		PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

		std::this_thread::sleep_for(std::chrono::milliseconds(800));

		PERFORMANCE_PROFILER_EE_END(PP2);
	}
}

// 2.���Զ��̳߳���
void Test2()
{
	thread t1(MutilTreadRun, 5);
	thread t2(MutilTreadRun, 4);
	thread t3(MutilTreadRun, 3);

	t1.join();
	t2.join();
	t3.join();
}

// 3.���������β�ƥ�䳡��
void Test3()
{
	// 2.����ƥ��
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "ƥ��");

	for (int i = 0; i < 10; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	PERFORMANCE_PROFILER_EE_END(PP1);

	// 2.��ƥ��
	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "��ƥ��");

	for (int i = 0; i < 10; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		PERFORMANCE_PROFILER_EE_END(PP2);
	}
}

int Fib(int n)
{
	PERFORMANCE_PROFILER_EE_BEGIN(Fib2, "�����ݹ�");

	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	int ret;
	if (n <= 1)
	{
		ret = n;
	}
	else
	{
		ret = Fib(n - 1) + Fib(n - 2);
	}

	PERFORMANCE_PROFILER_EE_END(Fib2);

	return ret;
}

// 4.���������ݹ����
void Test4()
{
	PERFORMANCE_PROFILER_EE_BEGIN(Fib1, "����");

	Fib(10);

	PERFORMANCE_PROFILER_EE_END(Fib1);
}

// 5.���Ի�����Դͳ�����
void Test5()
{
	PERFORMANCE_PROFILER_EE_RS_BEGIN(PP1, "PP1");

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	PERFORMANCE_PROFILER_EE_RS_END(PP1);

	PERFORMANCE_PROFILER_EE_RS_BEGIN(PP2, "PP2");

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	PERFORMANCE_PROFILER_EE_RS_END(PP2);
}

//
// http://www.cnblogs.com/Ripper-Y/archive/2012/05/19/2508511.html
// 6.CPUռ��������������������Դͳ��
//
void Test6()
{
	SetThreadAffinityMask(GetCurrentProcess(), 0x00000001);
	const double SPLIT = 0.01;
	const int COUNT = 200;
	const double PI = 3.14159265;
	const int INTERVAL = 300;
	DWORD busySpan[COUNT]; //array of busy time  
	DWORD idleSpan[COUNT]; //array of idle time  
	int half = INTERVAL / 2;
	double radian = 0.0;
	for (int i = 0; i < COUNT; i++)
	{
		busySpan[i] = (DWORD)(half + (sin(PI*radian)*half));
		idleSpan[i] = INTERVAL - busySpan[i];
		radian += SPLIT;
	}
	DWORD startTime = 0;
	int j = 0;
	while (true)
	{
		PERFORMANCE_PROFILER_EE_RS_BEGIN(PERFORMANCE_PROFILER_EE_RS, "PERFORMANCE_PROFILER_EE_RS");

		j = j%COUNT;
		startTime = GetTickCount();
		while ((GetTickCount() - startTime) <= busySpan[j]);

		Sleep(idleSpan[j]);
		j++;

		PERFORMANCE_PROFILER_EE_RS_END(PERFORMANCE_PROFILER_EE_RS);
		PerformanceProfiler::GetInstance()->OutPut();

		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

//
// 7.�������߿����������
// ����Test7�Ժ�cmd������PerformanceProfilerTool.exe -pid ���̺� ���ɲ������߿���
//
void Test7()
{
	Test6();
}

//
// 8.�������������㷨�������������棬Ȼ����в鿴��
//
int GetMidIndex(int* array, int left, int right)
{
	//return right;

	int mid = left + ((right - left) >> 1);

	if (array[left] < array[right])
	{
		if (array[right] < array[mid])
			return right;
		else if (array[left] > array[mid])
			return left;
		else
			return mid;
	}
	else
	{
		if (array[right] > array[mid])
			return right;
		else if (array[left] < array[mid])
			return left;
		else
			return mid;
	}
}

int Partion(int* array, int left, int right)
{
	PERFORMANCE_PROFILER_EE_BEGIN(Partion, "Partion");

	//
	// prevָ���key���ǰһ��λ��
	// cur��ǰѰ�ұ�keyС�����ݡ�
	//

	PERFORMANCE_PROFILER_EE_BEGIN(GetMidIndex, "GetMidIndex");

	int keyIndex = GetMidIndex(array, left, right);
	if (keyIndex != right)
	{
		swap(array[keyIndex], array[right]);
	}

	PERFORMANCE_PROFILER_EE_END(GetMidIndex);


	int key = array[right];
	int prev = left - 1;
	int cur = left;

	while (cur < right)
	{
		// �ҵ���key��С����������ǰ������ݽ��н���
		if (array[cur] < key && ++prev != cur)
		{
			swap(array[cur], array[prev]);
		}

		++cur;
	}

	swap(array[++prev], array[right]);

	PERFORMANCE_PROFILER_EE_END(Partion);

	return prev;
}

void InsertSort(int* array, int size)
{
	assert(array);

	for (int index = 1; index < size; ++index)
	{
		// ����ǰ������ǰ����
		int insertIndex = index - 1;
		int tmp = array[index];
		while (insertIndex >= 0 && tmp < array[insertIndex])
		{
			array[insertIndex + 1] = array[insertIndex];
			--insertIndex;
		}

		// ע�������λ��
		array[insertIndex + 1] = tmp;
	}
}

void QuickSort(int* array, int left, int right)
{
	PERFORMANCE_PROFILER_EE_BEGIN(QuickSort, "QuickSort_NonOP");

	if (left < right)
	{
		PERFORMANCE_PROFILER_EE_BEGIN(Partion, "Partion_NonOP");

		int boundary = Partion(array, left, right);

		PERFORMANCE_PROFILER_EE_END(Partion);

		QuickSort(array, left, boundary - 1);
		QuickSort(array, boundary + 1, right);
	}

	PERFORMANCE_PROFILER_EE_END(QuickSort);
}

void QuickSort_OP(int* array, int left, int right)
{
	PERFORMANCE_PROFILER_EE_BEGIN(QuickSort, "QuickSort_OP");

	if (left < right)
	{
		int gap = right - left;

		if (gap < 13)
		{
			PERFORMANCE_PROFILER_EE_BEGIN(InsertSort, "InsertSort");

			InsertSort(array + left, gap + 1);

			PERFORMANCE_PROFILER_EE_END(InsertSort);
		}
		else
		{
			PERFORMANCE_PROFILER_EE_BEGIN(Partion, "Partion_OP");

			int boundary = Partion(array, left, right);

			PERFORMANCE_PROFILER_EE_END(Partion);

			QuickSort_OP(array, left, boundary - 1);
			QuickSort_OP(array, boundary + 1, right);
		}
	}

	PERFORMANCE_PROFILER_EE_END(QuickSort);
}

void Test8()
{
	const int num = 100000;
	int a1[num] = { 0 };
	int a2[num] = { 0 };
	srand(time(0));
	for (int i = 0; i < num; ++i)
	{
		int x = rand() % num;
		a1[i] = x;
		a2[i] = x;
	}

	//
	// �����Ա��Ż���δ�Ż��Ŀ�������
	// ������������Ż��Ժ�Ŀ��������δ�Ż��Ŀ�������Ч�ʡ�
	// ���յ����������ʾ�Ż��Ժ�����������5��+��
	//

	QuickSort(a1, 0, num - 1);
	QuickSort_OP(a2, 0, num - 1);
}

int main()
{
	SET_PERFORMANCE_PROFILER_OPTIONS(
		PPCO_PROFILER | PPCO_SAVE_TO_CONSOLE | PPCO_SAVE_BY_COST_TIME);

	//Test1();
	//Test2();
	//Test3();
	//Test4();
	//Test5();
	//Test6();
	//Test7();
	Test8();

	return 0;
}