#include "SGI STL二级空间配置器内存池移植.h"
#include <vector>

//_S_round_up 2026.5.14
enum { _ALIGN = 8 };
static size_t _S_round_up(size_t __bytes)
{
	return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
}

//_S_freelist_index
static size_t _S_freelist_index(size_t __bytes)
{
	return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
}

int main()
{
//_S_round_up
	//_S_round_up：将 __bytes 上调至最临近的 8 的倍数
	//1 - 8 -> 8
	//9 - 16 -> 16
	//~((size_t)_ALIGN - 1))
		//00000000 00000000 00000000 00001000 -> 11111111 11111111 11111111 11111000
	//(((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1))
		//00000000 00000000 00000000 00000111 + __bytes(1 - 8)
		//11111111 11111111 11111111 11111000
		//0 ... 00000111 + __bytes 在 0 ... 00001000 ~ 0 ... 0 00001111 之间，经过 & 后均为 0 ... 00001000，从而实现 1 - 8 -> 8
	std::cout << ~(_ALIGN - 1) << std::endl;//_ALIGN 1 个字节
	std::cout << ~((size_t)_ALIGN - 1) << std::endl;//经过 (size_t) 强转后为 4 字节
	std::cout << _S_round_up(0) << std::endl;

//_S_freelist_index
		//_S_freelist_index：返回 __bytes 大小的 chunk 块位于 free-list 中的编号
		//1 - 8 -> 0
		//9 - 16 -> 1

//SGI STL二级空间配置器内存池移植测试 2026.5.17
	std::cout << _S_freelist_index(1) << std::endl;
	std::vector<int, myallocator<int>> vec;
	for (int i = 0; i < 100; i++)
	{
		vec.push_back(rand() % 1000);
	}
	for (int val : vec)
	{
		std::cout << val << " ";
	}
	std::cout << std::endl;

	return 0;
}