#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/types.h>

#define FUSE_USE_VERSION 26
#include "deps.h"
#include "utils.h"
#include "fuse.h"
#include "opera.h"
#include "usfs.h"

#define DEFAULT_FILE_SIZE   SIZE_4k

struct _USFS_FUSE_S
{
    char * root_dir;
    TREE_S * tree;
    struct fuse_args args;
    struct fuse * fuse;
    struct fuse_chan * ch;
    struct fuse_operations usfs_oper;
    USFS_OPERA opera_open;
    USFS_OPERA opera_close;
};

long long get_file_fd()
{
    static long long file_fd = 0;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&mutex);
    file_fd++;
    pthread_mutex_unlock(&mutex);

    return file_fd;
}

int stat_read(char * buf, int size, USFS_DATA_S * usfs_data)
{
    int rest = size, data_size = 0, read_size = 0, len = 0;
    FIFO_S * fifo = usfs_data->data;

    while (rest > 0)
    {
        pthread_mutex_lock(&usfs_data->mutex);
        data_size = fifo_get_data_size(fifo);
        if (data_size == 0)
        {
            if (usfs_data->op_stat == FILE_NOT_OPEN || usfs_data->op_stat == FILE_READ_END)
            {
                pthread_mutex_unlock(&usfs_data->mutex);
                break;
            }
            else
            {
                pthread_mutex_unlock(&usfs_data->mutex);
                usleep(1);
            }
        }

        len = MIN(data_size, rest);
        fifo_read(fifo, buf + read_size, len);
        read_size += len;
        rest -= len;
        pthread_mutex_unlock(&usfs_data->mutex);
    }
    usfs_data->st_size += read_size;

    return read_size;
}

int cfg_read(char * buf, int size, USFS_DATA_S * usfs_data)
{
    int ret = 0, read_size = 0;

    pthread_mutex_lock(&usfs_data->mutex);
    if (usfs_data->op_stat == FILE_READ_END)
    {
        read_size = 0;
    }
    else
    {
        CFG_ITEM_S * item = usfs_data->data;
        while (item)
        {
            if (item->isint)
            {
                ret = sprintf(buf + read_size, "%s = %d \n", item->name, *item->value);
            }
            else
            {
                ret = sprintf(buf + read_size, "%s = %s \n", item->name, item->str);
            }
            item = item->next;
            if (ret < 0)
            {
                break;
            }
            read_size += ret;
        }
        usfs_data->op_stat = FILE_READ_END;
    }
    pthread_mutex_unlock(&usfs_data->mutex);

    return read_size;
}

TNODE_S * path_get_node(const char * path, TREE_S * tree)
{
    char * file_path = strdup(path);
    CHECK_RTNM(file_path == NULL, NULL, "strdup failed! %s\n", strerror(errno));

    char * name = NULL;
    char * p = file_path;
    TNODE_S * tnode = tree;
    USFS_DATA_S * usfs_data = NULL;

    while (p && *p != '\0' && tnode)
    {
        while (*p == '/')
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }
        name = p;
        p = strchr(p, '/');
        if (p)
        {
            *p = '\0';
            p++;
        }
        if (tnode)
        {
            tnode = tree_get_child(tnode);
        }
        while (tnode)
        {
            usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
            if (strcmp(usfs_data->name, name) == 0)
            {
                break;
            }
            tnode = tree_next_Sibling(tnode);
        }
    }
    free(file_path);

    return tnode;
}

