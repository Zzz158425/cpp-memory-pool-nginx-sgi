> 通过剖析SGI STL二级空间配置器内存池源码深入理解其实现原理

# SGI STL二级空间配置器源码解析

SGI STL（SGI为厂商名字）包含了一级空间配置器和二级空间配置器，其中一级空间配置器 allocator 采用 malloc 和free来管理内存，和 C++ 标准库中提供的 allocator 是一样的，但其二级空间配置器 allocator 采用了基于freelist自由链表原理的内存池机制实现内存管理。

## 空间配置器的相关定义

allocate:负责给容器开辟内存空间 -> malloc

deallocate:负责释放容器内存空间 -> free

construct:负责给容器构造一个对象 -> 定位new实现

destroy:负责析构容器的对象=> p->~T()

分离了对象的内存开辟，对象构造；分离了对象的析构，内存的释放

## 源码解析

```c++
template <class _Tp, class _Alloc = __STL_DEFAULT_ALLOCATOR(_Tp) >
class vector : protected _Vector_base<_Tp, _Alloc>
{
    ...
}
```

可以看到，容器的默认空间配置器是__STL_DEFAULT_ALLOCATOR( _Tp)，它是一个宏定义，如下：

```c++
# ifndef __STL_DEFAULT_ALLOCATOR
# 	ifdef __STL_USE_STD_ALLOCATORS
# 		define __STL_DEFAULT_ALLOCATOR(T) allocator< T >
# 	else
# 		define __STL_DEFAULT_ALLOCATOR(T) alloc
# 	endif
# endif
```

从上面可以看到 __STL_DEFAULT_ALLOCATOR 通过宏控制有两种实现，一种是 allocator< T >，另一种是 alloc，这两种分别就是 SGI STL 的一级空间配置器和二级空间配置器的实现。

```c++
template <int __inst>
class __malloc_alloc_template // 一级空间配置器内存管理类 -- 通过 malloc 和 free 管理内存
```

```c++
template <bool threads, int inst>
class __default_alloc_template { // 二级空间配置器内存管理类 -- 通过自定义内存池实现内存管理
```

### 一级空间配置器

容器底层存储的对象的构造与析构是调用定义的全局的模板函数 Construct 和 Destroy 完成的，如下：

```c++
void push_back(const _Tp& __x) {
    if (_M_finish != _M_end_of_storage) {
      construct(_M_finish, __x);
      ++_M_finish;
    }
	...
}
 void pop_back() {
    --_M_finish;
    destroy(_M_finish);
  }
```

```c++
template <class _T1>
inline void construct(_T1* __p) {
  _Construct(__p);// 利用定位 new 实现
}
template <class _Tp>
inline void destroy(_Tp* __pointer) {
  _Destroy(__pointer);//调用对象的析构函数实现
}
```

因此容器的空间配置器只有 allocate 与 deallocate 操作来进行内存的管理

### 二级空间配置器

#### 重要类型和变量定义

```c++
// 内存池的粒度信息
enum {_ALIGN = 8};// 以 8 字节对齐
enum {_MAX_BYTES = 128};// 最大 128 个字节，即超过 128 byte 就不再由内存池来管理，依旧通过 malloc 与 free 来管理
enum {_NFREELISTS = 16};// 自由链表个数 16
```

```c++
// 每一个内存 chunk 块的头信息
union _Obj {
union _Obj* _M_free_list_link;// 下一个节点的地址
char _M_client_data[1]; /* The client sees this. */
};
```

```c++
// 组织所有自由链表的数组，数组的每一个元素的类型是_Obj*，全部初始化为0

static _Obj* __STL_VOLATILE _S_free_list[_NFREELISTS];
```

```c++
// Chunk allocation state. 记录内存chunk块的分配情况
static char* _S_start_free;
static char* _S_end_free;
static size_t _S_heap_size;

template <bool __threads, int __inst>
char* __default_alloc_template<__threads, __inst>::_S_start_free = 0;

template <bool __threads, int __inst>
char* __default_alloc_template<__threads, __inst>::_S_end_free = 0;

template <bool __threads, int __inst>
size_t __default_alloc_template<__threads, __inst>::_S_heap_size = 0;
```

#### 重要的辅助接口函数 _S_round_up 与 _S_freelist_index

