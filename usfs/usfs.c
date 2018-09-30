#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>

#include "deps.h"
#include "utils.h"
#include "opera.h"
#include "usfs.h"

#define MAX_QUE_LEN     16
#define USFS_FLAG       0x11223344
#define FORMAT_BUF_LEN  (MAX_FORMAT_STR_LEN * 2 + 16)

typedef struct
{
    int printed;    /* table header printed */
    char * header_buf;
    char * row_buf;
} FORMAT_TAB_S;

/* usfs mount point manage*/
typedef struct _USFS_MP_S
{
    char * root_dir;
    TREE_S * tree;
    USFS_FUSE_S * usfs_fuse;
    pthread_t tid_fuse;
} USFS_MP_S;

typedef struct
{
    int flag;
    int running;
    USFS_MP_S usfs_mp;
    FIFO_S  * fifo;
    TNODE_S * cur_node;
    TNODE_S * run_queue[MAX_QUE_LEN];
    char * buf;
    CFG_ITEM_S ** cur_item;
    pthread_mutex_t mutex_mp;
    pthread_mutex_t mutex_queue;
    pthread_cond_t cond;
    pthread_t tid_output;
} USFS_GLOBAL_S;

static USFS_GLOBAL_S gusfs = {0};

static void opera_open(TNODE_S * node)
{
    int i = 0;

    pthread_mutex_lock(&gusfs.mutex_queue);

    USFS_DATA_S * usfs_data = tree_get_data(node);
    CHECK_GOTOM(usfs_data == NULL, RELEASE, "tree_get_data failed!\n");

    usfs_data->data = gusfs.fifo;

    for (i = 0; i < MAX_QUE_LEN; i++)
    {
        if (gusfs.run_queue[i] == NULL)
        {
            gusfs.run_queue[i] = node;
            break;
        }
    }

    if (gusfs.cur_node == NULL)
    {
        gusfs.cur_node = node;
    }
    pthread_cond_signal(&gusfs.cond);

RELEASE:
    pthread_mutex_unlock(&gusfs.mutex_queue);
}

static void opera_close(TNODE_S * node)
{
    pthread_mutex_lock(&gusfs.mutex_queue);

    USFS_DATA_S * usfs_data = tree_get_data(node);
    CHECK_GOTOM(usfs_data == NULL, RELEASE, "tree_get_data failed!\n");

    fifo_reset((FIFO_S *)usfs_data->data);
    usfs_data->data = NULL;
RELEASE:
    pthread_mutex_unlock(&gusfs.mutex_queue);
}

static void usfs_data_del_cb(void * data, int size)
{
    CHECK_RTNVM(data == NULL, "data invalid\n");

    USFS_DATA_S * usfs_data = data;
    CHECK_RTNVM(size != sizeof(USFS_DATA_S), "size invalid,exp:%d act:%d\n", sizeof(USFS_DATA_S), size);

    free(usfs_data->name);
    //pthread_mutex_destroy(&usfs_data->mutex);

    if (usfs_data->ftype == FILE_CFG)
    {
        CFG_ITEM_S * ditem = NULL;
        CFG_ITEM_S * item = (CFG_ITEM_S *)usfs_data->data;
        while (item)
        {
            ditem = item;
            item = item->next;

            free(ditem->name);
            free(ditem);
        }
    }
}

static void thread_fuse(void * arg)
{
    USFS_MP_S * usfs_mp = (USFS_MP_S *)arg;

    usfs_fuse_loop(usfs_mp->usfs_fuse);
}

