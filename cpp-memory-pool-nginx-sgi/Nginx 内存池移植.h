#pragma once
#include<stdlib.h>
#include<memory.h>

//移植 Nginx 内存池，用 OOP 来实现
using u_char = unsigned char;
using ngx_uint_t = unsigned int;

struct ngx_pool_cleanup_s;
struct ngx_pool_large_s;
struct ngx_pool_data_t;
struct ngx_pool_s;

const int ngx_pagesize = 4096;//默认一个物理页面大小
const int NGX_MAX_ALLOC_FROM_POOL = (ngx_pagesize - 1);//小块内存池可分配的最大空间
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;//16k
const int NGX_POOL_ALIGNMENT = 16;

//清理函数（回调函数）的类型
typedef void (*ngx_pool_cleanup_pt)(void* data);
struct ngx_pool_cleanup_s
{
    ngx_pool_cleanup_pt handler;//定义了一个函数指针，保存清理操作的回调函数
    void* data;//传递给回调函数的参数
    ngx_pool_cleanup_s* next;//所有 cleanup 清理操作都被串在一条链表上
};

//分配大块内存的内存池的头部数据信息
struct ngx_pool_large_s
{
    ngx_pool_large_s* next;//所有的大块内存分配也是被串在一条链表上
    void* alloc;//保存分配出去的大块内存的起始地址
};

//分配小块内存的内存池的头部数据信息
struct ngx_pool_data_t
{
    u_char* last;//小块内存池可用内存的起始地址
    u_char* end;//小块内存池可用内存的末始地址
    ngx_pool_s* next;//所有小块内存池都被串在了一条链表上
    ngx_uint_t failed;//记录当前小块内存池分配内存失败的次数
};

//ngx 内存池的头部信息和管理成员信息
struct ngx_pool_s
{
    ngx_pool_data_t d;//存储的是当前小块内存池的使用情况
    size_t max;//存储的是小块内存和大块内存的分界线
    ngx_pool_s* current;//指向第一个提供小块内存分配的小块内存池
    ngx_pool_large_s* large;//指向大块内存（链表）的入口地址
    ngx_pool_cleanup_s* cleanup;//指向所有预置的清理操作回调函数（链表）的入口
};

#define NGX_ALIGNMENT sizeof(unsigned long)//小块内存分配考虑字节对齐时的单位
#define ngx_align(d, a) (((d) + (a - 1)) & ~(a - 1))//把数值 d 调整到临近 a 的倍数

//内存池大小按照 16 字节进行对齐 NGX_POOL_ALIGNMENT 的临近的倍数
const int NGX_MIN_POOL_SIZE =
ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)),
    NGX_POOL_ALIGNMENT);
//把指针 p 调整到 a 的临近的倍数
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define ngx_memzero(buf, n) (void) memset(buf, 0, n)//buff 缓冲区清零

//nginx 内存池
class ngx_mem_pool
{
public:
    //创建指定 size 大小的内存池，但是小块内存池不超过 1 个页面的大小
    void* ngx_create_pool(size_t size);

    //考虑内存字节对齐，从内存池申请 size 大小的内存
    void* ngx_palloc(size_t size);

    //不考虑内存字节对齐，从内存池申请 size 大小的内存
    void* ngx_pnalloc(size_t size);

    //调用的是 ngx_palloc 实现内存分配，但是会初始化 0
    void* ngx_pcalloc(size_t size);

    //释放大块内存
    void ngx_pfree(void* p);

    //内存重置
    void ngx_reset_pool();

    //内存池的销毁
    void ngx_destroy_pool();

    //添加回调清理操作函数
    ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);
private:
    ngx_pool_s* pool;//指向 ngx 内存池的入口指针
    //小块内存分配
    void* ngx_palloc_small(size_t size, ngx_uint_t align);

    //大块内存分配
    void* ngx_palloc_large(size_t size);

    //分配新的小块内存池
    void* ngx_palloc_block(size_t size);
};