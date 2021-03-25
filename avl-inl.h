#ifndef __AVL_INL_H__
#define __AVL_INL_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
# define AVL_EXPORT(declaration) __attribute__((unused)) static declaration
#else
# define AVL_EXPORT(declaration) static declaration
#endif

#ifndef _WIN64

struct avl_node {
   struct avl_node *avl_child[2];    /* left/right children */
   struct avl_node *avl_parent;      /* this node's parent */
   unsigned short avl_child_index;   /* my index in parent's avl_child[] */
   short avl_balance;                /* balance value: -1, 0, +1 */
};

#define AVL_XPARENT(n)       ((n)->avl_parent)
#define AVL_SETPARENT(n, p)  ((n)->avl_parent = (p))

#define AVL_XCHILD(n)        ((n)->avl_child_index)
#define AVL_SETCHILD(n, c)   ((n)->avl_child_index = (unsigned short)(c))

#define AVL_XBALANCE(n)      ((n)->avl_balance)
#define AVL_SETBALANCE(n, b) ((n)->avl_balance = (short)(b))

#else /* _WIN64 */

/*
 * for 64 bit machines, avl_pcb contains parent pointer, balance and child_index
 * values packed in the following manner:
 *
 * |63                                  3|        2        |1          0 |
 * |-------------------------------------|-----------------|-------------|
 * |      avl_parent hi order bits       | avl_child_index | avl_balance |
 * |                                     |                 |     + 1     |
 * |-------------------------------------|-----------------|-------------|
 *
 */
struct avl_node {
   struct avl_node *avl_child[2];    /* left/right children nodes */
   uintptr_t avl_pcb;        /* parent, child_index, balance */
};

   /*
    * macros to extract/set fields in avl_pcb
    *
    * pointer to the parent of the current node is the high order bits
    */
#define AVL_XPARENT(n)      ((struct avl_node *)((n)->avl_pcb & ~7))
#define AVL_SETPARENT(n, p) ((n)->avl_pcb = (((n)->avl_pcb & 7) | (uintptr_t)(p)))

/*
 * index of this node in its parent's avl_child[]: bit #2
 */
#define AVL_XCHILD(n)       (((n)->avl_pcb >> 2) & 1)
#define AVL_SETCHILD(n, c)  ((n)->avl_pcb = (uintptr_t)(((n)->avl_pcb & ~4) | ((c) << 2)))

/*
 * balance indication for a node, lowest 2 bits. A valid balance is
 * -1, 0, or +1, and is encoded by adding 1 to the value to get the
 * unsigned values of 0, 1, 2.
 */
#define AVL_XBALANCE(n)      ((int)(((n)->avl_pcb & 3) - 1))
#define AVL_SETBALANCE(n, b) ((n)->avl_pcb = (uintptr_t)((((n)->avl_pcb & ~3) | ((b) + 1))))

#endif /* _LP64 */

/*
 * macros used to create/access an avl_index_t
 */
#define AVL_INDEX2NODE(x)   ((struct avl_node *)((x) & ~1))
#define AVL_INDEX2CHILD(x)  ((x) & 1)
#define AVL_MKINDEX(n, c)   ((avl_index_t)(n) | (c))


/*
 * The tree structure. The fields avl_root, avl_compar, and avl_offset come
 * first since they are needed for avl_find().  We want them to fit into
 * a single 64 byte cache line to make avl_find() as fast as possible.
 */
struct avl_tree {
   struct avl_node *avl_root;    /* root node in tree */
   unsigned long avl_numnodes;     /* number of nodes in the tree */
};

typedef int (*avl_compare_fn)(const struct avl_node *a, const struct avl_node *b);

typedef uintptr_t avl_index_t;

#define AVL_BEFORE  (0)
#define AVL_AFTER   (1)
/*
 * Public functions.
 */
