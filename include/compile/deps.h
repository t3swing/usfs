#ifndef __DEPS_H__
#define __DEPS_H__

#include <stdio.h>

//#define DEBUG

#define SIZE_1K 1024
#define SIZE_4k (4*1024)
#define SIZE_1M (1024*1024)

typedef unsigned long int   ADDR_T;

typedef enum
{
    FALSE_E = 0,
    TRUE_E = 1,
} BOOL_E;

#define MIN(a, b)      ((a) < (b) ? (a) : (b))

/* o=open l=left r=right c=close,define open closed interval */
#define ISOIN(v, l, r)  ((v) > (l) && (v) < (r))
#define ISLIN(v, l, r)  ((v) >= (l) && (v) < (r))
#define ISRIN(v, l, r)  ((v) > (l) && (v) <= (r))
#define ISCIN(v, l, r)  ((v) >= (l) && (v) <= (r))

#ifdef DEBUG
#define DBG_PRT(str, args...)   fprintf(stdout,str,##args)
#define DBG_INFO(str, args...)  fprintf(stdout,"%s@%s:%d# "str, __FILE__, __FUNCTION__, __LINE__, ##args)
#else
#define DBG_PRT(str, args...)
#define DBG_INFO(str, args...)
#endif
#define DBG_ERR(str, args...)  fprintf(stderr,"%s@%s:%d# "str, __FILE__, __FUNCTION__, __LINE__, ##args)

/* check condition,if true return/goto with Messge */
#define CHECK_RTN(cond,err) if(cond){return (err);}
#define CHECK_GOTO(cond,lable) if(cond){goto lable;}
#define CHECK_MSG(cond,str,args...) if(cond){DBG_ERR(str,##args);}
#define CHECK_RTNM(cond,err,str,args...) if(cond){DBG_ERR(str,##args); return (err);}
#define CHECK_RTNVM(cond,str,args...) if(cond){DBG_ERR(str,##args); return;}
#define CHECK_GOTOM(cond,lable,str,args...) if(cond){DBG_ERR(str,##args); goto lable;}

#endif /* __DEPS_H__ */
