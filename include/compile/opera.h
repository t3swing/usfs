#ifndef __COMPILE_H__
#define __COMPILE_H__

#include "utils.h"
#include "usfs.h"

typedef enum
{
    FILE_NOT_OPEN = 0,
    FILE_OPENED = 1,
    FILE_READ_START = 2,
    FILE_READ_END = 3,
}FILE_OPERA_STAT_E;

typedef enum
{
    FILE_STAT   = 0,
    FILE_CFG    = 1,
    FILE_DIR    = 2,
}FILE_TYPE_E;

typedef struct _CFG_ITEM_S
{
    int hide;
    char * name;
    int isint;
    int * value;
    int max;
    char * str;
    struct _CFG_ITEM_S * next;
}CFG_ITEM_S;

typedef struct
{
    char * name;
    int st_size;
    int ftype;
    int op_stat;
    void * arg;
    int size;
    USFS_CB usfs_cb;
    void * data;
    pthread_mutex_t mutex;
} USFS_DATA_S;

typedef struct _USFS_FUSE_S USFS_FUSE_S;
typedef void (*USFS_OPERA)(TNODE_S * node);

USFS_FUSE_S * usfs_fuse_init(char * root_dir, TREE_S * tree,USFS_OPERA opera_open,USFS_OPERA opera_close);
int usfs_fuse_loop(USFS_FUSE_S * usfs_fuse);
int usfs_fuse_clean(USFS_FUSE_S * usfs_fuse);

TNODE_S * path_get_node(const char * path, TREE_S * tree);

#endif /* __COMPILE_H__ */