AVL_EXPORT(void avl_init(struct avl_tree *tree));
AVL_EXPORT(void avl_insert(struct avl_tree *tree, struct avl_node *node, avl_index_t where));
AVL_EXPORT(void avl_insert_here(struct avl_tree *tree, struct avl_node *node, struct avl_node *here, int direction));
AVL_EXPORT(struct avl_node *avl_find(struct avl_tree *tree, struct avl_node *node, avl_index_t *where, avl_compare_fn compare));
AVL_EXPORT(void avl_remove(struct avl_tree *tree, struct avl_node *node));
AVL_EXPORT(struct avl_node *avl_first(struct avl_tree *tree));
AVL_EXPORT(struct avl_node *avl_last(struct avl_tree *tree));
AVL_EXPORT(unsigned long avl_numnodes(struct avl_tree *tree));
AVL_EXPORT(struct avl_node *avl_walk(struct avl_tree *tree, struct avl_node *node, int left));

#define avl_foreach(tree, var)                                         \
   for ((var) = avl_first(tree);                                       \
        (var) != NULL;                                                 \
        (var) = avl_walk(tree, var, AVL_AFTER))

#define avl_foreach_from(tree, var, from)                              \
   for ((var) = (from);                                                \
        ((var) != NULL) && ((from) = avl_walk(tree, var, AVL_AFTER), (var) != NULL); \
        (var) = avl_walk(tree, var, AVL_AFTER))

#define avl_foreach_safe(tree, var, tmp)                               \
   for ((var) = avl_first(tree);                                       \
        ((var) != NULL) && ((tmp) = avl_walk(tree, var, AVL_AFTER), (var) != NULL); \
        (var) = (tmp))

#define avl_foreach_reverse(tree, var)                                 \
   for ((var) = avl_last(tree);                                        \
        (var) != NULL;                                                 \
        (var) = avl_walk(tree, var, AVL_BEFORE))

#define avl_foreach_reverse_from(tree, var, from)                      \
   for ((var) = (from); \
        ((var) != NULL) && ((from) = avl_walk(tree, var, AVL_BEFORE), (var) != NULL); \
        (var) = (from))

#define avl_foreach_reverse_safe(tree, var, tmp)                       \
   for ((var) = avl_last(tree);                                        \
        ((var) != NULL) && ((tmp) = avl_walk(tree, var, AVL_BEFORE), (var) != NULL); \
        (var) = (tmp))
/*
 * Implementation follows.
 */

AVL_EXPORT(void avl_init(struct avl_tree *tree))
{
   if (tree)
   {
      tree->avl_numnodes = 0;
      tree->avl_root = NULL;
   }
}

static const int  avl_child2balance[2]  = {-1, 1};
static const int  avl_balance2child[]   = {0, 0, 1};

/*
 * Search for the node which contains "value".  The algorithm is a
 * simple binary tree search.
 *
 * return value:
 *  NULL: the value is not in the AVL tree
 *      *where (if not NULL)  is set to indicate the insertion point
 *  "struct avl_node *"  of the found tree node
 */
AVL_EXPORT(struct avl_node *avl_find(struct avl_tree *tree, struct avl_node *find, avl_index_t *where, avl_compare_fn compare))
{
   struct avl_node *node;
   struct avl_node *prev = NULL;
   int child = 0;
   int diff;

   for (node = tree->avl_root; node != NULL; node = node->avl_child[child])
   {
      prev = node;

      diff = compare(find, node);
      if (diff == 0) return node;
      if (diff < 0) diff = -1;
      if (diff > 0) diff = 1;
      child = avl_balance2child[1 + diff];
   }

   if (where != NULL)
      *where = AVL_MKINDEX(prev, child);

   return (NULL);
}


/*
 * Perform a rotation to restore balance at the subtree given by depth.
 *
 * This routine is used by both insertion and deletion. The return value
 * indicates:
 *   0 : subtree did not change height
 *  !0 : subtree was reduced in height
 *
 * The code is written as if handling left rotations, right rotations are
 * symmetric and handled by swapping values of variables right/left[_heavy]
 *
 * On input balance is the "new" balance at "node". This value is either
 * -2 or +2.
 */
