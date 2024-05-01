#include "thread_init.h"

#include <cstdlib>
#include <cstring>


pthread_key_t THR_KEY;
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
    pthread_mutexattr_t my_fast_mutexattr;
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
    pthread_mutexattr_t my_errorcheck_mutexattr;
#endif

uint64_t my_thread_stack_size= (sizeof(void*) <= 4)? 65536: ((256-16)*1024);

pthread_mutex_t THR_LOCK_threads;
pthread_cond_t THR_COND_threads;

// 统计有多少个线程经过了 my_thread_init函数初始化
unsigned int THR_thread_count= 0;
static uint64_t thread_id= 0; 

// 标志 my_thread_global_init 函数是否已经调用过
static bool my_thread_global_init_done = false;

void my_mutex_init()
{
    /* Initialize mutex attributes */
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
    /*Set mutex type to "fast" a.k.a "adaptive"
    
    In this case the thread may steal the mutex from some other thread
    that is waiting for the same mutex.  This will save us some
    context switches but may cause a thread to 'starve forever' while
    waiting for the mutex (not likely if the code within the mutex is
    short).
    */
   
   pthread_mutexattr_init(&my_fast_mutexattr);
   pthread_mutexattr_settype(&my_fast_mutexattr, PTHREAD_MUTEX_ADAPTIVE_NP);
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
    /* Set mutex type to "errorcheck" */
    pthread_mutexattr_init(&my_errorcheck_mutexattr);
    pthread_mutexattr_settype(&my_errorcheck_mutexattr, PTHREAD_MUTEX_ERRORCHECK);
#endif
}

void my_mutex_end()
{
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
    pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
    pthread_mutexattr_destroy(&my_errorcheck_mutexattr);
#endif
}


struct st_my_thread_var* _my_thread_var()
{
    return (struct st_my_thread_var*)pthread_getspecific(THR_KEY);
}

int set_thread_var(struct st_my_thread_var *var)
{
    return pthread_setspecific(THR_KEY, (void*)var);
}


char *strmake(register char *dst, register const char *src, size_t length)
{
    while (length--)
    {
        if (! (*dst++ = *src++))
        {
            return dst-1;
        }
    }
    *dst=0;
    return dst;
}

const char *my_thread_name()
{
    char name_buff[100];
    struct st_my_thread_var *tmp = my_thread_var;
    // 如果tmp是NULL, 这个tmp->name能正常工作???
    if (!tmp->name[0])
    {
        uint64_t id = tmp ? tmp->id : 0;
        sprintf(name_buff,"T@%lu", id);
        strmake(tmp->name, name_buff, sizeof(tmp->name));
    }
    return tmp->name;
}

bool my_thread_init()
{
    struct st_my_thread_var *tmp;

    if (my_thread_var)
    {
        return true;
    }

    // 分配线程私有变量的内存;
    if (!(tmp= (struct st_my_thread_var *) calloc(1, sizeof(*tmp))))
    {
        return false;
    }
    
    int rc = set_thread_var(tmp);
    if(rc != 0)
    {
        return false;
    }

    tmp->pthread_self= pthread_self();
    pthread_mutex_init(&tmp->mutex, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&tmp->suspend, NULL);
    
    tmp->stack_ends_here= (char*)&tmp + STACK_DIRECTION * (long)my_thread_stack_size;
    
    pthread_mutex_lock(&THR_LOCK_threads);
    tmp->id = ++thread_id;
    ++THR_thread_count;
    pthread_mutex_unlock(&THR_LOCK_threads);
    tmp->init= 1;
    
    return true;
}

bool my_thread_end()
{
    struct st_my_thread_var *tmp;
    tmp= my_thread_var;
    
    set_thread_var(NULL);
    
    if (tmp && tmp->init)
    {
        pthread_mutex_destroy(&tmp->mutex);
        pthread_cond_destroy(&tmp->suspend);

        pthread_mutex_lock(&THR_LOCK_threads);
        // 这里条件变量并没有什么作用, mysql还有全局初始化, 全局析构函数, 全局析构函数会等待该条件变量... 我这里不会调用该函数...
        if (--THR_thread_count == 0)
            pthread_cond_signal(&THR_COND_threads);
        pthread_mutex_unlock(&THR_LOCK_threads);

        /* Trash variable so that we can detect false accesses to my_thread_var */
        tmp->init= 2;
        free(tmp);
    }
    return true;
}

bool my_thread_global_init()
{
    if(my_thread_global_init_done)
        return true;
    my_thread_global_init_done = true;
    int rc = pthread_key_create(&THR_KEY, NULL);
    if(rc)
        return false;

    return true;
}
