#include "poller/rbtree.h"

static void __rb_rotate_left(struct rb_node* node, struct rb_root* root)
{
    struct rb_node* right = node->rb_right;
    if((node->rb_right = right->rb_left))
        node->rb_right->rb_parent = node;
    right->rb_left = node;

    if((right->rb_parent = node->rb_parent))
    {
        if(node == node->rb_parent->rb_left)
            node->rb_parent->rb_left = right;
        else
            node->rb_parent->rb_right = right;
    }
    else
        root->rb_node = right;// 没有父亲的节点，那就只能是root
    node->rb_parent = right;
}

static void __rb_rotate_right(struct rb_node* node, struct rb_root* root)
{
    struct rb_node* left = node->rb_left;
    if((node->rb_left = left->rb_right))
        node->rb_left->rb_parent = node;
    left->rb_right = node;

    if((left->rb_parent = node->rb_parent))
    {
        if(node == node->rb_parent->rb_left)
            node->rb_parent->rb_left = left;
        else
            node->rb_parent->rb_right = left;
    }
    else
        root->rb_node = left;
    node->rb_parent = left;
}

void rb_insert_color(struct rb_node* node, struct rb_root* root)
{
    /**
     * 经过分析可以发现，插入一个新节点会违反的性质，只有两个：1和3
     * 违反1的话：只能刚插入第一个节点，因为默认初始化为红色。
     * 大部分是违反3，新插入的节点node和节点node的父亲都是红色
     */
    struct rb_node* parent = node->rb_parent;
    while(parent && parent->rb_color == RB_RED)
    {
        struct rb_node* gparent = parent->rb_parent;
        /**
         * 初始node是红的，进入while循环则说明parent也是红的，则违反性质3，再分为下面3种情况：
         * 1. node的父亲的兄弟是红的
         * 2. node的父亲的兄弟是黑的，且node是右孩子
         * 3. node的父亲的兄弟是黑的，且node是左孩子
         */
        if(parent == gparent->rb_left)
        {
            {// 情况1
                register struct rb_node *uncle = gparent->rb_right;
                if (uncle && uncle->rb_color == RB_RED)
                {
                    uncle->rb_color = RB_BLACK;
                    parent->rb_color = RB_BLACK;
                    gparent->rb_color = RB_RED;
                    node = gparent;
                    continue; // 节点往上，继续下一个循环。
                }
            }
            if(parent->rb_right == node)
            {
                // 情况2: 旋转，转化为情况3
                register struct rb_node* tmp;
                __rb_rotate_left(parent, root);
                //  旋转之后，parent就是node的孩子，node变成父亲。所以这里变量交换一下。
                tmp = node;
                parent = node;
                node = tmp;
            }
            // 情况3
            parent->rb_color = RB_BLACK;
            gparent->rb_color = RB_RED;
            __rb_rotate_right(gparent, root);
        }
        else
        {
            // 和上面的3种情况刚好对称。
            {
                {
                    register struct rb_node *uncle = gparent->rb_left;
                    if (uncle && uncle->rb_color == RB_RED)
                    {
                        uncle->rb_color = RB_BLACK;
                        parent->rb_color = RB_BLACK;
                        gparent->rb_color = RB_RED;
                        node = parent;
                        continue;
                    }
                }
                if(parent->rb_left == node)
                {
                    register struct rb_node* tmp;
                    __rb_rotate_right(parent, root);
                    tmp = node;
                    node = parent;
                    parent = tmp;
                }
                parent->rb_color = RB_BLACK;
                gparent->rb_color = RB_RED;
                __rb_rotate_left(gparent, root);
            }
        }
    }
    root->rb_node->rb_color = RB_BLACK;
}

/**
 * @brief 若删除的是黑色节点,需要重新平衡
 * @param node rb_erase删除操作执行完成后, 树结构调整后, 实际被删除位置的顶替者),
 * @param parent 结构调整后, node的父亲
 * @param root 根节点
 */
static void __rb_erase_color(struct rb_node *node, struct rb_node *parent,
                             struct rb_root *root)
{
    struct rb_node* other; // 实际上是node叔叔
    while((!node || node->rb_color == RB_BLACK) && node != root->rb_node)
    {
        if(parent->rb_left == node)
        {
            other = parent->rb_right;
            if(other->rb_color == RB_RED)
            {
                /// 对应算法导论情况1: 把父亲变黑, 叔叔变红, 然后左旋
                other->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;
                __rb_rotate_left(parent, root);
                other = parent->rb_right;// node的新叔叔
            }
            if((!other->rb_left || other->rb_left->rb_color == RB_BLACK)
                && (!other->rb_right || other->rb_right->rb_color == RB_BLACK))
            {
                /// 对应算法导论情况2:
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if(!other->rb_right || other->rb_right->rb_color == RB_BLACK)
                {
                    /// 对应算法导论情况3:
                    register struct rb_node* o_left;
                    if((o_left = other->rb_left))
                        o_left->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    __rb_rotate_right(other, root);
                    other = parent->rb_right;
                    /// 此时到了情况4,
                }
                /// 对应算法导论情况4:
                other->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (other->rb_right)
                    other->rb_right->rb_color = RB_BLACK;
                __rb_rotate_left(parent, root);
                node = root->rb_node;
                break;
            }
        }
        else
        {
            other = parent->rb_left;
            if (other->rb_color == RB_RED)
            {
                other->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;
                __rb_rotate_right(parent, root);
                other = parent->rb_left;
            }
            if ((!other->rb_left ||
                 other->rb_left->rb_color == RB_BLACK)
                && (!other->rb_right ||
                    other->rb_right->rb_color == RB_BLACK))
            {
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if (!other->rb_left ||
                    other->rb_left->rb_color == RB_BLACK)
                {
                    register struct rb_node *o_right;
                    if ((o_right = other->rb_right))
                        o_right->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    __rb_rotate_left(other, root);
                    other = parent->rb_left;
                }
                other->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (other->rb_left)
                    other->rb_left->rb_color = RB_BLACK;
                __rb_rotate_right(parent, root);
                node = root->rb_node;
                break;
            }
        }
    }
    if (node)
        node->rb_color = RB_BLACK;
}