static void thread_output()
{
    int i = 0;

    while (gusfs.running)
    {
        pthread_mutex_lock(&gusfs.mutex_queue);
        while (gusfs.cur_node == NULL)
        {
            pthread_cond_wait(&gusfs.cond, &gusfs.mutex_queue);
        }
        pthread_mutex_unlock(&gusfs.mutex_queue);

        for (i = 0; i < MAX_QUE_LEN; i++)
        {
            if (gusfs.run_queue[i] == gusfs.cur_node)
            {
                gusfs.run_queue[i] = NULL;
                break;
            }
        }

        USFS_DATA_S * usfs_data = tree_get_data(gusfs.cur_node);
        if (usfs_data && usfs_data->ftype == FILE_STAT)
        {
            usfs_data->op_stat = FILE_READ_START;
            usfs_data->usfs_cb(usfs_data->arg, usfs_data->size);
            usfs_data->op_stat = FILE_READ_END;
        }
        gusfs.cur_node = NULL;
        for (i = 0; i < MAX_QUE_LEN; i++)
        {
            if (gusfs.run_queue[i])
            {
                gusfs.cur_node = gusfs.run_queue[i];
                break;
            }
        }
    }
}

int usfs_create(char * root_dir)
{
    int ret = 0;

    pthread_mutex_init(&gusfs.mutex_mp, NULL);
    pthread_mutex_init(&gusfs.mutex_queue, NULL);
    pthread_cond_init(&gusfs.cond, NULL);

    gusfs.running = TRUE_E;
    ret = pthread_create(&gusfs.tid_output, NULL, (void *)thread_output, NULL);
    CHECK_RTNM(ret < 0, -1, "pthread_create error!\n");

    gusfs.fifo = fifo_create(NULL, SIZE_4k);
    CHECK_GOTOM(gusfs.fifo == NULL, RELEASE, "fifo_create error!\n");

    gusfs.buf = calloc(1, FORMAT_BUF_LEN);
    CHECK_GOTOM(gusfs.buf == NULL, RELEASE, "calloc error! %s\n", strerror(errno));

    USFS_MP_S * usfs_mp = &gusfs.usfs_mp;
    usfs_mp->root_dir = strdup(root_dir);

    usfs_mp->tree = tree_create(sizeof(USFS_DATA_S));
    CHECK_GOTOM(usfs_mp->tree == NULL, RELEASE, "tree_create failed!\n");

    USFS_DATA_S * usfs_data = tree_get_data(usfs_mp->tree);
    CHECK_GOTOM(usfs_data == NULL, RELEASE, "tree_get_data failed!\n");

    usfs_data->name = strdup("/");
    usfs_data->ftype = FILE_DIR;

    usfs_mp->usfs_fuse = usfs_fuse_init(root_dir, usfs_mp->tree, opera_open, opera_close);
    CHECK_GOTOM(usfs_mp->usfs_fuse == NULL, RELEASE, "usfs_fuse_init failed!\n");

    ret = pthread_create(&usfs_mp->tid_fuse, NULL, (void *)thread_fuse, usfs_mp);
    CHECK_GOTOM(ret < 0, RELEASE, "tree_get_data failed!\n");

    return 0;

RELEASE:
    usfs_destroy();
    return -1;
}

int usfs_destroy()
{
    pthread_t tid;
    memset(&tid, 0, sizeof(pthread_t));

    gusfs.running = FALSE_E;
    pthread_mutex_lock(&gusfs.mutex_queue);
    gusfs.cur_node = gusfs.usfs_mp.tree;
    pthread_cond_signal(&gusfs.cond);
    pthread_mutex_unlock(&gusfs.mutex_queue);
    if (!pthread_equal(tid, gusfs.tid_output))
    {
        pthread_join(gusfs.tid_output, NULL);
    }
    pthread_cond_destroy(&gusfs.cond);

    USFS_MP_S * usfs_mp = &gusfs.usfs_mp;
    if (usfs_mp->usfs_fuse)
    {
        usfs_fuse_clean(usfs_mp->usfs_fuse);
    }
    if (usfs_mp->tree)
    {
        tree_delete(usfs_mp->tree, usfs_data_del_cb);
    }
    if (usfs_mp->root_dir)
    {
        free(usfs_mp->root_dir);
    }
    memset(usfs_mp, 0, sizeof(USFS_MP_S));

    if (gusfs.fifo)
    {
        fifo_destroy(gusfs.fifo);
    }
    if (gusfs.buf)
    {
        free(gusfs.buf);
    }
    memset(&gusfs, 0, sizeof(USFS_GLOBAL_S));

    return 0;
}