static int usfs_getattr(const char * path, struct stat * stbuf)
{
    int ret = 0;

    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;

    memset(stbuf, 0, sizeof(struct stat));

    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "");

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data == NULL, -ENOENT, "");

    if (usfs_data->ftype == FILE_DIR)
    {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    }
    else
    {
        CHECK_RTNM(tree_get_child(tnode) != NULL, -ENOENT, "Not reg file!");
        if (usfs_data->ftype == FILE_CFG)
        {
            stbuf->st_mode = S_IFREG | 0666;
        }
        else
        {
            stbuf->st_mode = S_IFREG | 0444;
        }
        stbuf->st_nlink = 1;
        stbuf->st_size = (usfs_data->st_size > 0) ? usfs_data->st_size : DEFAULT_FILE_SIZE;
    }
    stbuf->st_atime = time(NULL);
    stbuf->st_mtime = stbuf->st_atime;
    stbuf->st_ctime = stbuf->st_atime;

    DBG_INFO("path:%s st_size:%lld \n", path, stbuf->st_size);

    return ret;
}

static int usfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi)
{
    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;
    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "path:%s does not exist!", path);

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data->ftype != FILE_DIR, -ENOTDIR, "path:%s not a directory!", path);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    tnode = tree_get_child(tnode);
    while (tnode)
    {
        usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
        if (usfs_data->name)
        {
            filler(buf, usfs_data->name, NULL, 0);
        }
        tnode = tree_next_Sibling(tnode);
    }

    DBG_INFO("file_name:%s buf:%s fh:%lld\n", path, (char *)buf, fi->fh);
    return 0;
}

static int usfs_open(const char * path, struct fuse_file_info * fi)
{
    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;
    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "path:%s does not exist!", path);

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data->ftype == FILE_DIR, -EACCES, "path:%s not reg file!", path);

    usfs_data->st_size = 0;
    usfs_data->op_stat = FILE_OPENED;
    if (usfs_data->ftype == FILE_STAT)
    {
        usfs_fuse->opera_open(tnode);
    }
    fi->fh = get_file_fd();
    DBG_INFO("file_name:%s fh:%lld\n", path, fi->fh);

    return 0;
}

static int usfs_read(const char * path, char * buf, size_t size, off_t offset, struct fuse_file_info * fi)
{
    int read_size = 0;

    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;
    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "path:%s does not exist!", path);

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data->ftype == FILE_DIR, -EACCES, "path:%s not reg file!", path);

    if (usfs_data->ftype == FILE_STAT)
    {
        read_size = stat_read(buf, size, usfs_data);
    }
    else if (usfs_data->ftype == FILE_CFG)
    {
        read_size = cfg_read(buf, size, usfs_data);
    }
    DBG_INFO("path:%s name:%s buf:%#x size:%d offset:%d read_size:%d fh:%lld\n", path, usfs_data->name, (int)buf, (int)size, (int)offset, read_size, fi->fh);

    return read_size;
}

static int usfs_write(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fi)
{
    int len = strlen(buf);

    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;
    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "path:%s does not exist!", path);

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data->ftype != FILE_CFG, -EACCES, "path:%s not cfg file!", path);

    DBG_INFO("path:%s buf:%s\n", path, buf);

    char * p = strchr(buf, '=');
    CHECK_RTNM(p == NULL, -EFAULT, "param error!");

    *p = '\0';
    p++;

    char * key = trim((char *)buf);
    char * value = trim((char *)p);
    CHECK_RTNM(strlen(key) == 0 || strlen(value) == 0, -EFAULT, "param error!");

    CFG_ITEM_S * item = usfs_data->data;
    while (item)
    {
        if (strcmp(item->name, key) == 0)
        {
            if (item->isint)
            {
                *item->value = atoi(value);
            }
            else
            {
                strcpy(item->str, value);
            }
            break;
        }
        item = item->next;
    }

    return len;
}

static int usfs_release(const char * path, struct fuse_file_info * fi)
{
    struct fuse_context * fc = fuse_get_context();
    CHECK_RTNM(fc == NULL || fc->private_data == NULL, 0, "fuse_get_context failed! \n");

    USFS_FUSE_S * usfs_fuse = fc->private_data;
    TNODE_S * tnode = path_get_node(path, usfs_fuse->tree);
    CHECK_RTNM(tnode == NULL, -ENOENT, "path:%s does not exist!", path);

    USFS_DATA_S * usfs_data = (USFS_DATA_S *)tree_get_data(tnode);
    CHECK_RTNM(usfs_data->ftype == FILE_DIR, -EACCES, "path:%s not reg file!", path);

    usfs_data->st_size = 0;
    usfs_data->op_stat = FILE_NOT_OPEN;

    return 0;
}