AVL_EXPORT(int avl__rotation(struct avl_tree *tree, struct avl_node *node, int balance))
{
   int left = !(balance < 0);   /* when balance = -2, left will be 0 */
   int right = 1 - left;
   int left_heavy = balance >> 1;
   int right_heavy = -left_heavy;
   struct avl_node *parent = AVL_XPARENT(node);
   struct avl_node *child = node->avl_child[left];
   struct avl_node *cright;
   struct avl_node *gchild;
   struct avl_node *gright;
   struct avl_node *gleft;
   int which_child = AVL_XCHILD(node);
   int child_bal = AVL_XBALANCE(child);

   /* BEGIN CSTYLED */
   /*
    * case 1 : node is overly left heavy, the left child is balanced or
    * also left heavy. This requires the following rotation.
    *
    *                   (node bal:-2)
    *                    /           \
    *                   /             \
    *              (child bal:0 or -1)
    *              /    \
    *             /      \
    *                     cright
    *
    * becomes:
    *
    *              (child bal:1 or 0)
    *              /        \
    *             /          \
    *                        (node bal:-1 or 0)
    *                         /     \
    *                        /       \
    *                     cright
    *
    * we detect this situation by noting that child's balance is not
    * right_heavy.
    */
   /* END CSTYLED */
   if (child_bal != right_heavy)
   {
      /*
       * compute new balance of nodes
       *
       * If child used to be left heavy (now balanced) we reduced
       * the height of this sub-tree -- used in "return...;" below
       */
      child_bal += right_heavy; /* adjust towards right */

      /*
       * move "cright" to be node's left child
       */
      cright = child->avl_child[right];
      node->avl_child[left] = cright;
      if (cright != NULL)
      {
         AVL_SETPARENT(cright, node);
         AVL_SETCHILD(cright, left);
      }

      /*
       * move node to be child's right child
       */
      child->avl_child[right] = node;
      AVL_SETBALANCE(node, -child_bal);
      AVL_SETCHILD(node, right);
      AVL_SETPARENT(node, child);

      /*
       * update the pointer into this subtree
       */
      AVL_SETBALANCE(child, child_bal);
      AVL_SETCHILD(child, which_child);
      AVL_SETPARENT(child, parent);
      if (parent != NULL)
         parent->avl_child[which_child] = child;
      else
         tree->avl_root = child;

      return (child_bal == 0);
   }

   /* BEGIN CSTYLED */
   /*
    * case 2 : When node is left heavy, but child is right heavy we use
    * a different rotation.
    *
    *                   (node b:-2)
    *                    /   \
    *                   /     \
    *                  /       \
    *             (child b:+1)
    *              /     \
    *             /       \
    *                   (gchild b: != 0)
    *                     /  \
    *                    /    \
    *                 gleft   gright
    *
    * becomes:
    *
    *              (gchild b:0)
    *              /       \
    *             /         \
    *            /           \
    *        (child b:?)   (node b:?)
    *         /  \          /   \
    *        /    \        /     \
    *            gleft   gright
    *
    * computing the new balances is more complicated. As an example:
    *    if gchild was right_heavy, then child is now left heavy
    *       else it is balanced
    */
   /* END CSTYLED */
   gchild = child->avl_child[right];
   gleft = gchild->avl_child[left];
   gright = gchild->avl_child[right];

   /*
    * move gright to left child of node and
    *
    * move gleft to right child of node
    */
   node->avl_child[left] = gright;
   if (gright != NULL)
   {
      AVL_SETPARENT(gright, node);
      AVL_SETCHILD(gright, left);
   }

   child->avl_child[right] = gleft;
   if (gleft != NULL)
   {
      AVL_SETPARENT(gleft, child);
      AVL_SETCHILD(gleft, right);
   }

   /*
    * move child to left child of gchild and
    *
    * move node to right child of gchild and
    *
    * fixup parent of all this to point to gchild
    */
   balance = AVL_XBALANCE(gchild);
   gchild->avl_child[left] = child;
   AVL_SETBALANCE(child, (balance == right_heavy ? left_heavy : 0));
   AVL_SETPARENT(child, gchild);
   AVL_SETCHILD(child, left);

   gchild->avl_child[right] = node;
   AVL_SETBALANCE(node, (balance == left_heavy ? right_heavy : 0));
   AVL_SETPARENT(node, gchild);
   AVL_SETCHILD(node, right);

   AVL_SETBALANCE(gchild, 0);
   AVL_SETPARENT(gchild, parent);
   AVL_SETCHILD(gchild, which_child);
   if (parent != NULL)
      parent->avl_child[which_child] = gchild;
   else
      tree->avl_root = gchild;

   return (1);  /* the new tree is always shorter */
}


