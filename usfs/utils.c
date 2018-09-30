#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "deps.h"
#include "utils.h"

struct _FIFO_S
{
    int ci;
    int pi;
    int alloc;
    int size;
    char * buf;
};

struct _TREE_S
{
    struct _TREE_S * child; /* frist child */
    struct _TREE_S * sibling;
    struct _TREE_S * parent;
    int size;
    char data[];
};

FIFO_S * fifo_create(char * buf, int size)
{
    CHECK_RTNM(size <= 0, NULL, "param error! size:%d\n", size);

    FIFO_S * fifo = NULL;

    if (buf)
    {
        fifo = calloc(1, sizeof(FIFO_S));
        CHECK_RTNM(fifo == NULL, NULL, "calloc error! %s\n", strerror(errno));
        fifo->alloc = FALSE_E;
        fifo->buf = buf;
        fifo->size = size;
    }
    else
    {
        fifo = calloc(1, sizeof(FIFO_S) + size);
        CHECK_RTNM(fifo == NULL, NULL, "calloc error! %s\n", strerror(errno));
        fifo->alloc = TRUE_E;
        fifo->buf = (char *)fifo + sizeof(FIFO_S);
        fifo->size = size;
    }

    return fifo;
}

int fifo_destroy(FIFO_S * fifo)
{
    CHECK_RTNM(fifo == NULL, -1, "param error! fifo invalid!\n");

    free(fifo);

    return 0;
}

int fifo_reset(FIFO_S * fifo)
{
    CHECK_RTNM(fifo == NULL, -1, "param error! fifo invalid!\n");

    fifo->ci = fifo->pi = 0;

    return 0;
}

int fifo_read(FIFO_S * fifo, char * buf, int size)
{
    int len = fifo->size - fifo->ci;

    if (len < size)
    {
        memcpy(buf, fifo->buf + fifo->ci, len);
        memcpy(buf + len, fifo->buf, size - len);
        fifo->ci = size - len;
    }
    else
    {
        memcpy(buf, fifo->buf + fifo->ci, size);
        fifo->ci = fifo->ci + size;
    }

    return 0;
}

int fifo_write(FIFO_S * fifo, char * buf, int size)
{
    int len = fifo->size - fifo->pi;

    if (len < size)
    {
        memcpy(fifo->buf + fifo->pi, buf, len);
        memcpy(fifo->buf, buf + len, size - len);
        fifo->pi = size - len;
    }
    else
    {
        memcpy(fifo->buf + fifo->pi, buf, size);
        fifo->pi = fifo->pi + size;
    }

    return 0;
}

int fifo_get_data_size(FIFO_S * fifo)
{
    CHECK_RTNM(fifo == NULL, -1, "param error! fifo invalid!\n");

    if (fifo->pi >= fifo->ci)
    {
        return (fifo->pi - fifo->ci);
    }
    else
    {
        return (fifo->size - fifo->ci + fifo->pi);
    }
}

int fifo_get_free_size(FIFO_S * fifo)
{
    CHECK_RTNM(fifo == NULL, -1, "param error! fifo invalid!\n");

    if (fifo->pi >= fifo->ci)
    {
        return (fifo->size - fifo->pi + fifo->ci);
    }
    else
    {
        return (fifo->ci - fifo->pi);
    }
}

int tree_del_leaf(TNODE_S * tnode, DELETE_CB del_cb)
{
    CHECK_RTNM(tnode == NULL || tnode->child != NULL, -1, "param error! tree invalid!\n");

    TNODE_S * pnode = tnode->parent;

    /* root node */
    if (pnode == NULL)
    {
        if (del_cb)
        {
            del_cb(tnode->data, tnode->size);
        }
        free(tnode);
        //DBG_INFO("free tree node:%#x\n", (int)tnode);
        return 0;
    }

    TNODE_S * snode = pnode->child;
    CHECK_RTNM(snode == NULL, -1, "tree error!!\n");
    if (snode == tnode)
    {
        pnode->child = snode->sibling;
        if (del_cb)
        {
            del_cb(tnode->data, tnode->size);
        }
        free(tnode);
        //DBG_INFO("free node:%#x\n", (int)tnode);
        return 0;
    }

    while (snode)
    {
        if (snode->sibling == tnode)
        {
            snode->sibling = tnode->sibling;
            if (del_cb)
            {
                del_cb(tnode->data, tnode->size);
            }
            free(tnode);
            //DBG_INFO("free node:%#x\n", (int)tnode);
            break;
        }
        snode = snode->sibling;
    }

    return 0;
}

TREE_S * tree_create(int data_size)
{
    TREE_S * tree = calloc(1, sizeof(TREE_S) + data_size);
    CHECK_RTNM(tree == NULL, NULL, "calloc error! %s\n", strerror(errno));

    tree->size = data_size;

    //DBG_INFO("new tree node:%#x\n", (int)tree);
    return tree;
}

int tree_destroy(TREE_S * tree)
{
    CHECK_RTNM(tree == NULL, -1, "param error! tree invalid!\n");

    return 0;
}

TNODE_S * tree_add(TNODE_S * tnode, int data_size)
{
    CHECK_RTNM(tnode == NULL, NULL, "param error! tree invalid!\n");

    TNODE_S * new_node = calloc(1, sizeof(TREE_S) + data_size);
    CHECK_RTNM(new_node == NULL, NULL, "calloc error! %s\n", strerror(errno));

    new_node->size = data_size;

    if (tnode->child == NULL)
    {
        new_node->parent = tnode;
        tnode->child = new_node;
    }
    else
    {
        TNODE_S * slibing = tnode->child;
        while (slibing->sibling != NULL)
        {
            slibing = slibing->sibling;
        }
        new_node->parent = tnode;
        slibing->sibling = new_node;
    }

    //DBG_INFO("new node:%#x\n", (int)new_node);
    return new_node;
}

int tree_delete(TNODE_S * tnode, DELETE_CB del_cb)
{
    CHECK_RTNM(tnode == NULL, -1, "param error! tree invalid!\n");

    TNODE_S * dnode = NULL;
    TNODE_S * snode = tnode->child;
    while (snode)
    {
        dnode = snode;
        snode = snode->sibling;

        tree_delete(dnode, del_cb);
    }

    tree_del_leaf(tnode, del_cb);

    return 0;
}

TNODE_S * tree_get_child(TNODE_S * tnode)
{
    CHECK_RTNM(tnode == NULL, NULL, "param error! tnode invalid!\n");

    if (tnode->child)
    {
        return tnode->child;
    }
    return NULL;
}

TNODE_S * tree_next_Sibling(TNODE_S * tnode)
{
    CHECK_RTNM(tnode == NULL, NULL, "param error! tnode invalid!\n");

    if (tnode->sibling)
    {
        return tnode->sibling;
    }
    return NULL;
}

void * tree_get_data(TNODE_S * tnode)
{
    CHECK_RTNM(tnode == NULL, NULL, "param error! tnode invalid!\n");

    return (void *)tnode->data;
}

char * trim(char * str)
{
    char * p = str;

    while (isspace(*p))
    {
        p++;
    }
    str = p;
    p = str + strlen(str) - 1;
    while (isspace(*p))
    {
        --p;
    }
    *(p + 1) = '\0';

    return str;
}

