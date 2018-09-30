#ifndef __UTILS_H__
#define __UTILS_H__

typedef struct _FIFO_S FIFO_S;
typedef struct _TREE_S TREE_S, TNODE_S;

/**
 *   A simple fifo
 */
FIFO_S * fifo_create(char * buf, int size);
int fifo_destroy(FIFO_S * fifo);
int fifo_reset(FIFO_S * fifo);
int fifo_read(FIFO_S * fifo, char * buf, int size);
int fifo_write(FIFO_S * fifo, char * buf, int size);
int fifo_get_data_size(FIFO_S * fifo);
int fifo_get_free_size(FIFO_S * fifo);

/**
 *   A simple tree
 */
typedef void (*DELETE_CB)(void * data, int size);
TREE_S * tree_create(int data_size);
int tree_destroy(TREE_S * tree);
TNODE_S * tree_add(TNODE_S * tnode, int data_size);
int tree_delete(TNODE_S * tnode, DELETE_CB del_cb);
TNODE_S * tree_get_child(TNODE_S * tnode);
TNODE_S * tree_next_Sibling(TNODE_S * tnode);
void * tree_get_data(TNODE_S * tnode);

/**
 * string utils
 */
char * trim(char * str);

#endif /* __UTILS_H__ */
