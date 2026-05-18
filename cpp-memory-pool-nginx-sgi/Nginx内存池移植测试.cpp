#include <stdio.h>
#include <iostream>
#include <string>
#include "Nginx 内存池移植.h"

/*
struct Data
{
    char* ptr;
    FILE* pfile;
};

void func1(void* p1)
{
    char* p = (char*)p1;
    printf("free ptr mem!");
    free(p);
}

void func2(void* pf1)
{
    FILE* pf = (FILE*)pf1;
    printf("close file!");
    fclose(pf);
}

int main()
{
    //ngx_create_pool 的代码逻辑可以直接实现在 mempool 的构造函数中
    //ngx_destroy_pool 的代码逻辑可以直接实现在 mempool 的析构函数中 
    //512 - sizeof(ngx_pool_t) - 4095   =>   max
    ngx_mem_pool mempool;
    if (mempool.ngx_create_pool(512) == nullptr)
    {
        std::cout << "ngx_create_pool fail..." << std::endl;
        return -1;
    }

    //从小块内存池分配的
    if (mempool.ngx_palloc(128) == nullptr)
    {
        std::cout << "ngx_palloc 128 bytes fail..." << std::endl;
        return -1;
    }

    //从大块内存池分配的
    Data* p2 = (Data*)mempool.ngx_palloc(512);
    if (p2 == nullptr)
    {
        std::cout << "ngx_palloc 512 bytes fail..." << std::endl;
        return -1;
    }

    p2->ptr = (char*)malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");

    ngx_pool_cleanup_s* c1 = mempool.ngx_pool_cleanup_add(sizeof(char*));
    c1->handler = (ngx_pool_cleanup_pt)func1;
    c1->data = p2->ptr;

    ngx_pool_cleanup_s* c2 = mempool.ngx_pool_cleanup_add(sizeof(FILE*));
    c2->handler = (ngx_pool_cleanup_pt)func2;
    c2->data = p2->pfile;

    mempool.ngx_destroy_pool(); // 1.调用所有的预置的清理函数 2.释放大块内存 3.释放小块内存池所有内存

    return 0;
}
*/


