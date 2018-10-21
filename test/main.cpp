
#include <exception>
#include <vector>
#include <chrono>
#include <iostream>
#include <utility>

#include "ScopeGuard.h"

//#define FOLLY_EXCEPTION_COUNT_USE_STD
//#include "folly/ScopeGuard.h"

#ifdef _DEBUG
static constexpr size_t vecSize = 1000'000;
#else
static constexpr size_t vecSize = 1000'000'000;
#endif // _DEBUG


static std::vector<int> makeVector()
{
	std::vector<int> v;
	v.reserve(vecSize);
	std::srand(uint32_t(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
	std::cout << "Populating vector of " << vecSize << " integers\n";
	for (int i = 0; i < vecSize; ++i)
	{
		v.push_back(std::rand());
	}
	return v;
}

static const std::vector<int> myData = makeVector();

inline bool getCheckedCstyle(size_t ind, int& value)
{
	if (ind >= vecSize) return false;
	value = myData[ind];
	return true;
}

inline const int& get(size_t ind)
{
	if (ind >= vecSize) throw std::out_of_range("f");
	return myData[ind];
}

inline std::pair<int, bool> getCheckedCppstyle(size_t ind)
{
	if (ind >= vecSize) return { 0,false };
	return { myData[ind] , true };
}

int test(int param = 0)
{

	//"C-style" checking
	std::int64_t summ = 0;
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	
	size_t sz = vecSize + param;
	for (size_t i = 0; i < sz; ++i)
	{
		int buff = 0;
		const auto res = getCheckedCstyle(i, buff);
		if (!res) break;
		summ += buff;
	}

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Summ: " << summ << "\n"
		<< "getCheckedCstyle: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms.\n\n";


	//"C++ style" checking
	summ = 0;
	start = std::chrono::steady_clock::now();

	for (size_t i = 0; i < sz; ++i)
	{
		const auto res = getCheckedCppstyle(i);
		if (!res.second) break;
		summ += res.first;
	}
	
	end = std::chrono::steady_clock::now();
	std::cout << "Summ: " << summ << "\n"
		<< "getCheckedCppstyle: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms.\n\n";



	//exeption checking
	summ = 0;
	start = std::chrono::steady_clock::now();

	try
	{
		for (size_t i = 0; i < sz; ++i)
		{
			summ += get(i);
		}
	}
	catch (...)
	{
		std::cout << "Thrown! Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms.\n";
	}
	end = std::chrono::steady_clock::now();
	std::cout << "Summ: " << summ << "\n"
		<< "get + try/catch: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms.\n\n";


	//scope_guard style checking
	summ = 0;
	start = std::chrono::steady_clock::now();

	{
		SCOPE_FAIL{ std::cout << "Thrown! Time:" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms.\n\n"; };
		for (size_t i = 0; i < sz; ++i)
		{
			summ += get(i);
		}
	}
	//catch (...)
	//{
	//	std::cout << "Thrown!\n" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms.\n";
	//}
	end = std::chrono::steady_clock::now();
	std::cout << "Summ: " << summ << "\n"
		<< "get + SCOPE_FAIL: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms.\n\n";


	return 0;
}


int main()
{

	int a = 1;

	do
	{
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

		if (a == 1)
		{
			std::cout << "\nTesting noexept\n";
			test();
		}
		else
		{
			std::cout << "Testing with exept\n";
			try
			{
				test(a);
			}
			catch (...)
			{
				std::cout << "Caught!\n";
			}
		}

		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "\n";

		std::cout << "\n";
		std::cout << "Continue? 1 - noexept, 2 - exept, 0 - stop\n";
		std::cin >> a;
	} while (a != 0);

	return 0;
}