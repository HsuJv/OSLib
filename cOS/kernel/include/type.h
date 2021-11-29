#ifndef _TYPE_INCLUDED
#define _TYPE_INCLUDED

#ifndef __WORDSIZE
#define __WORDSIZE 64
#endif


#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef INTS
#define INTS
typedef char int8;
typedef unsigned char uint8;
typedef uint8 byte;

typedef short int16;
typedef unsigned short uint16;

typedef int int32;
typedef unsigned int uint32;

typedef long long int64;
typedef unsigned long long uint64;
#endif

#ifndef BOOLS
#define BOOLS
typedef int8 bool;

#define true 1
#define false 0
#endif

/* Types for `void *' pointers.  */
#if __WORDSIZE == 64
#ifndef __intptr_defined
typedef int64 intptr;
#define __intptr_defined
#endif
typedef uint64 uintptr;
#else
#ifndef __intptr_defined
typedef int32 intptr;
#define __intptr_defined
#endif
typedef uint32 uintptr;
#endif

#endif /* _TYPE_INCLUDED */
