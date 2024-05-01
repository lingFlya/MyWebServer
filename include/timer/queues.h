/*
  Code for general handling of priority Queues.
  Implementation of queues from "Algorithms in C" by Robert Sedgewick.

  注释: 这个heap的实现非常有意思, 它给每个元素都加上了index, 也就是说这个heap的元素支持随机访问;
*/

#ifndef _QUEUES_H
#define _QUEUES_H


typedef int (*queue_compare)(void *, unsigned char *, unsigned char *);

typedef struct st_queue
{
    unsigned char **root;               // 该数组的idx是从1开始计算的, 便于后面计算
    unsigned int element_count;
    unsigned int max_element_count;
    unsigned int offset_to_key;         // 用于元素之间比较大小, 加入到root的元素根据堆序性, 需要比较大小
    unsigned int offset_to_queue_pos;   // 如果init时设置为非0值, 表示需要将 {元素在root数组的idx} 存储到元素内部(即元素地址偏移offset_to_queue_pos的位置..)
    unsigned int auto_extent;           // 当queue大小不够时, resize时每次增加多少元素内存; 0表示不允许动态增加内存
    int max_at_top;                     // -1表示大根堆, 1表示小根堆
    queue_compare compare;
    void *first_cmp_arg;
} QUEUE;

#define queue_set_max_at_top(queue, set_arg) \
    (queue)->max_at_top= set_arg ? -1 : 1
// 堆的索引是从1开始的, 没有使用0
#define queue_first_element(queue) 1


/**
 * @brief 初始化堆
 * @return 0表示成功, 1表示失败
 */
int init_queue(QUEUE *queue, unsigned int max_element_count, unsigned int offset_to_key,
    bool max_at_top, queue_compare compare,
    void *first_cmp_arg, unsigned int offset_to_queue_pos, 
    unsigned int auto_extent);

/**
 * @brief 重新初始化堆, 会删除堆中所有元素
 * @return 0表示成功, 1表示失败
 */
int reinit_queue(QUEUE *queue, unsigned int max_element_count, unsigned int offset_to_key,
    bool max_at_top, queue_compare compare,
    void *first_cmp_arg, unsigned int offset_to_queue_pos,
    unsigned int auto_extent);

/**
 * @brief 调整堆能够存放的最大元素数量, 如果 max_element_count 比堆中当前元素数量小
 * 则多余元素会被删除
 * @param queue 堆
 * @param max_element_count 最大元素数量
 * @return 0表示成功, 1表示失败
 */
int resize_queue(QUEUE *queue, unsigned int max_element_count);

/**
 * @brief 删除堆, 释放内存
 * @param queue 被释放的堆
 */
void delete_queue(QUEUE *queue);

/**
 * @brief 插入一个新元素到堆中
 * @param queue 被插入的堆
 * @param element 新元素
 * @return 0表示成功, 1表示失败
 */
int queue_insert(QUEUE *queue, unsigned char *element);

/**
 * @brief 和queue_insert类似, 插入一个新元素到堆中, 如果没有空间存放新元素, 则扩大内存
 * @return 0表示成功, 1表示无法分配更多内存, 2表示 auto_extend=0, 不允许自动扩大内存;
 */
int queue_insert_safe(QUEUE* queue, unsigned char* element);

/**
 * @brief 移除堆中的一个元素
 * @param queue 堆
 * @param idx 移除堆中处于idx位置的元素
 * @return 成功则返回被移除元素的地址
 */
unsigned char* queue_remove(QUEUE* queue, unsigned int idx);

/**
 * @brief 移除堆顶的元素
 * @param queue 堆
 * @return 成功则返回被移除元素的地址
 */
unsigned char* queue_remove_top(QUEUE* queue);

/**
 * @brief 子函数, 检查处于idx位置的元素位置是否正确;
 * 必须先做一次下滤, 然后做一次上浮; 注释说这里有优化: 
 * 下滤时, 只比较左右孩子, 而左右孩子并不会和idx位置的元素进行比较
 * 最后统一在上浮时进行比较;
 * @param queue 堆
 * @param idx 在堆中的idx
 */
void queue_replace(QUEUE*queue, unsigned int idx);


/**
 * @brief 对堆顶元素做一次检查, 只需要做一次下滤; 因为头顶没有元素, 不需要上浮
 * @param queue 堆
 */
void queue_replace_top(QUEUE* queue);

/**
 * @brief 原始建堆方法; 从下往上对每个非叶子节点做一次下滤;
 * @param queue 堆
 */
void queue_fix(QUEUE *queue);


#define queue_empty(queue) ((queue)->element_count == 0)
#define queue_top(queue) ((queue)->root[1])
#define queue_element(queue,index) ((queue)->root[index])
#define queue_end(queue) ((queue)->root[(queue)->element_count])
#define queue_is_inited(queue) ((queue)->root != NULL)
#define queue_remove_all(queue) ((queue)->element_count= 0)
#define queue_is_full(queue) (queue->element_count == queue->max_element_count)


#endif // _QUEUES_H
