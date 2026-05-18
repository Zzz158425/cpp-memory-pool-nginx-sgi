#include <ngx_config.h>
#include <nginx.h>
#include <ngx_core.h>
#include <ngx_palloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
            const char *fmt, ...)
{

}

typedef struct Data stData;
struct Data
{
    char *ptr;
    FILE *pfile;
};

void func1(char *p)
{
    printf("free ptr mem!");
    free(p);
}
void func2(FILE *pf)
{
    printf("close file!");
    fclose(pf);
}
void main()
{
	// 512 - sizeof(ngx_pool_t) - 4095   =>   max
    ngx_pool_t *pool = ngx_create_pool(512, NULL);
    if(pool == NULL)
    {
        printf("ngx_create_pool fail...");
        return;
    }

    void *p1 = ngx_palloc(pool, 128); // 从小块内存池分配的
    if(p1 == NULL)
    {
        printf("ngx_palloc 128 bytes fail...");
        return;
    }

    stData *p2 = ngx_palloc(pool, 512); // 从大块内存池分配的
    if(p2 == NULL)
    {
        printf("ngx_palloc 512 bytes fail...");
        return;
    }
    p2->ptr = malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");
    
    ngx_pool_cleanup_t *c1 = ngx_pool_cleanup_add(pool, sizeof(char*));
    c1->handler = func1;
    c1->data = p2->ptr;

    ngx_pool_cleanup_t *c2 = ngx_pool_cleanup_add(pool, sizeof(FILE*));
    c2->handler = func2;
    c2->data = p2->pfile;

    ngx_destroy_pool(pool); // 1.调用所有的预置的清理函数 2.释放大块内存 3.释放小块内存池所有内存

    return;
}
