#ifndef __DT_HC_PLUG_H
#define __DT_HC_PLUG_H

/*  LIBC    */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <errno.h>

#define HC_DA_MACRO_BASED
#include "hc.h"

#define IGNORE_VALUE (void)

#define min                 hc_min
#define max                 hc_max
#define da_append           hc_da_append
#define da_free             hc_da_free
#define arrlen(A)           hc_arrlen
#define str_fmt(s)          (int)(s).len,(s).ptr
#define sb_fmt(sb)          (int)sb.count, sb.items
#define tkn_fmt(s)          (int)(s.length), (s.data)
#define str_multiline(...)  #__VA_ARGS__

#define debug(...) fprintf(stderr, "DEBUG: "__VA_ARGS__)
#define inst(...) (Instruction) {__VA_ARGS__}


#define UNREACHABLE(...) do {\
    fprintf(stderr,"UNREACHABLE at [%s:%s:%d]: ",__FILE__,__func__,__LINE__); \
    fprintf(stderr,__VA_ARGS__); \
    fprintf(stderr,"\n"); \
    abort();\
} while(0)


#endif//__DT_HC_PLUG_H
