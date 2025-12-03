#ifndef __UTIL_H
#define __UTIL_H

#define IGNORE_VALUE  (void)
#define INVALID_INDEX ((size_t) -1)

#define CONSTCSTR(S) 		((const char*) (S))
#define MUTCSTR(S) 			((char*) (S))
#define DISGCONST(P)		((void*) (P))
#define ABORT(MSG) assert(0 && MSG)

#ifndef loop
#	define	loop(I,N) for(size_t I = 0; I < (N); I++)
#endif

#ifndef MIN
#	define MIN(A,B) ((A) > (B)) ? (B) : (A)
#endif

#ifndef MAX
#	define MAX(A,B) ((A) < (B)) ? (B) : (A)
#endif

#endif //__UTIL_H
