/**
 * @author 2mu
 * @date 2022/10/20
 * @brief 简单双向链表的实现。
 */

#include <stdbool.h>

#ifndef POLLER_LIST_H
#define POLLER_LIST_H

/**
 * @brief 简单双向链表的实现（有头节点）
 */

struct list_head{
    struct list_head* prev;
    struct list_head* next;
};

/**
 * list_entry - get the struct for this entry; 访问链表节点ptr指向的数据
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * @brief 初始化双向链表
 * @param list 需要初始化的双向链表
 */
void ListInit(struct list_head* list);

/**
 * @brief 将node增加到head的后面（头插）
 */
void ListAdd(struct list_head* node, struct list_head* head);

/**
 * @brief 将node增加到head的前面（尾插）
 */
void ListAddTail(struct list_head* node, struct list_head* head);

/**
 * @brief 从head链表中删除entry
 */
void ListDel(struct list_head* entry);

/**
 * @brief 将node节点从原有链表中删除，插入到head(另一个链表)的后面
 */
void ListMove(struct list_head* node, struct list_head* head);

/**
 * @brief 将node节点从原有链表中删除，插入到head(另一个链表)的尾部
 */
void ListMoveTail(struct list_head* node, struct list_head* head);

/**
 * @brief 判断列表是否为空
 */
bool ListEmpty(const struct list_head* head);

/**
 * @brief 合并两个链表，将list头节点元素后面的所有元素都移动到head之后（头插法）
 * @param list 被合并的链表
 * @param head 另一个链表的头节点
 */
void ListSplice(struct list_head* list, struct list_head* head);

/**
 * @brief 合并两个链表，将list头节点元素后面的所有元素都移动到head之后（头插法），并重新初始化list
 * @param list 被合并的链表
 * @param head 另一个链表的头节点
 */
void ListSpliceInit(struct list_head* list, struct list_head* head);

#endif //POLLER_LIST_H
