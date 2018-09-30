#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usfs.h"
#include "deps.h"
#include "utils.h"

#define ROOT_PATH   "/"
#define ROOT_DIR    "/dir"

int tab_row = 10;
int loop = 20;
char str[32] = {"hello"};

void table_cb(void * arg, int size)
{
    int i = 0;
    int row = *(int *)arg;

    usfs_ftab_start("A TABLE TEST");
    for (i = 0; i < row; i++)
    {
        usfs_ftab_add("RowId", "%d", i);
        usfs_ftab_add(" Col1 ", "%d", i * 2);
        usfs_ftab_add(" Col2 ", "%d", i * 3);
        usfs_ftab_add("   Col3", "%d", i * 5);
        usfs_ftab_addl("Col4  ", "%d", i * 7);
        usfs_ftab_addl("Col5", "%d", i * 9);
        usfs_ftab_end();
    }
}

void general_cb(void * arg, int size)
{
    int i = 0;
    int max = *(int *)arg;

    for (i = 0; i < max; i++)
    {
        usfs_printf("i:%d hello world, rtsp_cb\n", i);
    }
}

void dir_cb(void * arg, int size)
{
    char * str1 = arg;

    usfs_printf("This gui! str:%s\n", str1);
}

void cfg_cb(void * arg, int size)
{
    usfs_add_cfg_int(0, "tabRow", &tab_row);
    usfs_add_cfg_int(0, "loop", &loop);
    usfs_add_cfg_str(0, "str", str, sizeof(str));
}

typedef struct
{
    int val;
} NODE_DATA_S;

TNODE_S * add_node(TNODE_S * tnode, int val)
{
    TNODE_S * cnode = tree_add(tnode, sizeof(NODE_DATA_S));
    CHECK_RTNM(cnode == NULL, NULL, "tree_add error!");

    NODE_DATA_S * data = tree_get_data(cnode);
    if (data)
    {
        data->val = val;
        DBG_INFO("new node:%#x val:%d\n", (int)cnode, val);
    }

    return cnode;
}

static void del_cb(void * data, int size)
{
    NODE_DATA_S * node_data = data;

    if (node_data)
    {
        DBG_INFO("del node:%#x val:%d\n", (int)((char *)data - 16), node_data->val);
    }
}

int tree_test()
{
    int val = 1;

    TREE_S * tree = tree_create(sizeof(NODE_DATA_S));
    CHECK_RTNM(tree == NULL, -1, "tree_create error!");

    TNODE_S * tnode_a1 = add_node(tree, val++);
    CHECK_RTNM(tnode_a1 == NULL, -1, "tree_add error!");
    {
        add_node(tnode_a1, val++);
        add_node(tnode_a1, val++);
        add_node(tnode_a1, val++);
    }

    TNODE_S * tnode_b1 = add_node(tree, val++);
    CHECK_RTNM(tnode_b1 == NULL, -1, "tree_add error!");
    {
        add_node(tnode_b1, val++);
        add_node(tnode_b1, val++);
        add_node(tnode_b1, val++);
    }
    tree_delete(tnode_b1, del_cb);

    TNODE_S * tnode_c1 = add_node(tree, val++);
    CHECK_RTNM(tnode_c1 == NULL, -1, "tree_add error!");
    {
        add_node(tnode_c1, val++);
        add_node(tnode_c1, val++);
        add_node(tnode_c1, val++);
    }

    TNODE_S * tnode_d1 = add_node(tree, val++);
    CHECK_RTNM(tnode_d1 == NULL, -1, "tree_add error!");
    {
        add_node(tnode_b1, val++);
        add_node(tnode_b1, val++);
        add_node(tnode_b1, val++);
    }

    int i = 0;
    TNODE_S * tnode = tree;
    for (i = 0; i < 10; i++)
    {
        tnode = add_node(tnode, val++);
        add_node(tnode, val++);
    }
    tree_delete(tree, del_cb);

    return 0;
}

int main()
{
    int ret = 0;

#if 0
    tree_test();
#else
    ret = usfs_create("hfs");
    CHECK_RTNM(ret < 0, -1, "usfs_init error ret:%d\n", ret);

    usfs_create_sfile(ROOT_PATH, "general", general_cb, &loop, sizeof(loop));
    usfs_create_sfile(ROOT_PATH, "table", table_cb, &tab_row, sizeof(tab_row));
    usfs_create_cfile(ROOT_PATH, "cfg", cfg_cb, NULL, 0);

    ret = usfs_mkdir("/", "dir");
    if (ret >= 0)
    {
        usfs_create_sfile(ROOT_DIR, "dir", dir_cb, str, sizeof(str));
    }

    while (getchar() != 'q')
    {
        usleep(1000);
    }
    usfs_delete_cfile(ROOT_PATH, "cfg");
    usfs_delete_sfile(ROOT_PATH, "table");
    usfs_delete_sfile(ROOT_PATH, "general");
    usfs_rmdir("/", "dir", TRUE_E);

    usfs_destroy();
#endif
    return 0;
}
