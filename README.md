## USFS介绍 ##
USFS(user status file system)用户状态系统，是一个基于libfuse的专为展示应用程序状态信息而设计的一个小系统。简化了状态信息系统设计，用很少的代码就可以打造一个较为完善（带vfs功能）的状态信息系统。大家可以在https://github.com/t3swing/usfs 获取源码及更多信息。

支持以下功能：
* 文件及目录创建，支持树状目录结构（方便分模块、展示临时对象状态）
* 支持状态文件和配置文件两种类型
* 支持普通格式输出和表格式输出（表格输出，简化数组或多对象输出）
* 配置文件支持echo修改某项配置（支持vfs操作，简化配置接口）

linux的proc虚拟文件系统，支持状态和配置两种形式的文件，USFS也支持这两种类型文件，状态文件只读，主要是状态信息输出（格式可自定义），配置文件可读写，需要注意的是，配置文件格式与linux的不同，USFS的配置是key-value对的形式，中间用=号隔开(key与value两边的空格都会去掉，value中间的空格有效)。如：

> sw@t3swing:usfs$ cat hfs/cfg</br>
> tabRow = 10 </br>
> loop = 20 </br>
> str = hello world </br>

修改配置文件，使用echo命令即可，如# echo loop=10 > hfs/cfg 或# echo "loop = 10" >> hfs/cfg ,此时再查看loop的值就能看到变化。

## 使用方法 ##
### 编译 ###
此工程中不包含libfuse的源码，需编译好libfuse.a放到lib目录中。编译直接make即可,编译出的库为lib/libusfs.a，使用时需链接libfuse库，参考main函数例子。

> sw@t3swing:usfs$ make clean </br>
> sw@t3swing:usfs$ make

交叉编译
> sw@t3swing:usfs$ make clean</br>
> sw@t3swing:usfs$ make host=arm-linux</br>

### 测试程序 ###
```c
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usfs.h"
#include "deps.h"

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

    usfs_printf("This is a dir! str:%s\n", str1);
}

void cfg_cb(void * arg, int size)
{
    usfs_add_cfg_int(0, "tabRow", &tab_row);
    usfs_add_cfg_int(0, "loop", &loop);
    usfs_add_cfg_str(0, "str", str, sizeof(str));
}

int main()
{
    int ret = 0;

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

    return 0;
}

```
以上的测试程序输出如下：
> sw@t3swing:hfs$ ls </br>
> cfg  dir  general  table </br>
> sw@t3swing:hfs$ cat cfg </br>
> tabRow = 10  </br>
> loop = 20  </br>
> str = hello </br> 
> sw@t3swing:hfs$ cat table </br>
> 
> -----A TABLE TEST----------------------------------------------- </br>
>  RowId  Col1   Col2     Col3 Col4   Col5
>      0      0      0       0 0      0   
>      1      2      3       5 7      9   
>      2      4      6      10 14     18  
>      3      6      9      15 21     27  
>      4      8     12      20 28     36  
>      5     10     15      25 35     45  
>      6     12     18      30 42     54  
>      7     14     21      35 49     63  
>      8     16     24      40 56     72  
>      9     18     27      45 63     81  
> sw@t3swing:hfs$ cat dir/dir  </br>
> This is a dir! str:hello </br>
> sw@t3swing:hfs$ echo tabRow=5 > cfg </br>
> sw@t3swing:hfs$ cat cfg  </br>
> tabRow = 5  </br>
> loop = 20  </br>
> str = hello  </br>
> sw@t3swing:hfs$ cat table  </br>
> 
> -----A TABLE TEST----------------------------------------------- </br>
>  RowId  Col1   Col2     Col3 Col4   Col5
>      0      0      0       0 0      0   
>      1      2      3       5 7      9   
>      2      4      6      10 14     18  
>      3      6      9      15 21     27  
>      4      8     12      20 28     36  
> sw@t3swing:hfs$ 