int usfs_mkdir(char * path, char * dir_name)
{
    DBG_INFO("mkdir file_name:%s/%s \n", path, dir_name);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", path);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, dir_name) == 0)
        {
            DBG_ERR("can not create same file:%s\n", dir_name);
            return -1;
        }
        tnode =  tree_next_Sibling(tnode);
    }

    tnode = tree_add(pnode, sizeof(USFS_DATA_S));
    CHECK_RTNM(tnode == NULL, -1, "tree_add failed! %s/%s\n", path, dir_name);

    usfs_data = tree_get_data(tnode);
    CHECK_RTNM(usfs_data == NULL, -1, "tree_get_data failed! %s/%s\n", path, dir_name);

    usfs_data->name = strdup(dir_name);
    usfs_data->ftype = FILE_DIR;

    DBG_INFO("end file_name:%s/%s \n", path, dir_name);

    return 0;
}

int usfs_rmdir(char * path, char * dir_name, int force)
{
    DBG_INFO("rmdir file_name:%s/%s force:%d\n", path, dir_name,force);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", path);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, dir_name) == 0)
        {
            CHECK_RTNM(usfs_data->ftype != FILE_DIR, -1, "%s/%s not directory!\n", path, dir_name);
            CHECK_RTNM(!force && tree_get_child(tnode) , -1, "%s/%s not an empty directory!\n", path, dir_name);

            return tree_delete(tnode, usfs_data_del_cb);
        }
        tnode =  tree_next_Sibling(tnode);
    }

    return -1;
}

int usfs_create_sfile(char * path, char * file_name, USFS_CB usfs_cb, void * arg, int size)
{
    DBG_INFO("create status file_name:%s/%s size:%d\n", path, file_name, size);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", file_name);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, file_name) == 0)
        {
            DBG_ERR("can not create same file:%s\n", file_name);
            return -1;
        }
        tnode =  tree_next_Sibling(tnode);
    }

    tnode = tree_add(pnode, sizeof(USFS_DATA_S));
    CHECK_RTNM(tnode == NULL, -1, "tree_add failed! %s/%s\n", path, file_name);

    usfs_data = tree_get_data(tnode);
    CHECK_RTNM(usfs_data == NULL, -1, "tree_get_data failed! %s/%s\n", path, file_name);

    usfs_data->name = strdup(file_name);
    usfs_data->ftype = FILE_STAT;
    usfs_data->op_stat = FILE_NOT_OPEN;
    usfs_data->usfs_cb = usfs_cb;
    usfs_data->arg = arg;
    usfs_data->size = size;
    pthread_mutex_init(&usfs_data->mutex, NULL);

    DBG_INFO("ok file_name:%s/%s size:%d\n", path, file_name, size);
    return 0;
}

int usfs_delete_sfile(char * path, char * file_name)
{
    DBG_INFO("delete status file_name:%s/%s!\n", path, file_name);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", file_name);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, file_name) == 0)
        {
            CHECK_RTNM(usfs_data->ftype != FILE_STAT, -1, "%s/%s not a status file!\n", path, file_name);
            return tree_delete(tnode, usfs_data_del_cb);
        }
        tnode =  tree_next_Sibling(tnode);
    }

    return -1;
}

