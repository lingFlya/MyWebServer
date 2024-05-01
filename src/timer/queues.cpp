/*
  This code originates from the Unireg project.

  Code for generell handling of priority Queues.
  Implementation of queues from "Algorithms in C" by Robert Sedgewick.

  The queue can optionally store the position in queue in the element
  that is in the queue. This allows one to remove any element from the queue
  in O(1) time.

  Optimisation of _downheap() and queue_fix() is inspired by code done
  by Mikael Ronström, based on an optimisation of _downheap from
  Exercise 7.51 in "Data Structures & Algorithms in C++" by Mark Allen
  Weiss, Second Edition.
*/

#include "timer/queues.h"

#include <cstdlib>

int init_queue(QUEUE* queue, unsigned int max_element_count, unsigned int offset_to_key,
    bool max_at_top, queue_compare compare,
    void *first_cmp_arg, unsigned int offset_to_queue_pos,
    unsigned int auto_extent)
{
    queue->root = (unsigned char**)malloc((max_element_count + 1) * sizeof(void*));
    if(queue->root == NULL)
        return 1;
    
    queue->element_count = 0;
    queue->max_element_count = max_element_count;
    queue->offset_to_key = offset_to_key;
    queue->offset_to_queue_pos = offset_to_queue_pos;
    queue->auto_extent = auto_extent;
    queue_set_max_at_top(queue, max_at_top);
    queue->compare = compare;
    queue->first_cmp_arg = first_cmp_arg;
    return 0;
}

int reinit_queue(QUEUE *queue, unsigned int max_element_count,unsigned int offset_to_key,
    bool max_at_top, queue_compare compare,
    void *first_cmp_arg, unsigned int offset_to_queue_pos,
    unsigned int auto_extent)
{
    queue->element_count = 0;
    queue->offset_to_key = offset_to_key;
    queue->offset_to_queue_pos = offset_to_queue_pos;
    queue->auto_extent = auto_extent;
    queue_set_max_at_top(queue, max_at_top);
    queue->compare = compare;
    queue->first_cmp_arg = first_cmp_arg;
    return resize_queue(queue, max_element_count);
}

int resize_queue(QUEUE* queue, unsigned int max_element_count)
{
    if(queue->max_element_count == max_element_count)
        return 0;
    unsigned char** new_root = (unsigned char**)realloc((void*)queue->root, (max_element_count + 1) * sizeof(void*));
    if(new_root == NULL)
        return 1;
    if(queue->max_element_count < max_element_count)
        queue->max_element_count = max_element_count;
    queue->root = new_root;
    return 0;
}

void delete_queue(QUEUE* queue)
{
    free(queue->root);
    queue->root = NULL;// 允许多次调用释放, 因为free官方文档说明: 若是NULL, 则free函数什么也不做
}

/**
 * @brief 将元素插入到idx指定的位置, 然后对其做一次上浮, 直到合法位置(满足堆序性)
 * @param queue 要插入的heap
 * @param element 插入的元素
 * @param idx 从堆的idx位置开始insert
 */
static void _insert_at(QUEUE* queue, unsigned char* element, unsigned int idx)
{
    unsigned int next_index, offset_to_key = queue->offset_to_key;
    unsigned int offset_to_queue_pos = queue->offset_to_queue_pos;
    while((next_index = idx >> 1) > 0 &&
        queue->compare(queue->first_cmp_arg, 
            element + offset_to_key,
            queue->root[next_index] + offset_to_key) * queue->max_at_top < 0)
    {
        queue->root[idx] = queue->root[next_index];
        if(offset_to_queue_pos)
            (*(unsigned int*)(queue->root[idx] + offset_to_queue_pos - 1)) = idx;
        idx = next_index;   
    }
    queue->root[idx] = element;
    if(offset_to_queue_pos)
        (*(unsigned int*)(element + offset_to_queue_pos - 1)) = idx;
}

int queue_insert(QUEUE* queue, unsigned char* element)
{
    if(queue->element_count == queue->max_element_count)
    {
        return 1;
    }
    _insert_at(queue, element, ++queue->element_count);
    return 0;
}

