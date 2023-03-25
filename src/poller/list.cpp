/**
 * @author 2mu
 * @date 2022/10/21
 * @brief 源文件
 */

#include "poller/list.h"


/**
 * @brief 将node节点增加到prev和next之间
 */
static inline void __ListAdd(struct list_head* node, struct list_head* prev, struct list_head* next)
{
    node->prev = prev;
    node->next = next;
    prev->next = node;
    next->prev = node;
}

/**
 * @brief 已知一个节点的prev和next的情况下，删除该节点。
 */
static inline void __ListDel(struct list_head* prev, struct list_head* next)
{
    prev->next = next;
    next->prev = prev;
}

static inline void __ListSplice(struct list_head* list, struct list_head* head)
{
    struct list_head* first = list->next;
    struct list_head* last = list->prev;
    struct list_head* at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}


inline void ListInit(struct list_head* list)
{
    list->prev = list;
    list->next = list;
}

inline void ListAdd(struct list_head* node, struct list_head* head)
{
    __ListAdd(node, head, head->next);
}

inline void ListAddTail(struct list_head* node, struct list_head* head)
{
    __ListAdd(node, head->prev, head);
}

inline void ListDel(struct list_head* entry)
{
    __ListDel(entry->prev, entry->next);
}

inline void ListMove(struct list_head* node, struct list_head* head)
{
    __ListDel(node->prev, node->prev);
    ListAdd(node, head);
}

inline void ListMoveTail(struct list_head* node, struct list_head* head)
{
    __ListDel(node->prev, node->prev);
    ListAddTail(node, head);
}

inline bool ListEmpty(const struct list_head* head)
{
    return head->next == head;
}

inline void ListSplice(struct list_head* list, struct list_head* head)
{
    // 如果list只有头节点，直接退出
    if(!ListEmpty(list))
        __ListSplice(list ,head);
}

inline void ListSpliceInit(struct list_head* list, struct list_head* head)
{
    // 如果list只有头节点，直接退出
    if(!ListEmpty(list))
    {
        __ListSplice(list ,head);
        ListInit(list);
    }
}