/*
 * Insert a new node into an AVL tree at the specified (from avl_find()) place.
 *
 * Newly inserted nodes are always leaf nodes in the tree, since avl_find()
 * searches out to the leaf positions.  The avl_index_t indicates the node
 * which will be the parent of the new node.
 *
 * After the node is inserted, a single rotation further up the tree may
 * be necessary to maintain an acceptable AVL balance.
 */
AVL_EXPORT(void avl_insert(struct avl_tree *tree, struct avl_node *new_node, avl_index_t where))
{
   struct avl_node *parent = AVL_INDEX2NODE(where);
   int old_balance;
   int new_balance;
   int which_child = AVL_INDEX2CHILD(where);

   if (!tree && !new_node)
      return;

   /*
    * First, add the node to the tree at the indicated position.
    */
   ++tree->avl_numnodes;

   new_node->avl_child[0] = NULL;
   new_node->avl_child[1] = NULL;

   AVL_SETCHILD(new_node, which_child);
   AVL_SETBALANCE(new_node, 0);
   AVL_SETPARENT(new_node, parent);
   if (parent != NULL)
      parent->avl_child[which_child] = new_node;
   else
      tree->avl_root = new_node;
   /*
    * Now, back up the tree modifying the balance of all nodes above the
    * insertion point. If we get to a highly unbalanced ancestor, we
    * need to do a rotation.  If we back out of the tree we are done.
    * If we brought any subtree into perfect balance (0), we are also done.
    */
   for (;;)
   {
      new_node = parent;
      if (new_node == NULL)
         return;

      /*
       * Compute the new balance
       */
      old_balance = AVL_XBALANCE(new_node);
      new_balance = old_balance + avl_child2balance[which_child];

      /*
       * If we introduced equal balance, then we are done immediately
       */
      if (new_balance == 0)
      {
         AVL_SETBALANCE(new_node, 0);
         return;
      }

      /*
       * If both old and new are not zero we went
       * from -1 to -2 balance, do a rotation.
       */
      if (old_balance != 0)
         break;

      AVL_SETBALANCE(new_node, new_balance);
      parent = AVL_XPARENT(new_node);
      which_child = AVL_XCHILD(new_node);
   }

   /*
    * perform a rotation to fix the tree and return
    */
   (void) avl__rotation(tree, new_node, new_balance);
}

/*
 * Insert "new_data" in "tree" in the given "direction" either after or
 * before (AVL_AFTER, AVL_BEFORE) the data "here".
 *
 * Insertions can only be done at empty leaf points in the tree, therefore
 * if the given child of the node is already present we move to either
 * the AVL_PREV or AVL_NEXT and reverse the insertion direction. Since
 * every other node in the tree is a leaf, this always works.
 *
 * To help developers using this interface, we assert that the new node
 * is correctly ordered at every step of the way in DEBUG kernels.
 */