void rb_erase(struct rb_node* node, struct rb_root* root)
{
    struct rb_node* child, *parent;
    int color;

    /// 前面两个分支包含3种最容易处理的情况: 1. node是叶子节点 2. node只有左孩子 3. node只有右孩子
    /// 这3种情况的处理方式相同, 只要把node删除, 并让其node的孩子接替node
    if(!node->rb_left)
        child = node->rb_right;
    else if(!node->rb_right)
        child = node->rb_left;
    else
    {
        /// 最复杂的情况，节点的左孩子和右孩子都存在。不好操作，会先和前驱或者和后继交换再删除。
        struct rb_node* old = node, *left;
        node = node->rb_right;
        while((left = node->rb_left))
            node = left;
        child = node->rb_right;// 这个后继只可能有右孩子。
        parent = node->rb_parent;
        color = node->rb_color;

        /// 删除后继节点的父子关系，给后继节点的child和后继节点的parent建立父子关系
        if(child)
            child->rb_parent = parent;
        if(parent)
        {
            if(parent->rb_left == node)
                parent->rb_left = child;
            else
                parent->rb_right = child;
        }
        else
            root->rb_node = child;

        /// start: 这一段代码是用找到的**后继**替换掉old，建立父子关系，后续才能删除old。（为什么不是交换节点之间的数据以避免建立关系，因为这种侵入式的实现不能这么做）
        if(node->rb_parent == old)
            parent = node;/// 这里设置parent,是为了调用__rb_erase_color函数。且只有一种可能：后继就是node的右孩子（其没有左孩子了）。
        node->rb_parent = old->rb_parent;
        node->rb_color = old->rb_color;
        node->rb_left = old->rb_left;
        node->rb_right = old->rb_right;

        if(old->rb_parent)
        {
            if(old->rb_parent->rb_left == old)
                old->rb_parent->rb_left = node;
            else
                old->rb_parent->rb_right = node;
        }
        else
            root->rb_node = node;

        old->rb_left->rb_parent = node;
        if(old->rb_right)
            old->rb_right->rb_parent = node;
        /// end
        goto color;
    }

    parent = node->rb_parent;
    color = node->rb_color;
    if(child)
        child->rb_parent = parent;
    if(parent)
    {
        if(parent->rb_left == node)
            parent->rb_left = child;
        else
            parent->rb_right = child;
    }
    else
        root->rb_node = child;

    color:
    /**
     * @brief 红色节点删除并不会违反5条性质，所以无需平衡。原因如下:
     * 1. 树中的黑高度并没有变化.
     * 2. 没有相邻的红色节点
     * 3. 红色节点不可能是root节点
     */

    /// 若是黑色节点删除，则需要重新平衡。
    if(color == RB_BLACK)
        __rb_erase_color(child, parent, root);
}

struct rb_node* rb_next(struct rb_node* node)
{
    // 如果右孩子存在，那就是右孩子最左侧的节点。
    if(node->rb_right)
    {
        node = node->rb_right;
        while(node->rb_left)
            node = node->rb_left;
        return node;
    }
    // 如果右孩子不存在，那就是node的祖先。
    while(node->rb_parent && node == node->rb_parent->rb_right)
        node = node->rb_parent;
    return node;
    // 若是没有后继，为什么不返回NULL？ 没有后续的节点只能是rb_last。没必要返回NULL
}

struct rb_node* rb_prev(struct rb_node* node)
{
    if(node->rb_left)
    {
        node = node->rb_left;
        while(node->rb_right)
            node = node->rb_right;
        return node;
    }
    while(node->rb_parent && node == node->rb_parent->rb_left)
        node = node->rb_parent;
    return node;
}

struct rb_node* rb_first(struct rb_root* root)
{
    struct rb_node* node = root->rb_node;
    if(!node)
        return (struct rb_node*)0;
    while(node->rb_left)
        node = node->rb_left;
    return node;
}

struct rb_node* rb_last(struct rb_root* root)
{
    struct rb_node* node = root->rb_node;
    if(!node)
        return (struct rb_node*)0;
    while(node->rb_right)
        node = node->rb_right;
    return node;
}

void rb_replace_node(struct rb_node *victim, struct rb_node *newNode,
        struct rb_root *root)
{
    struct rb_node* parent = victim->rb_parent;
    if(parent)
    {
        if(victim == parent->rb_left)
            parent->rb_left = newNode;
        else
            parent->rb_right = newNode;
    }
    else
        root->rb_node = newNode;        // 没有父节点，那么victim只能是root

    if(victim->rb_left)
        victim->rb_left->rb_parent = newNode;
    if(victim->rb_right)
        victim->rb_right->rb_parent = newNode;

    // newNode 直接拷贝 victim的所有指针
    *newNode = *victim;
}