测试程序实现的功能为：
* 创建2个状态文件、1个配置文件、一个目录，目录里面再创建一个状态文件。
* 配置文件有3个配置项，每个状态文件会用到这个配置项。
* 可以通过改变配置的值来改变状态信息的值

状态文件的回调函数每次cat的时候（实际是文件open操作时）调用，配置文件创建的时候会调用一次回调函数，每次echo的时候会修改配置项。 </br>
状态信息输出table样式时，每次调用usfs_ftab_add都会添加一列，需注意usfs_ftab_end的位置（在循环里调用），列宽度由usfs_ftab_add的name参数宽度决定。

## API用法介绍 ##
### 初始化、去初始化 ###
```c
/**
 * create and destroy user stat file system
 *
 * @root_dir: the path fuse will mounte on
 * @return:  success , <0 failed
 */
int usfs_create(char * root_dir);
int usfs_destroy();
```
初始化与去初始化，初始化需要指定挂载点，即root_dir根目录（使用"/"表示），后面的接口都是此路径的相对路径。 </br>
**<font color=#0000ff>注意</font>**：root_dir指定的一定是目录，一定得存在，且有读写执行权限。

### 创建、删除目录 ###
```c
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
```
创建删除目录，path为相对root_dir的目录，如想在root_dir下创建目录，此时path填"/"即可。目录创建需一级一级创建，如上一级不存在，下一级会创建不成功。删除目录的时候，force选项为真时表明不管该目录下是否有文件或者目录，直接删除，有点类似rm -rf作用，否则force为假时智能删空目录。

### 创建、删除状态文件 ###
```c
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
```
状态文件创建时会关联一个回调函数，此回调函数可传入两个参数。当用户cat某个状态文件，就会调用该回调函数，此回调函数里可以使用如下打印函数进行输出：
```c
/**
 * a basic output fuction in usfs,just like printf, you should use it in usfs_create_sfile fuction's usfs_cb
 *
 * @format: like printf
 * @return: 0 success , <0 failed
 */
int usfs_printf(char * format, ...) FORMAT_CHECK(1, 2);

/**
 * fomat style Table, have header,can add whith colounm
 */
int usfs_ftab_start(char * format, ...) FORMAT_CHECK(1, 2);
int usfs_ftab_add(char * name, char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftab_addl(char * name, char * format, ...) FORMAT_CHECK(2, 3);
int usfs_ftab_end();
```
接口usfs_printf与printf用法一致，只不过输出的地方不一样罢了。ftab指的是表格样式输出，是对usfs_printf进一步封装，通过此系列函数，非常容易输出一个表格样式的状态信息。使用usfs_ftab_add函数输出的列是右对齐的，而usfs_ftab_addl输出的列是左对齐的。

### 创建、删除配置文件 ###
```c
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
```
这两个接口用来创建和删除配置文件的，虽然也是usfs_cb回调函数，但与usfs_create_sfile不一样，里面调用usfs_printf等函数没有任何作用，需调用如下函数：
```c
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
```
这两个函数用来为配置文件添加配置信息，除了str类型外，只支持int型，其他类型用str类型转吧。hide这个参数用来隐藏配置项的，设为TRUE时配置项虽然被隐藏(cat 配置文件，不会被显示出来)，但实际是可以通过echo命令进行修改的，有啥用？可以理解为高级功能，也可以理解为后门，反正不是本人原创的。 </br>
配置文件创建完成后，可以通过echo命令来设置配置项。如echo "cfgname = 1" > /usfs/cfg 命令，会把cfgname的值改为1，echo时使用重定向符'>'与使用追加符'>>'效果是一样的。

**<font color=#0000ff>注意</font>**：配置文件usfs_create_cfile的回调函数，只在usfs_create_cfile调用时执行一次，而不像状态文件usfs_create_sfile的回调函数每次cat都会调用。虽然两者回调函数形式一样，但调用时机及使用的函数都是不一样的。