```c++
/*将 __bytes 上调至最邻近的 8 的倍数*/
static size_t _S_round_up(size_t __bytes){ 
    return (((__bytes) + (size_t) _ALIGN-1) & ~((size_t) _ALIGN - 1)); }
```

```c++
/*返回 __bytes 大小的chunk块位于 free-list 中的编号*/
static size_t _S_freelist_index(size_t __bytes) {
	return (((__bytes) + (size_t)_ALIGN-1)/(size_t)_ALIGN - 1); }
```

#### allocate 内存分配

定义二级指针 __my_free_list 用来遍历这个指针数组，根据所分配的内存大小定位到相应的串口块，通过指针解引用访问这个数组相应元素的值赋给 result，如果 result 为空，这个串口块从来没有被分配过，即该串口块没有任何的内存池，分配内存池；若相应串口块不空，指向自由链表的下一个空闲节点，当前节点返回

```c++
static void* allocate(size_t __n)
{
    void* __ret = 0;
	if (__n > (size_t) _MAX_BYTES) {
  		__ret = malloc_alloc::allocate(__n);
	}
	else {
        _Obj* __STL_VOLATILE* __my_free_list
            = _S_free_list + _S_freelist_index(__n);
        ifndef _NOTHREADS
        _Lock __lock_instance;// 线程安全
        endif
        _Obj* __RESTRICT __result = *__my_free_list;
        if (__result == 0)
         __ret = _S_refill(_S_round_up(__n));
        else {
            *__my_free_list = __result -> _M_free_list_link;
            __ret = __result;
            }
        }
	return __ret;
};
```
#### _S_refill 函数

1.分配相应指定大小的 chunk 块内存池

2.把整个 chunk 分为小的 chunk 并将每个小 chunk 块连接起来

```c++
__default_alloc_template<__threads, __inst>::_S_refill(size_t __n)
{
    int __nobjs = 20;
    char* __chunk = _S_chunk_alloc(__n, __nobjs);
    _Obj* __STL_VOLATILE* __my_free_list;
    _Obj* __result;
    _Obj* __current_obj;
    _Obj* __next_obj;
    int __i;
if (1 == __nobjs) return(__chunk);
__my_free_list = _S_free_list + _S_freelist_index(__n);
/* Build free list in chunk */
  __result = (_Obj*)__chunk;
  *__my_free_list = __next_obj = (_Obj*)(__chunk + __n);
  for (__i = 1; ; __i++) {
    __current_obj = __next_obj;
    __next_obj = (_Obj*)((char*)__next_obj + __n);
    if (__nobjs - 1 == __i) {
        __current_obj -> _M_free_list_link = 0;
        break;
    } else {
        __current_obj -> _M_free_list_link = __next_obj;
    }
  }
return(__result);
}
```
#### _S_chunk_alloc 函数

1.内存 > 128，直接通过 malloc 进行分配

2.内存 <= 128，通过内存池进行

​	2.1.内存池余量充足，直接从池中划出所需大小

​	2.2.内存池余量不足够但至少能分 1 个，池中剩下的不够请求数但至少能分 1 个，把池中最后这点内存全部分配出去，避免浪费。

​	2.3.内存池余量连 1 个都满足不了

​		2.3.1.若池中有零头，虽然不够一个 block，但也不能丢弃。计算它属于哪个 free list，挂回到对应索引的 free list 中，实现零内存浪费。

​		2.3.2.向堆申请新内存，采用动态增长策略 — 已分配越多，下次多申请越多，减少系统调用次数

​		2.3.3.如果 malloc 返回 nullptr，不放弃，遍历比自己大的 free list，搜到第一个非空的更大 free list，把整条链表的第一个 block 挖过来作为新的内存池，然后递归调用自身。

​		2.3.4.一级分配器内部会调用 malloc，若失败则尝试用户注册的 OOM handler，如果 handler 也无法释放内存则抛 bad_alloc 异常。

​		2.3.5.申请成功，更新 _S_heap_size，设置 _S_end_free，然后递归调用自身。此时再进来大概率走情况 1 或 2，完成分配。

##### 内存释放，deallocate 函数

1.chunk 块 > 128 直接 free

2.chunk 块 <= 128，往内存池进行归还，把要归还的节点插入到对应指针数组中的静态链表，此时，对应的指针数组中的指针指向该节点，该节点的地址域指向原本静态链表中的第一个节点

##### 扩/缩容，reallocate 函数

