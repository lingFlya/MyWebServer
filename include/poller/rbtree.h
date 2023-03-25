/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  Some example of insert and search follows here. The search is a plain
  normal search over an ordered tree. The insert instead must be implemented
  int two steps: as first thing the code must insert the element in
  order as a red leaf in the tree, then the support library function
  rb_insert_color() must be called. Such function will do the
  not trivial work to rebalance the rbtree if necessary.

-----------------------------------------------------------------------
static inline struct page * rb_search_page_cache(struct inode * inode,
						 unsigned long offset)
{
	rb_node_t * n = inode->i_rb_page_cache.rb_node;
	struct page * page;

	while (n)
	{
		page = rb_entry(n, struct page, rb_page_cache);

		if (offset < page->offset)
			n = n->rb_left;
		else if (offset > page->offset)
			n = n->rb_right;
		else
			return page;
	}
	return NULL;
}

static inline struct page * __rb_insert_page_cache(struct inode * inode,
						   unsigned long offset,
						   rb_node_t * node)
{
	rb_node_t ** p = &inode->i_rb_page_cache.rb_node;
	rb_node_t * parent = NULL;
	struct page * page;

	while (*p)
	{
		parent = *p;
		page = rb_entry(parent, struct page, rb_page_cache);

		if (offset < page->offset)
			p = &(*p)->rb_left;
		else if (offset > page->offset)
			p = &(*p)->rb_right;
		else
			return page;
	}

	rb_link_node(node, parent, p);

	return NULL;
}

static inline struct page * rb_insert_page_cache(struct inode * inode,
						 unsigned long offset,
						 rb_node_t * node)
{
	struct page * ret;
	if ((ret = __rb_insert_page_cache(inode, offset, node)))
		goto out;
	rb_insert_color(node, &inode->i_rb_page_cache);
 out:
	return ret;
}
-----------------------------------------------------------------------
*/

/**
 * @author 2mu
 * @date 2022/7/26
 * @brief 这应该是某个Linux内核版本的红黑树代码。让我来参考学习一下吧。
 * 红黑树的性质：
 * 性质1：每个节点要么是黑色，要么是红色。
 * 性质2：root是黑的（算法导论这么说的，但是有些地方并没有这条，按照算法导论来）
 * 性质3：每个叶子节点是黑色。（NULL节点被当成叶子节点（NIL），叫做哨兵）
 * 性质4：若结点为红色，其两个孩子结点一定都是黑色。
 * 性质5：任意一结点到每个叶子结点的路径都包含数量相同的黑结点。
 *
 * 注意：红黑树并不是任意节点左右孩子高度差 <= 1的平衡树。它有自己的平衡规则(性质4，黑色完美平衡)。
 *
 * 引理：一颗有n个节点红黑树，高度最多是2log(n+1)
 */

#ifndef	_LINUX_RB_TREE_H
#define	_LINUX_RB_TREE_H

#pragma pack(1)
struct rb_node
{
    struct rb_node *rb_parent;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
    char rb_color;
#define	RB_RED		0
#define	RB_BLACK	1
};
#pragma pack()

/**
 * @brief root，在平衡树中，由于左旋，右旋的原因，根节点是非常有可能会变化的。
 * 也就是rb_node成员会经常变化。这里包装一层实际上方便。
 */
struct rb_root
{
    struct rb_node* rb_node;
};

#define RB_ROOT (struct rb_root){ (struct rb_node *)0, }
#define	rb_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 对于新插入的节点，默认是红色，需要检测其是否违反红黑树性质，重新染色左旋右旋，使整棵树满足红黑树性质
 * @param node 新插入的节点
 * @param root 树的root节点
 */
extern void rb_insert_color(struct rb_node *node, struct rb_root *root);

/**
 * @brief 删除一个节点
 * @param node 要删除的节点
 * @param root 根节点
 */
extern void rb_erase(struct rb_node *node, struct rb_root *root);

/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(struct rb_node *);
extern struct rb_node *rb_prev(struct rb_node *);
extern struct rb_node *rb_first(struct rb_root *);
extern struct rb_node *rb_last(struct rb_root *);

/**
 * @brief 快速以指定节点替换掉树的原有节点, 而不需要重新平衡
 * @param victim 原有节点
 * @param newNode 新节点
 * @param root 树
 */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *newNode,
                            struct rb_root *root);

#ifdef __cplusplus
}
#endif

/**
 * @brief 直接将新节点node，设置为parent的子节点
 * @param node 新节点
 * @param parent 新节点的父亲
 * @param link 新节点node和parent的关系，左孩子还是右孩子？
 */
static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
    struct rb_node **link)
{
    node->rb_parent = parent;
    node->rb_color = RB_RED;
    node->rb_left = node->rb_right = (struct rb_node *)0;

    *link = node;
}

#endif  // _LINUX_RB_TREE_H