static int usfs_truncate(const char * path, off_t offset)
{
    DBG_INFO("path:%s offset:%d\n", path, (int)offset);

    return 0;
}

void usfs_fill_opera(struct fuse_operations * opera)
{
    opera->getattr  = usfs_getattr;
    opera->readdir  = usfs_readdir;
    opera->open     = usfs_open;
    opera->read     = usfs_read;
    opera->write    = usfs_write;
    opera->release  = usfs_release;
    opera->truncate = usfs_truncate;
}

USFS_FUSE_S * usfs_fuse_init(char * root_dir, TREE_S * tree, USFS_OPERA opera_open, USFS_OPERA opera_close)
{
    CHECK_RTNM(root_dir == NULL, NULL, "param error,root_dir invalid!\n");

#ifdef DEBUG
    int argc = 3;
    char * argv[] = {"usfs", "-d", root_dir, NULL};
#else
    int argc = 2;
    char * argv[] = {"usfs", root_dir, NULL};
#endif

    USFS_FUSE_S * usfs_fuse = calloc(1, sizeof(USFS_FUSE_S));
    CHECK_RTNM(usfs_fuse == NULL, NULL, "calloc failed! %s\n", strerror(errno));

    usfs_fuse->tree = tree;
    usfs_fuse->opera_open = opera_open;
    usfs_fuse->opera_close = opera_close;
    usfs_fuse->root_dir = strdup(root_dir);

    //char * mountpoint = NULL;
    usfs_fuse->args.argc = argc;
    usfs_fuse->args.argv = argv;
    fuse_parse_cmdline(&usfs_fuse->args, NULL, NULL, NULL);

    usfs_fuse->ch = fuse_mount(root_dir, &usfs_fuse->args);
    CHECK_GOTOM(usfs_fuse->ch == NULL, RELEASE, "fuse_mount failed\n");

    usfs_fill_opera(&usfs_fuse->usfs_oper);
    usfs_fuse->fuse = fuse_new(usfs_fuse->ch, &usfs_fuse->args, &usfs_fuse->usfs_oper, sizeof(struct fuse_operations), usfs_fuse);
    CHECK_GOTOM(usfs_fuse->fuse == NULL, RELEASE, "fuse_new failed\n");

    return usfs_fuse;

RELEASE:
    usfs_fuse_clean(usfs_fuse);

    return 0;
}

int usfs_fuse_loop(USFS_FUSE_S * usfs_fuse)
{
    CHECK_RTNM(usfs_fuse == NULL, -1, "param error,usfs_fuse invalid!\n");

    int ret = fuse_loop(usfs_fuse->fuse);
    if (ret < 0)
    {
        usfs_fuse_clean(usfs_fuse);
        return -1;
    }

    return 0;
}

int usfs_fuse_clean(USFS_FUSE_S * usfs_fuse)
{
    CHECK_RTNM(usfs_fuse == NULL, -1, "param error,usfs_fuse invalid!\n");

    if (usfs_fuse->fuse)
    {
        fuse_exit(usfs_fuse->fuse);
        if (usfs_fuse->ch)
        {
            fuse_unmount(usfs_fuse->root_dir, usfs_fuse->ch);
        }
        fuse_destroy(usfs_fuse->fuse);
    }

    fuse_opt_free_args(&usfs_fuse->args);

    if (usfs_fuse->root_dir)
    {
        free(usfs_fuse->root_dir);
    }

    free(usfs_fuse);

    return 0;
}

