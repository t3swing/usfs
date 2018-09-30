#ifndef __USFS_H__
#define __USFS_H__

/* format string len limit */
#define MAX_FORMAT_STR_LEN  (4*1024)
#define FORMAT_CHECK(a,b)  __attribute__((format(printf, (a), (b))))

/**
 * the callback fuction for one usfs file
 * you can use usfs_printf(or table,tree) api in this function,
 * usfs file will output what you are input with usfs_printf.
 *
 * @arg: the arg you passed in usfs_create_file function
 * @size: the size of the arg
 */
typedef void (*USFS_CB)(void * arg, int size);

/**
 * create and destroy user stat file system
 *
 * @root_dir: the path fuse will mounte on
 * @return:  success , <0 failed
 */
int usfs_create(char * root_dir);
int usfs_destroy();

/**
 * mk and rm directory on the certain postion
 *
 * @path: the path of the dir which is relative path for root_dir
 * @dir_name: directory name
 * @force: if you want remove not empty directory,set it true
 * @return: 0 success , <0 failed
 */
int usfs_mkdir(char * path, char * dir_name);
int usfs_rmdir(char * path, char * dir_name,int force);

/** 
 * create or delete a user status file in usfs
 *
 * @path: the path of the dir which is relative path for root_dir
 * @file_name: user stat file
 * @usfs_cb: you can write messege to file_name through this usfs_cb
 * @arg: the arg for usfs_cb,it can be use in usfs_cb
 * @size: the size for the arg
 * @return: 0 success , <0 failed
 */
int usfs_create_sfile(char * path, char * file_name, USFS_CB usfs_cb, void * arg, int size);
int usfs_delete_sfile(char * path, char * file_name);

/** 
 * create or delet a user configure file in usfs
 * you can change configure item by comand echo,example:
 * #echo "cfgname = 1" > /usfs/cfg 
 *
 * @path: the path of the dir which is relative path for root_dir
 * @file_name: user stat file
 * @usfs_cb: you should use usfs_add_cfg_int/string in this usfs_cb
 * @arg: the arg for usfs_cb,it can be use in usfs_cb
 * @size: the size for the arg
 * @return: 0 success , <0 failed
 */
int usfs_create_cfile(char * path, char * file_name,USFS_CB usfs_cb, void * arg, int size);
int usfs_delete_cfile(char * path, char * file_name);

/**
 * a basic output fuction in usfs,just like printf, you should use it in usfs_create_sfile fuction's usfs_cb
 *
 * @format: like printf
 * @return: 0 success , <0 failed
 */
int usfs_printf(char * format, ...) FORMAT_CHECK(1, 2);

/**
 *  regist configure item in cfg file ,you should use it in usfs_create_cfile fuction's usfs_cb
 *
 * @hide: if hide,the item will not show when cat the cfg file
 * @name: the name of the configure item 
 * @value: the pointer of the integer value
 * @str: the pointer of the string
 * @max_len: the string buffer len
 * @return: 0 success , <0 failed
 */
int usfs_add_cfg_int(int hide,char * name, int * value);
int usfs_add_cfg_str(int hide,char * name, char * str,int max_len);

/* ------------------------------------------------------------------------- *
 * usfs provided lot of api for more format style,suggest to use it          *
 * like usfs_printf,you can use it in usfs_create_sfile fuction's usfs_cb    *
 * ------------------------------------------------------------------------- */

/**
 * a general header for all usfs file
 */
int usfs_format_header(char * format, ...) FORMAT_CHECK(1, 2);

/**
 * fomat style Table, have header,can add whith colounm
 */
int usfs_ftab_start(char * format, ...) FORMAT_CHECK(1, 2);
int usfs_ftab_add(char * name, char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftab_addl(char * name, char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftab_end();

/**
 * fomat style Table,
 */
int usfs_ftree_start(char * format, ...) FORMAT_CHECK(1, 2);
int usfs_ftree_add_child(int level,char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftree_add_sibling(int level,char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftree_printf(int level,char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftree_end();

#endif /* __USFS_H__ */