AVL_EXPORT(void avl_insert_here(struct avl_tree *tree, struct avl_node *node, struct avl_node *here, int direction))
{
   int child = direction;        /* rely on AVL_BEFORE == 0, AVL_AFTER == 1 */

   /*
   * If corresponding child of node is not NULL, go to the neighboring
   * node and reverse the insertion direction.
   */
   if (here->avl_child[child] != NULL)
   {
      here = here->avl_child[child];
      child = 1 - child;
      while (here->avl_child[child] != NULL) 
      {
         here = here->avl_child[child];
      }
   }
   avl_insert(tree, node, AVL_MKINDEX(here, child));
}

/*
 * Delete a node from the AVL tree.  Deletion is similar to insertion, but
 * with 2 complications.
 *
 * First, we may be deleting an interior node. Consider the following subtree:
 *
 *     d           c            c
 *    / \         / \          / \
 *   b   e       b   e        b   e
 *  / \         / \          /
 * a   c       a            a
 *
 * When we are deleting node (d), we find and bring up an adjacent valued leaf
 * node, say (c), to take the interior node's place. In the code this is
 * handled by temporarily swapping (d) and (c) in the tree and then using
 * common code to delete (d) from the leaf position.
 *
 * Secondly, an interior deletion from a deep tree may require more than one
 * rotation to fix the balance. This is handled by moving up the tree through
 * parents and applying rotations as needed. The return value from
 * avl_rotation() is used to detect when a subtree did not change overall
 * height due to a rotation.
 */
AVL_EXPORT(void avl_remove(struct avl_tree *tree, struct avl_node *del))
{
   struct avl_node *parent;
   struct avl_node *node;
   struct avl_node tmp;
   int old_balance;
   int new_balance;
   int left;
   int right;
   int which_child;

   if (!tree && !del)
      return;

   /*
    * Deletion is easiest with a node that has at most 1 child.
    * We swap a node with 2 children with a sequentially valued
    * neighbor node. That node will have at most 1 child. Note this
    * has no effect on the ordering of the remaining nodes.
    *
    * As an optimization, we choose the greater neighbor if the tree
    * is right heavy, otherwise the left neighbor. This reduces the
    * number of rotations needed.
    */
   if (del->avl_child[0] != NULL && del->avl_child[1] != NULL)
   {

      /*
       * choose node to swap from whichever side is taller
       */
      old_balance = AVL_XBALANCE(del);
      left = avl_balance2child[old_balance + 1];
      right = 1 - left;

      /*
       * get to the previous value'd node
       * (down 1 left, as far as possible right)
       */
      for (node = del->avl_child[left];
           node->avl_child[right] != NULL;
           node = node->avl_child[right])
         ;
      /*
       * create a temp placeholder for 'node'
       * move 'node' to delete's spot in the tree
       */
      tmp = *node;

      *node = *del;
      if (node->avl_child[left] == node)
         node->avl_child[left] = &tmp;

      parent = AVL_XPARENT(node);
      if (parent != NULL)
         parent->avl_child[AVL_XCHILD(node)] = node;
      else
         tree->avl_root = node;
      AVL_SETPARENT(node->avl_child[left], node);
      AVL_SETPARENT(node->avl_child[right], node);

      /*
       * Put tmp where node used to be (just temporary).
       * It always has a parent and at most 1 child.
       */
      del = &tmp;
      parent = AVL_XPARENT(del);
      parent->avl_child[AVL_XCHILD(del)] = del;
      which_child = (del->avl_child[1] != 0);
      if (del->avl_child[which_child] != NULL)
         AVL_SETPARENT(del->avl_child[which_child], del);
   }


   /*
    * Here we know "delete" is at least partially a leaf node. It can
    * be easily removed from the tree.
    */
   --tree->avl_numnodes;
   parent = AVL_XPARENT(del);
   which_child = AVL_XCHILD(del);
   if (del->avl_child[0] != NULL)
      node = del->avl_child[0];
   else
      node = del->avl_child[1];

   /*
    * Connect parent directly to node (leaving out delete).
    */
   if (node != NULL)
   {
      AVL_SETPARENT(node, parent);
      AVL_SETCHILD(node, which_child);
   }
   if (parent == NULL)
   {
      tree->avl_root = node;
      return;
   }
   parent->avl_child[which_child] = node;


   /*
    * Since the subtree is now shorter, begin adjusting parent balances
    * and performing any needed rotations.
    */
   do {

      /*
       * Move up the tree and adjust the balance
       *
       * Capture the parent and which_child values for the next
       * iteration before any rotations occur.
       */
      node = parent;
      old_balance = AVL_XBALANCE(node);
      new_balance = old_balance - avl_child2balance[which_child];
      parent = AVL_XPARENT(node);
      which_child = AVL_XCHILD(node);

      /*
       * If a node was in perfect balance but isn't anymore then
       * we can stop, since the height didn't change above this point
       * due to a deletion.
       */
      if (old_balance == 0)
      {
         AVL_SETBALANCE(node, new_balance);
         break;
      }

      /*
       * If the new balance is zero, we don't need to rotate
       * else
       * need a rotation to fix the balance.
       * If the rotation doesn't change the height
       * of the sub-tree we have finished adjusting.
       */
      if (new_balance == 0)
         AVL_SETBALANCE(node, new_balance);
      else if (!avl__rotation(tree, node, new_balance))
         break;
   } while (parent != NULL);
}