int usfs_create_cfile(char * path, char * file_name, USFS_CB usfs_cb, void * arg, int size)
{
    DBG_INFO("start file_name:%s/%s size:%d\n", path, file_name, size);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", file_name);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, file_name) == 0)
        {
            DBG_ERR("can not create same file:%s\n", file_name);
            return -1;
        }
        tnode =  tree_next_Sibling(tnode);
    }

    tnode = tree_add(pnode, sizeof(USFS_DATA_S));
    CHECK_RTNM(tnode == NULL, -1, "tree_add failed! %s/%s\n", path, file_name);

    usfs_data = tree_get_data(tnode);
    CHECK_RTNM(usfs_data == NULL, -1, "tree_get_data failed! %s/%s\n", path, file_name);

    usfs_data->name = strdup(file_name);
    usfs_data->ftype = FILE_CFG;

    pthread_mutex_init(&usfs_data->mutex, NULL);
    pthread_mutex_lock(&gusfs.mutex_mp);
    gusfs.cur_item = (CFG_ITEM_S **)&usfs_data->data;
    usfs_cb(arg, size);
    gusfs.cur_item = NULL;
    pthread_mutex_unlock(&gusfs.mutex_mp);

    CFG_ITEM_S * item = usfs_data->data;
    while (item)
    {
        if (item->isint)
        {
            printf("[%s] = [%d]\n", item->name, *item->value);
        }
        else
        {
            printf("[%s] = [%s]\n", item->name, item->str);
        }
        item = item->next;
    }

    DBG_INFO("ok file_name:%s/%s size:%d\n", path, file_name, size);
    return 0;
}

int usfs_delete_cfile(char * path, char * file_name)
{
    DBG_INFO("delete config file_name:%s/%s!\n", path, file_name);

    TNODE_S * pnode = path_get_node(path, gusfs.usfs_mp.tree);
    CHECK_RTNM(pnode == NULL, -1, "path error! %s\n", file_name);

    USFS_DATA_S * usfs_data = tree_get_data(pnode);
    CHECK_RTNM(!usfs_data || usfs_data->ftype != FILE_DIR, -1, "path:%s invalid!\n", path);

    TNODE_S * tnode = tree_get_child(pnode);
    while (tnode)
    {
        usfs_data = tree_get_data(tnode);
        if (usfs_data && strcmp(usfs_data->name, file_name) == 0)
        {
            CHECK_RTNM(usfs_data->ftype != FILE_CFG, -1, "%s/%s not a config file!\n", path, file_name);
            return tree_delete(tnode, usfs_data_del_cb);
        }
        tnode =  tree_next_Sibling(tnode);
    }

    return -1;
}

int usfs_add_cfg_int(int hide, char * name, int * value)
{
    CHECK_RTNM(gusfs.cur_item == NULL , -1, "param error!\n");

    CFG_ITEM_S * item = NULL;
    CFG_ITEM_S * new_item = calloc(1, sizeof(CFG_ITEM_S));
    CHECK_RTNM(new_item == NULL, -1, "calloc error! %s\n", strerror(errno));

    new_item->hide = hide;
    new_item->name = strdup(name);
    new_item->isint = TRUE_E;
    new_item->value = value;
    new_item->next = NULL;

    item = *gusfs.cur_item;
    if (!item)
    {
        *gusfs.cur_item = new_item;
    }
    else
    {
        while (item->next)
        {
            item = item->next;
        }
        item->next = new_item;
    }
    return 0;
}

int usfs_add_cfg_str(int hide, char * name, char * str, int max_len)
{
    CHECK_RTNM(gusfs.cur_item == NULL , -1, "param error!\n");

    CFG_ITEM_S * item = NULL;
    CFG_ITEM_S * new_item = calloc(1, sizeof(CFG_ITEM_S));
    CHECK_RTNM(new_item == NULL, -1, "calloc error! %s\n", strerror(errno));

    new_item->hide = hide;
    new_item->name = strdup(name);
    new_item->isint = FALSE_E;
    new_item->str = str;
    new_item->max = max_len;
    new_item->next = NULL;

    item = *gusfs.cur_item;
    if (!item)
    {
        *gusfs.cur_item = new_item;
    }
    else
    {
        while (item->next)
        {
            item = item->next;
        }
        item->next = new_item;
    }
    return 0;
}