1.原 chunk 块 > 128 && 新 chunk 块 > 128，调用库函数提供的 realloc

2.原 chunk 块与新 chunk 块均在同一字节下分配，不需要扩容与缩容，直接返回

3.进行扩/缩容，规范原来的 chunk 块，返回扩/缩后的 chunk 地址

```c++
// 分配内存的入口函数
static void* allocate(size_t __n)

// 负责把分配好的 chunk 块进行连接，添加到自由链表当中
static void* _S_refill(size_t __n);

// 分配相应内存字节大小的 chunk 块，并且给下面三个成员变量初始化
static char* _S_chunk_alloc(size_t __size, int& __nobjs);

// 把chunk块归还到内存池
static void deallocate(void* __p, size_t __n);

// 内存池扩容函数
template <bool threads, int inst>
void*
__default_alloc_template<threads, inst>::reallocate(void* __p,
                                                    size_t __old_sz,
                                                    size_t __new_sz);
```

#### SGI STL 二级空间配置器内存池的实现优点：

1.对于每一个字节数的 chunk 块分配，都是给出一部分进行使用，另一部分作为备用，这个备用可以给当前
字节数使用，也可以给其它字节数使用。

2.对于备用内存池划分完 chunk 块以后，如果还有剩余的很小的内存块，再次分配的时候，会把这些小的内
存块再次分配出去，备用内存池使用的干干净净！

3.当指定字节数内存分配失败以后，有一个异常处理的过程，bytes-128 字节所有的 chunk 块进行查看，如果
哪个字节数有空闲的 chunk 块，直接借一个出去如果上面操作失败，还会调用一直循环调用 oom_malloc 这么一个预先设置好的 malloc 内存分配失败以后的回调函数（此处 oom_malloc 不恰当可能会造成死循环），若没设置 mallocthrow bad_alloc 则直接抛出错误。

## 使用内存池好处

防止小块内存频繁的分配，释放，造成内存很多的碎片出来，内存没有更多的连续的大内存块。所以应用对于小块
内存的操作，一般都会使用内存池来进行管理。

![](../../Git/image/SGI STL内存池源码讲解上课图示.png)

> 剖析nginx的内存池源码，讲解原理实现以及该内存池设计的应用场景

# Nginx 内存池源码解析

## 重要类型定义

```c++
// nginx内存池的主结构体类型
struct ngx_pool_s {
	ngx_pool_data_t d; // 内存池的数据头
	size_t max; // 小块内存分配的最大值
	ngx_pool_t *current; // 小块内存池入口指针
	ngx_chain_t *chain;
	ngx_pool_large_t *large; // 大块内存分配入口指针
	ngx_pool_cleanup_t *cleanup; // 清理函数handler的入口指针
	ngx_log_t *log;
};
```

```c++
typedef struct ngx_pool_s ngx_pool_t;
// 小块内存数据头信息
typedef struct {
	u_char *last; // 可分配内存开始位置
	u_char *end; // 可分配内存末尾位置
	ngx_pool_t *next; // 保存下一个内存池的地址
	ngx_uint_t failed; // 记录当前内存池分配失败的次数
} ngx_pool_data_t;
```

```c++
typedef struct ngx_pool_large_s ngx_pool_large_t;
// 大块内存类型定义
struct ngx_pool_large_s {
	ngx_pool_large_t *next; // 下一个大块内存
	void *alloc; // 记录分配的大块内存的起始地址
};
```

```c++
typedef void (*ngx_pool_cleanup_pt)(void *data); // 清理回调函数的类型定义
typedef struct ngx_pool_cleanup_s ngx_pool_cleanup_t;
// 清理操作的类型定义，包括一个清理回调函数，传给回调函数的数据和下一个清理操作的地址
struct ngx_pool_cleanup_s {
	ngx_pool_cleanup_pt handler; // 清理回调函数
	void *data; // 传递给回调函数的指针
	ngx_pool_cleanup_t *next; // 指向下一个清理操作
};
```

## nginx内存池重要函数接口总结

```c++
ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log); // 创建内存池
void ngx_destroy_pool(ngx_pool_t *pool); // 销毁内存池
void ngx_reset_pool(ngx_pool_t *pool); // 重置内存池
void *ngx_palloc(ngx_pool_t *pool, size_t size); // 内存分配函数，支持内存对齐
void *ngx_pnalloc(ngx_pool_t *pool, size_t size); // 内存分配函数，不支持内存对齐
void *ngx_pcalloc(ngx_pool_t *pool, size_t size); // 内存分配函数，支持内存初始化0
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p // 内存释放（大块内存）
ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, size_t size); // 添加清理
handler
```