/*
 * Walk from one node to the previous valued node (ie. an infix walk
 * towards the left). At any given node we do one of 2 things:
 *
 * - If there is a left child, go to it, then to it's rightmost descendant.
 *
 * - otherwise we return thru parent nodes until we've come from a right child.
 *
 * Return Value:
 * NULL - if at the end of the nodes
 * otherwise next node
 */
AVL_EXPORT(struct avl_node *avl_walk(struct avl_tree *tree, struct avl_node *node, int left))
{
   int right = 1 - left;
   int was_child;


   /*
    * nowhere to walk to if tree is empty
    */
   if (node == NULL)
      return (NULL);

   /*
    * Visit the previous valued node. There are two possibilities:
    *
    * If this node has a left child, go down one left, then all
    * the way right.
    */
   if (node->avl_child[left] != NULL)
   {
      for (node = node->avl_child[left];
           node->avl_child[right] != NULL;
           node = node->avl_child[right])
         ;
      /*
       * Otherwise, return thru left children as far as we can.
       */
   }
   else
   {
      for (;;)
      {
         was_child = AVL_XCHILD(node);
         node = AVL_XPARENT(node);
         if (node == NULL)
            return (NULL);
         if (was_child == right)
            break;
      }
   }

   return node;
}

/*
 * Return the lowest valued node in a tree or NULL.
 * (leftmost child from root of tree)
 */
AVL_EXPORT(struct avl_node *avl_first(struct avl_tree *tree))
{
   struct avl_node *node;
   struct avl_node *prev = NULL;

   for (node = tree->avl_root; node != NULL; node = node->avl_child[0])
      prev = node;

   if (prev != NULL)
      return prev;
   return (NULL);
}

/*
 * Return the highest valued node in a tree or NULL.
 * (rightmost child from root of tree)
 */
AVL_EXPORT(struct avl_node *avl_last(struct avl_tree *tree))
{
   struct avl_node *node;
   struct avl_node *prev = NULL;

   for (node = tree->avl_root; node != NULL; node = node->avl_child[1])
      prev = node;

   if (prev != NULL)
      return prev;
   return (NULL);
}

/*
 * Return the number of nodes in an AVL tree.
 */
AVL_EXPORT(unsigned long avl_numnodes(struct avl_tree *tree))
{
   return (tree->avl_numnodes);
}

#ifdef  __cplusplus
}
#endif
#endif /* !__AVL_INL_H__ */