int usfs_printf(char * format, ...)
{
    int rest = 0, free_size = 0, len = 0;
    char buf[FORMAT_BUF_LEN];
    va_list va;
    USFS_DATA_S * usfs_data = NULL;
    FIFO_S * fifo = NULL;

    CHECK_RTNM(gusfs.cur_node == NULL , -1, "param error!\n");

    va_start(va, format);
    rest = vsnprintf(buf, FORMAT_BUF_LEN - 1, format, va);
    va_end(va);
    CHECK_RTNM(rest < 0 , -1, "fomat string too long! format:%s\n", format);

    while (rest > 0)
    {
        usfs_data = tree_get_data(gusfs.cur_node);
        if (usfs_data)
        {
            fifo = usfs_data->data;
            pthread_mutex_lock(&usfs_data->mutex);
            free_size = fifo_get_free_size(fifo);
            if (free_size == 0)
            {
                pthread_mutex_unlock(&gusfs.mutex_queue);
                usleep(1);
            }

            len = MIN(free_size, rest);
            fifo_write(fifo, buf, len);
            rest -= len;
            pthread_mutex_unlock(&usfs_data->mutex);
        }
    }

    return 0;
}

int usfs_format_header(char * format, ...)
{
    return 0;
}

int usfs_ftab_start(char * format, ...)
{
    CHECK_RTNM(gusfs.buf == NULL , -1, "param error!\n");

    memset(gusfs.buf, 0, FORMAT_BUF_LEN);

    FORMAT_TAB_S * tab = (FORMAT_TAB_S *)gusfs.buf;
    tab->header_buf = gusfs.buf + sizeof(FORMAT_TAB_S);
    tab->row_buf = gusfs.buf + sizeof(FORMAT_TAB_S) + MAX_FORMAT_STR_LEN;

    /* print table name string */
    char buf[128] = {0};
    memset(buf, '-', 64);

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buf + 5, 64, format, va);
    buf[len + 5] = '-';
    va_end(va);

    usfs_printf("\n%s\n", buf);

    return 0;
}

int usfs_ftab_add(char * name, char * format, ...)
{
    CHECK_RTNM(gusfs.buf == NULL , -1, "param error!\n");

    FORMAT_TAB_S * tab = (FORMAT_TAB_S *)gusfs.buf;

    if (!tab->printed)
    {
        sprintf(tab->header_buf + strlen(tab->header_buf), " %s", name);
    }

    va_list va;
    char buf[MAX_FORMAT_STR_LEN] = {0};

    /* frist print table message into a tmp buf */
    va_start(va , format);
    vsnprintf(buf, MAX_FORMAT_STR_LEN, format, va);
    va_end(va);

    /* add message to row_buf align right */
    sprintf(tab->row_buf + strlen(tab->row_buf), " %*s", strlen(name), buf);

    return 0;
}

int usfs_ftab_addl(char * name, char * format, ...)
{
    CHECK_RTNM(gusfs.buf == NULL , -1, "param error!\n");

    FORMAT_TAB_S * tab = (FORMAT_TAB_S *)gusfs.buf;

    if (!tab->printed)
    {
        sprintf(tab->header_buf + strlen(tab->header_buf), " %s", name);
    }

    va_list va;
    char buf[MAX_FORMAT_STR_LEN] = {0};

    va_start(va , format);
    vsnprintf(buf, MAX_FORMAT_STR_LEN, format, va);
    va_end(va);

    /* add message to row_buf align left */
    sprintf(tab->row_buf + strlen(tab->row_buf), " %-*s", strlen(name), buf);

    return 0;
}


int usfs_ftab_end()
{
    CHECK_RTNM(gusfs.buf == NULL , -1, "param error!\n");

    FORMAT_TAB_S * tab = (FORMAT_TAB_S *)gusfs.buf;

    if (!tab->printed)
    {
        usfs_printf("%s\n", tab->header_buf);
        tab->printed = TRUE_E;
    }

    usfs_printf("%s\n", tab->row_buf);
    memset(tab->row_buf, 0, MAX_FORMAT_STR_LEN);

    return 0;
}