int queue_insert_safe(QUEUE* queue, unsigned char* element)
{
    if(queue->element_count == queue->max_element_count)
    {
        if(!queue->auto_extent)
            return 2;
        if(resize_queue(queue, queue->max_element_count + queue->auto_extent))
            return 1;
    }
    queue_insert(queue, element);
    return 0;
}

unsigned char* queue_remove(QUEUE* queue, unsigned int idx)
{
    unsigned char* element = queue->root[idx];
    queue->root[idx] = queue->root[queue->element_count--];
    queue_replace(queue, idx);
    return element;
}

unsigned char* queue_remove_top(QUEUE* queue)
{
    return queue_remove(queue, queue_first_element(queue));
}

void queue_replace(QUEUE* queue, unsigned int idx)
{
    unsigned char* element = queue->root[idx];
    unsigned int next_index, 
        half_queue= queue->element_count >> 1,
        offset_to_key= queue->offset_to_key,
        offset_to_queue_pos= queue->offset_to_queue_pos;
    bool first= true;

    while(idx <= half_queue)
    {
        next_index = idx + idx;
        // 这里的下滤必须优先左边, 因为是完全二叉树...
        if(next_index < queue->element_count &&
            queue->compare(queue->first_cmp_arg,
                queue->root[next_index] + offset_to_key,
                queue->root[next_index + 1] + offset_to_key) * queue->max_at_top > 0)
        {
            ++next_index;
        }
        if(first && 
            queue->compare(queue->first_cmp_arg,
                queue->root[next_index] + offset_to_key,
                element + offset_to_key) * queue->max_at_top >= 0)
        {
            // 已经不需要下滤, 提前退出下滤; **仅仅是在循环的第一次时检查** 原因:
            // 估计是因为过多检查可能反而效率不好, 因为下滤时父节点和左右孩子都需要比较, 而上浮时只比较父子
            queue->root[idx] = element;
            if(offset_to_queue_pos)
                *((unsigned int*)(queue->root[idx] + offset_to_queue_pos - 1)) = idx;
            break;
        }

        // 直接下沉, 不比较大小; 最后上浮时再比较父子大小
        first = false;
        queue->root[idx] = queue->root[next_index];
        if(offset_to_queue_pos)
            *((unsigned int*)(queue->root[idx] + offset_to_queue_pos - 1)) = idx;
        idx = next_index;
    }

    /***
    上浮不可以省略, 必须要做; 考虑这样一种例子: 如果删除元素1, 
    6和1交换之后, 6既要下滤, 而且还需要上浮一次, 否则就不满足堆序性;
            10
           /  \
          9    3
         / \   /\
        8   5 1  2
       / \
      7   6
    *****/
    _insert_at(queue, element, idx);
}

/**
 * @brief 下滤, 从指定堆的idx位置开始下滤
 * @param queue 堆
 * @param idx 指定的位置
 */
static void _down_heap(QUEUE *queue, unsigned int idx)
{
    unsigned char* element = queue->root[idx];
    unsigned int half_queue = queue->element_count >> 1;
    unsigned int next_index, 
        offset_to_key = queue->offset_to_key;
        
    while(idx <= half_queue)
    {
        next_index = idx + idx;
        if(queue->compare(queue->first_cmp_arg,
            queue->root[next_index] + offset_to_key,
            queue->root[next_index + 1] + offset_to_key) * queue->max_at_top > 0)
        {
            ++next_index;
        }
        if(queue->compare(queue->first_cmp_arg,
            queue->root[next_index] + offset_to_key,
            element + offset_to_key) * queue->max_at_top >= 0)
        {
            break;
        }
        queue->root[idx] = queue->root[next_index];
        if(queue->offset_to_queue_pos)
            (*(unsigned int*)(queue->root[idx] + queue->offset_to_queue_pos - 1)) = idx;
        idx = next_index;
    }
    queue->root[idx] = element;
    if(queue->offset_to_queue_pos)
        (*(unsigned int*)(queue->root[idx] + queue->offset_to_queue_pos - 1)) = idx;
}

void queue_fix(QUEUE* queue)
{
    for(unsigned int i = queue->element_count >> 1; i > 0; --i)
        _down_heap(queue, i);
}

void queue_replace_top(QUEUE* queue)
{
    _down_heap(queue, queue_first_element(queue));
}