### ngx_create_pool 函数：初始化内存池

1.分配原始内存块：ngx_memalign(16, size, log)，一次性连头带数据一起分配

2.设置所有字段初值：last/end/max/current、各种链表指针置 NULL、failed 归零

3.确定分配策略参数：算出 max，作为后续小/大内存分配的分界创建完成后池就处于可用状态，可以直接调 ngx_palloc 分配内存。

### ngx_palloc 函数：内存池的核心分配

1.当申请的内存大小小于等于 max（以一个页面大小 4095 为分界线），进行小块内存分配

​	1.1.ngx_palloc_small：检查当前内存块是否够分配，不够进入 ngx_palloc_block(pool, size) 分配新块

​	1.2.ngx_palloc_block：它按首块大小分配一块新的对齐内存，将 last 指针跳到 ngx_pool_data_t 头部之后并直接嵌入本次分配的结果，然后遍历从 current 到链表尾的所有块，对每个已满块的 failed 计数加 1，当某块 failed > 4 时将 current 指针后移以跳过它，最后把新块挂到链表末尾，返回分配到的内存地址。

2.当申请的内存大小大于 max（以一个页面大小 4095 为分界线），进行大块内存分配

​	2.1.ngx_palloc_large：处理超过 max 的大内存分配：直接 malloc 一块内存，优先在 large 链表前 4 个节点中找已释放的空槽（alloc == NULL，有可能存在某一大块内存被 ngx_pfree 释放）来挂载它，找不到则从池内小分配拿一个 ngx_pool_large_t 节点，用头插法挂入链表。

### ngx_reset_pool 函数：内存池重置

1.重置池到刚创建的状态：释放所有大块内存，把所有数据块的 last 指针拉回头部、failed 归零，池内小块空间全部复用，但不释放池块本身。注意：除第一块内存外，后续小内存块无 ngx_pool_s，此处重置直接把 last 指针指向了 ngx_pool_t（ngx_pool_data_t + ngx_pool_s） 会浪费一定的内存（max -> log），但也无错。

2.nginx大块内存分配 -> 内存释放 ngx_free 函数

3.nginx小块内存分配 -> 没有提供任何的内存释放函数，实际上，从小块内存的分配方式来看（直接通过 last 指针偏移来分配内存），它也没法进行小块内存的回收。

4.nginx 本质：http 服务器是一个短链接的服务器，客户端（浏览器）发起一个 request 请求，到达 nginx 服务器以后，处理完成，nginx 给客户端返回一个 response 响应，http  服务器就主动断开 tcp 连接（http 1.1 keep-avlie:60s) http 服务器（nginx）返回响应以后，需要等待 60s，60s 之内客户端又发来请求，重置这个时间，否则 60s 之内没有客户端发来的响应，nginx 就主动断开连接，此时 nginx 可以调用 ngx_reset_pool 重置内存池了，等待下一次该客户端的请求。所以 Nginx 内存池使用有局限性。

### ngx_pool_cleanup_add：添加清理

注册一个清理回调节点。调用者拿到 ngx_pool_cleanup_t 后设置 c -> handler = 自己的清理函数，当池销毁时该函数会被自动调用，主要用于自动关闭文件描述符、释放第三方库资源等，防止内存泄漏。

### ngx_destroy_pool：销毁内存池

先执行清理回调（释放外部资源），再释放所有大块内存，最后把整个池的数据块链全部 free 归还系统。销毁后 pool 指针不可再用。

```c++
void ngx_destroy_pool(ngx_pool_t *pool)
{
    ngx_pool_t          *p, *n;
    ngx_pool_large_t    *l;
    ngx_pool_cleanup_t  *c;

    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "run cleanup: %p", c);
            c->handler(c->data);
        }
    }
    ...
}
//自己实现的外部资源释放函数
void release(void *p)
{
    free(p);
}
ngx_pool_cleanup_t *pclean = ngx_pool_cleanup_add(pool, sizeof(char*));
pclean->handler = &release;
pclean->data = pData->p;
//c->handler != nullptr
c->handler(c->data);//release(pData->p)

```

![](../../Git/image/nginx内存池代码剖析上课图例.png)
