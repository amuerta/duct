
/*
   ARENA
*/

// 
// Simple implementation of arena allocator build on top of the posix malloc
// you can replace malloc by a system specific memory allocator, like linuxe's mmap
// or window's get..memory..something..idk i dont use windows.
//


#ifndef __ARENA_H
#define __ARENA_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


// customize block size to your desire.
#ifndef ARENA_NODE_SIZE // 2048, (4096*1024) - linux max page size
#   define ARENA_NODE_SIZE 4096 // 4kb is default linux page size. 
#endif

#define ARENA_HEADER_SIZE sizeof(ArenaNode)


typedef unsigned char arena_bitmask8;
enum {
    ARENA_RESET_SIZE   = (1 << 0),
    ARENA_RESET_MEMORY = (1 << 1),
    ARENA_FREE_NODES   = (1 << 2),
};

typedef struct ArenaNode {
    size_t              allocated;
    struct ArenaNode*   next;
    char                data[];
} ArenaNode;

typedef struct {
    size_t      totally_allocated;
    ArenaNode*  memory;
} Arena;


// use these
void*       arena_alloc     (Arena*, size_t);
void        arena_memcpy    (Arena* a, void* data, size_t size);
void        arena_reset     (Arena*, int opt);
#define     arena_put(A, I) arena_memcpy(A, &I, sizeof(I))
#define     arena_clear(A)  arena_reset(A, ARENA_RESET_SIZE | ARENA_RESET_MEMORY)
#define     arena_rewind(A) arena_reset(A, ARENA_RESET_SIZE)
#define     arena_free(A)   arena_reset(A, ARENA_RESET_SIZE | ARENA_RESET_MEMORY | ARENA_FREE_NODES)


// change this one for your specific need/enviorment/taste
ArenaNode* arena_make_node(void);



ArenaNode* arena_make_node(void) {
    return calloc(ARENA_NODE_SIZE, 1);
}

void* arena_alloc(Arena* a, size_t size) {
    assert(ARENA_NODE_SIZE > size);
    void* ret = 0;
    ArenaNode* tail = a->memory;
    
    if (!a->memory) {
        a->memory = arena_make_node();
        tail = a->memory;
        goto arena_alloc_goto;
    }
    while(tail->next) tail = tail->next;

arena_alloc_goto:
    if (tail->allocated + size < (ARENA_NODE_SIZE - ARENA_HEADER_SIZE)) {
        ret = tail->data + tail->allocated;
        tail->allocated += size;
    } else {
        tail->next = arena_make_node();
        tail = tail->next;
        goto arena_alloc_goto;
    }

    return ret;
}




void arena_reset(Arena* a, int opt) {
    a->totally_allocated = 0;
    ArenaNode* node = a->memory;
    
    while(node) {
        ArenaNode* to_free = node;
        if (opt & ARENA_RESET_SIZE)
            node->allocated = 0;
        if (opt & ARENA_RESET_MEMORY)
            memset(node->data, 0, ARENA_NODE_SIZE - ARENA_HEADER_SIZE);

        node = node->next;
        if (opt & ARENA_FREE_NODES) 
            free(to_free);
    }
}

void arena_memcpy(Arena* a, void* data, size_t size) {
    void* cell = arena_alloc(a, size);
    assert(cell && "Unexprected null, failed to allocate");
    memcpy(cell, data, size);
}


#endif//__ARENA_H
//
// ARGS - argument parsing
//


// TODO: add flags that don't need '--'
// TODO: add support '--flag=<VALUE>'


#ifndef __HCH_ARGS_H
#define __HCH_ARGS_H

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  
#include <stdbool.h> 

typedef struct {
    char**          items;
    int             count;
} ArgsSlice;

const char* arg_str_is_flag(const char* str) {
    const char* flag = str;
    bool valid_flag = flag && strlen(flag) >= 2;
    if (!valid_flag)  return NULL;
    if (*flag == '-') flag++; else return NULL;
    if (*flag == '-') flag++;
    return flag;
}

// TODO: make this function accept multiple args, 
int arg_flag(ArgsSlice args, const char* flag) {
    assert(flag);
    for(int i = 0; i < args.count; i++) {
        const char* s = args.items[i];
        if (( s = arg_str_is_flag(s))) 
            if (strcmp(s, flag)==0) 
                return i;
    }
    return 0;
}

int arg_list(ArgsSlice args, const char* flag, ArgsSlice* out) {
    int b = 0;
    if((b = arg_flag(args,flag))) {
        if (b+1 < args.count)   out->items = args.items + b+1;
        else                    out->items = 0;

        for(int i = b+1; i < args.count; i++) {
            if(arg_str_is_flag(args.items[i])) break;
                out->count++;
        }
    }
    return b;
}

#endif//__HCH_ARGS_H
/*
    DYNAMIC ARRAY
*/

#ifndef __HC_DA_H
#define __HC_DA_H

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  

// This dynamic array is a trick i have seen @Tsoding use,
// and its (probably) the simplest way of doing dynamic array i used.
// 
// It's sort of robust and convinient to use.
// HOWEVER it's just a macro, which can be an issue when trying to port 
// projects/libraries to other languages.
//
// My solution:
//  Have api that either calls macros, or actual functions.
//  You can change which one to use with simple toggle.
//  Behaviour remains the same in both cases.
//
//  This way you can use hc_da_append outside of C, 
//  its pretty dirty with `void*` but you can create interfaces
//  in pretty much any other language other than C, or use macros of any kind.

#ifndef HC_DA_ITEMS_NAME
#   define HC_DA_ITEMS_NAME       items
#endif

#ifndef HC_DA_GROW_FACTOR
#   define HC_DA_GROW_FACTOR 2
#endif

#ifndef HC_DA_START_CAPACITY
#   define HC_DA_START_CAPACITY 32
#endif


// Simpler implementation means DA
// doesn't use header to store array metadata
// this ones relies on all items being in strict order
// as in DaGeneric.

// generic "interface"
typedef struct {
    void* items;
    size_t count, capacity, typesize;
} DaGeneric;

#define hc_da_append_macro(DA, VAR) do { \
    if ((DA)->capacity == 0) {\
        (DA)->capacity = HC_DA_START_CAPACITY;\
        (DA)->HC_DA_ITEMS_NAME = calloc(HC_DA_START_CAPACITY,sizeof(*(DA)->HC_DA_ITEMS_NAME));\
    }\
    if ((DA)->count >= (DA)->capacity) {\
        (DA)->capacity *= HC_DA_GROW_FACTOR;\
        (DA)->HC_DA_ITEMS_NAME = realloc((DA)->HC_DA_ITEMS_NAME,sizeof(*(DA)->HC_DA_ITEMS_NAME) * (DA)->capacity);\
    }\
    (DA)->HC_DA_ITEMS_NAME[((DA)->count)++] = VAR;\
} while(0);

void hc_da_append_generic(void* hc_da_ptr, void* varptr, size_t size) {
    DaGeneric* da = hc_da_ptr;
    if (da->capacity == 0) {
        da->capacity = HC_DA_START_CAPACITY; 
        da->items = calloc(HC_DA_START_CAPACITY , size);
        da->typesize = size;
    }
    assert(size == da->typesize);
    if (da->count >= da->capacity) {
        da->capacity *= HC_DA_GROW_FACTOR;
        da->items = realloc(da->items,size * da->capacity);
    }
    memcpy(da->items + (da->count * size), varptr, size);
    da->count++;
}


// Header Based implementation
// More robust and portable

typedef struct {
    size_t count, capacity, typesize;
} __DaHead__;
#define DaData __DaHead__ __hc_da_head__;
#define DaHead DaData

void hc_da_append_headed_generic(
        void** items_ptr, 
        __DaHead__ *head, 
        void* item, size_t tsize) 
{
    #define items (*items_ptr)
    if (head->capacity == 0 || !items) {
        head->capacity = HC_DA_START_CAPACITY; 
        items = calloc(HC_DA_START_CAPACITY,
                sizeof(tsize));
        head->typesize = tsize;
    }
    assert(tsize == head->typesize);
    if (head->count >= head->capacity) {
        head->capacity *= HC_DA_GROW_FACTOR;
        items = realloc(items,tsize * head->capacity);
    }
    memcpy(items + (head->count * tsize), item, tsize);
    head->count++;
    #undef items
}

#define hc_da_headed_append(A, it) hc_da_append_headed_generic(\
        ((void**) (&((A)->HC_DA_ITEMS_NAME))),\
        &((A)->__hc_da_head__),\
        &it, sizeof(it))
#define hc_da_headed_count(arr)       ((arr).__hc_da_head__.count)
#define hc_da_headed_capacity(arr)    ((arr).__hc_da_head__.capacity)
#define hc_da_headed_get(arr)         ((arr).HC_DA_ITEMS_NAME)



//
// INTERFACE
//


#ifdef HC_DA_MACRO_BASED

#define hc_da_append(A, it)        hc_da_append_macro(A, it)
#define hc_da_capacity(A)          ((A).capacity)
#define hc_da_get(A)               ((A).HC_DA_ITEMS_NAME)
#define hc_da_count(A)             ((A).count)
#define hc_da_foreach(A, iter)     for(size_t iter = 0; iter < hc_da_count(A); iter++)
#define hc_da_free(A)              free(A.HC_DA_ITEMS_NAME);

#else

#define hc_da_append(A, it)        hc_da_headed_append(A, it)
#define hc_da_capacity(A)          hc_da_headed_capacity(A)
#define hc_da_get(A)               hc_da_headed_get(A)
#define hc_da_count(A)             hc_da_headed_count(A)
#define hc_da_foreach(A, iter)     for(size_t iter = 0; iter < hc_da_count(A); iter++)
#define hc_da_free(A)              free(A.HC_DA_ITEMS_NAME);

#endif

// END
#endif// __HC_DA_H
/*
    END OF DYNAMIC ARRAY
*/
//
//
//
#ifndef __LIST_H
#define __LIST_H

// remove useless warning 
// hide warning '-Wmissing-field-initializers'
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  

//
// Linked List (legacy)
//

#define hc_li_macro_append(L, I) do {\
    if(!(L)) {(L) = (I); (L)->tail = (L);}\
    else {\
        void* __list_item__ = (I);\
        (L)->tail->next = __list_item__;\
        (L)->tail = __list_item__;\
    }\
}while(0)

#define hc_li_macro_next(LI) ((LI)->next)
#define hc_li_macro_foreach(list, type, iterator) \
    for(type iterator = list; iterator; iterator = hc_li_next(iterator))


#define hc_li_macro_defer(LI, T, ...) do {\
   T* __next__ = (LI);\
   T* __prev__ = (LI);\
   while(__next__) {\
       __prev__ = __next__;\
       __next__ = __next__->next;\
       {__VA_ARGS__}\
       __prev__ = 0;\
   } (LI) = 0;\
} while(0)


// 
// LINK LIST HEADER BASED
//

typedef struct {
    size_t typesize;
    void *next, *prev, *tail;
} __ListData__;
#define ListHead ListData
#define ListData __ListData__ __head__;

#define hc_li_append_generic_macro(L, I) do {\
    if(!(L)) {(L) = (I); (L)->__head__.tail = (L);}\
    else {\
        size_t __head_offset__ = (void*)&((L)->__head__) - (void*)(L);\
        void* __list_item__ = (I);\
        void* __tail__ = (L)->__head__.tail;\
        /*Set prev item*/\
        (I)->__head__.prev = __tail__;\
        /*Set next item*/\
        ((__ListData__*)(__tail__ + __head_offset__))->next = __list_item__;\
        /*Set new tail*/\
        (L)->__head__.tail = __list_item__;\
    }\
}while(0)

void hc_li_append_generic_fn(
        void**listptr, 
        void* item, 
        size_t item_size, 
        void* list_header) 
{
    assert(listptr);
    assert(item);
    #define list (*listptr)
    __ListData__ *head = list_header;
    size_t offset = list_header - list;

    if (!list) { // pointer is null, the offset IS the list_header
        list = item;
        head = list + offset;
        head->tail = list;
        head->typesize = item_size;
    } 

    else {
        assert(head->typesize == item_size);
        head = list_header;
        __ListData__* tail_head = head->tail + offset;
        __ListData__* item_head = item + offset;
        // set previous
        void* tail = head->tail;
        item_head->prev = tail;
        // set next
        tail_head->next = item;
        head->tail = item;
    }

#undef list
}


#define hc_li_append_generic_wrap(list, item)\
        hc_li_append_generic_fn((void**)&(list),\
                item, sizeof(*item),\
                &((list)->__head__))

#define hc_li_generic_next(list) (((list)->__head__).next)
#define hc_li_generic_prev(list) (((list)->__head__).prev)

#define hc_li_generic_foreach(list, type, iterator) \
    for(type iterator = list; iterator; iterator = hc_li_next(iterator))

// TODOS

#define hc_li_defer_generic #error "TODO: implement defer generic"

//
// Interface
//

#ifdef HC_LI_MACRO_BASED

#define hc_li_append(list, item)\
        hc_li_macro_append(list, item)
#define hc_li_next(list) hc_li_macro_next(list)
#define hc_li_foreach(LI, T, I, ...) hc_li_macro_foreach(LI, T, I)
#define hc_li_defer(LI, T, ...) hc_li_macro_defer(LI, T, ...)

#else

#define hc_li_append(list, item)\
        hc_li_append_generic_wrap(list, item)
#define hc_li_next(list) hc_li_generic_next(list)
#define hc_li_foreach(LI, T, I) hc_li_generic_foreach(LI, T, I)
#define hc_li_defer(LI, T) hc_li_generic_defer()

#endif


// restore warning '-Wmissing-field-initializers'
#pragma GCC diagnostic pop
// END
#endif//__LINK_H
#ifndef __HCH_MAP_H
#define __HCH_MAP_H

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  
#include <stdbool.h> 
#include <stdint.h>


// This map is intent-ed to be initialized by you
// for convenience there are `hc_map_heap` functions
// that allocate map via libc calloc, 
// i may add custom allocator interface in future, but for now you can just edit
// the source.
//
// Map is more lose then da.h or link.h is, for convenience, probably.
// By default it should be easy enough to use, but you have to 
// know what each macro/function does, so that you don't make mistakes.
// Memory sanitizer is advised if you wont use any memory trackers like free lists.
//
// If the map is allocated on the heap, it sets a marker that 
//  hc_map_free, hc_map_resize_generic,
//  hc_map_append_or_get
// use to check if everything is fine,
// IF NOT, they assert.

#ifndef HC_MAP_DEFAULT_INIT_SIZE
#   define HC_MAP_DEFAULT_INIT_SIZE 2048
#endif

//
// "API"
//
//  But tbh, this is just a macro wrapper around actual generic logic,
//  you can make kind of interface for map and make it as safe as you want, 
//  so i don't see a point making map any more difficult.
//
//  On the other hand it's good idea to have a limited sized map,
//  since its way easier to work with and rarely if ever you will need 
//  dynamically growling map. Cause why would you??

#define hc_map_key_fmt(MS)  (int)(MS).count, (MS).items
#define hc_map_get_or_reserve(M, K) \
    hc_map_get_or_reserve_generic(&(M)->map_head,\
            (void**) (&(M)->items),\
            (sizeof(*((M)->items))),\
            K)

#define hc_map_get(M, K) \
    hc_map_get_generic(&(M)->map_head,\
            (void**) (&(M)->items),\
            (sizeof(*((M)->items))),\
            K)

#define hc_map_grow(M, NEW_SIZE) \
    hc_map_grow_generic(&(M)->map_head,\
            (void**) (&(M)->items),\
            (sizeof(*((M)->items))),\
            NEW_SIZE)

#define hc_map_free(M) \
    hc_map_heap_free(&(M)->map_head);\
    free((M)->items);

//
// TYPES
//
#ifndef HC_DEFINITION_TYPE_INDEX_T
#define HC_DEFINITION_TYPE_INDEX_T
    typedef size_t              index_t;
#endif//HC_DEFINITION_TYPE_INDEX_T

// Map uses slice instead of cstring
// for obvious reasons...
typedef struct {
    const char* items;
    size_t      count;
} MapKeySlice;

typedef struct {
    MapKeySlice*    keys;
    size_t          count, capacity, typesize;
    unsigned long (*hash1) (const char*, size_t);
    unsigned long (*hash2) (const char*, size_t);
    bool            heap_allocated; // if not assert on attempt to free
} MapHead;

// hash functions: 
// https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed#145633
unsigned long djb2    (const char* str, size_t size, char shift);
unsigned long fnv1a   (const char* data, size_t size);

// Key
MapKeySlice hc_map_slice(const char* str, size_t count);
MapKeySlice hc_map_key(const char* str);

// map
static inline bool  hc_map_key_is_ok   (long int index);
float               hc_map_load        (MapHead  m); // in range from 0 to 1
long int            hc_map_query       (MapHead  m, MapKeySlice string);
long int            hc_map_take     (MapHead* m, MapKeySlice string);
void                hc_map_clear       (MapHead* m);
void                hc_map_calloc      (MapHead* m, size_t size);
void*               hc_map_reserve_or_get(MapHead* head, void** data, size_t typesize, MapKeySlice key);

unsigned long djb2(const char* str, size_t size, char shift) {
    unsigned long h = 5381;
    for (size_t i = 0; i < size; i++) 
        h = ((h << shift) + h) + str[i];
    return h;
}

unsigned long fnv1a(const char* data, size_t size) {
    unsigned long h = 2166136261UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }
    return h;
}

unsigned long hash1   (const char* data, size_t size) {
    return fnv1a(data, size);
}
unsigned long hash2   (const char* data, size_t size) {
    return djb2(data, size, 33);
}


MapKeySlice hc_map_key(const char* str) {
    MapKeySlice s = {.items=str, .count=strlen(str)};
    return s;
}

MapKeySlice hc_map_slice(const char* str, size_t count) {
    MapKeySlice s = {.items=str, .count=count};
    return s;
}

static inline bool  hc_map_key_is_empty(MapKeySlice key) {
    return !key.items || key.count == 0;
}

static inline bool  hc_map_key_is_ok(long int index) {
    return index >= 0;
}


float hc_map_load(MapHead m) {
    assert(m.capacity);
    return (m.count == 0) ? 0 : (float)m.count/(float)m.capacity;
}

void hc_map_set_default_hashes(MapHead* m) {
    m->hash1 = hash1;
    m->hash2 = hash2;
}

void hc_map_calloc(MapHead* m, size_t cap) {
    if (cap == 0) m->capacity = HC_MAP_DEFAULT_INIT_SIZE;
    else m->capacity = cap;
    m->keys = calloc(m->capacity, sizeof(*m->keys));
    assert(m->keys && "CALLOC FAILED IN hc_map_calloc");
}

MapHead hc_map_heap(size_t cap, size_t typesize) {
    MapHead m = {0};
    hc_map_calloc(&m, cap);
    m.hash1 = hash1;
    m.hash2 = hash2;
    m.heap_allocated = true;
    m.typesize = typesize;
    return m;
}

void hc_map_clear(MapHead* m) {
    memset(m->keys, 0, sizeof(*(m->keys)) * m->capacity);
    m->count = 0;
    // m->capacity = 0;
}

void hc_map_heap_free(MapHead* m) {
    assert(m->heap_allocated && 
            "MAKE SURE YOU MARK HEAP ALLOCATED MAP OR NOT ATTMEPT TO FREE NON HEAP MEMORY MAP.");
    if(m->keys) free(m->keys);
    memset(m, 0, sizeof(*m));
}


long int hc_map_take(MapHead* m, MapKeySlice string) {
    assert(m->keys && "EXPECTED TO HAVE KEYS INITILIZED");
    unsigned long 
        h1 = m->hash1(string.items, string.count),
        h2,
        index = -1
    ;
    MapKeySlice key = m->keys[(index = (h1 % m->capacity))];
    const size_t cap = m->capacity;

    // collision
    if (key.items) {
        h2 = m->hash2(string.items, string.count);
        // we hit again
        if (m->keys[(index = (h1+h2)% m->capacity)].items) {
            // "otherwise - iterative approach"
            for(size_t i = 0; i < m->capacity; i++) {
                if (!m->keys[(index = ((h1+h2)+i)%cap)].items) 
                    goto end;
            }
            return -1;
        } 
    } 

end:
    m->count++;
    if ((long int)index >= 0) m->keys[index] = string;
    return (long int) index;

}

long int hc_map_query(MapHead m, MapKeySlice string) {
#define mks_eq(k,s) (strncmp(k.items, s.items, s.count) == 0)
    // check if initlized
    assert(m.keys && "EXPECTED TO HAVE KEYS INITILIZED");
    if(!m.capacity) return -1;
    assert(string.items && string.count);

    unsigned long
        h1 = m.hash1(string.items, string.count),
        h2,
        index = -1
    ;
    size_t len = string.count;

    // check 1
    size_t cap = m.capacity;
    MapKeySlice key = m.keys[(index = h1 % m.capacity)];
    if (key.items && key.count == len && mks_eq(key, string)) 
        return (long int)index;

    // check 2 (double hash)
    h2 = m.hash2(string.items, string.count);
    key = m.keys[(index = (h1+h2)% cap)]; 

    if (key.items && key.count == len && mks_eq(key, string))  
        return (long int)index;

    //  try linear lookup  
    for(size_t i = 0; i < m.capacity; i++) {
        key = m.keys[(index = ((h1+h2)+i)%cap)]; 
        if(!key.items) break; // if we have a gap, this means 
                              // desired key can't be found here since
                              // if it would exist, it would be inserted in 
                              // linear fassion with current key,
                              // gap indicated end of this key lookup sequence
                              // or buggy/corrputed behaviour of the map
        if(key.count != len) continue;
        if (mks_eq(key, string))  
            return (long int)index;
    }

    return -1;
#undef mks_eq
}


void* hc_map_get_or_reserve_generic(MapHead* head, void** data, size_t typesize, MapKeySlice key) {
    assert(typesize == head->typesize && "MISMATCH IN MAP ITEMS TYPE SIZE AND GIVEN ITEM");
    long int i = hc_map_query(*head, key);
    // doesn't exist
    if(i == -1) {
        i = hc_map_take(head, key);
        assert(i != -1 && "ATTEMPT TO APPEND TO FULL MAP, PERFORM CHECK FIRST");
    }
    void* item = (*data) + typesize * i;
    return item;
}


void* hc_map_get_generic(MapHead* head, void** data, size_t typesize, MapKeySlice key) {
    assert(typesize == head->typesize && "MISMATCH IN MAP ITEMS TYPE SIZE AND GIVEN ITEM");
    long int i = hc_map_query(*head, key);
    // doesn't exist
    if(i == -1) return NULL;
    void* item = (*data) + typesize * i;
    return item;
}

void hc_map_grow_generic(MapHead* head, void** data, size_t typesize, size_t new_size) {
    assert(head->heap_allocated && "CANNOT GROW NON HEAP ALLOCATED MAP");
    assert(data && "EXPECTED DATA PTR TO BE VALID");
    assert(typesize == head->typesize && "MISMATCH IN MAP ITEMS TYPE SIZE AND GIVEN ITEM");
    assert(new_size > head->capacity && "MAP CAN ONLY GROW with hc_map_grow_generic");

    MapHead      new_head  = hc_map_heap(new_size, typesize);
    void*        new_items = calloc(new_size*typesize,          1);
    
    for(size_t i = 0; i < head->capacity; i++) {
        MapKeySlice key = head->keys[i];
        if(hc_map_key_is_empty(key)) continue;
        // if item exists, rehash it into new head 
        // and move item to new_items

        void* item = (*data) + typesize*i;
        void* old_item = hc_map_get_or_reserve_generic
            (&new_head, &new_items, typesize, key);
        memcpy(old_item, item, typesize);
    }
    
    // set new head
    hc_map_heap_free(head); // this will fail if not heap
    memcpy(head, &new_head, sizeof(*head));
    // set new data
    free(*data);            // so this is ok, probably.
    *data = new_items;
}

#endif//__HCH_MAP_H
//
// POOL datastructure
//

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  

// TODO:? make it use not void* but user defined union/struct?

#ifndef __HCH_POOL_H
#define __HCH_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


// TODO: use inline __asm__(int3) to have a proper breakpoint 
// instead of this old funny hack
// cause segmentaion fault to be able to run gdb on breakpoint
#ifndef FAULT_TRIGGER
#   ifdef  HCH_ASSERT_NO_BREAKPOINT
#       define FAULT_TRIGGER // does nothing 
#   else
#       define FAULT_TRIGGER __asm__("int3")
#   endif
#endif

#ifndef hch_assert
#define hch_assert(COND,...) \
    do { if (!(COND)) { \
        fprintf(stderr,"Assertion at [%s:%s:%d]: ",__FILE__,__func__,__LINE__); \
        fprintf(stderr,__VA_ARGS__); \
        fprintf(stderr,"\n"); \
        FAULT_TRIGGER;      \
        exit(1);            \
    }} while(0)
#endif

#ifndef HC_DEFINITION_TYPE_INDEX_T
#define HC_DEFINITION_TYPE_INDEX_T
    typedef size_t              index_t;
#endif//HC_DEFINITION_TYPE_INDEX_T

typedef unsigned char       pool_bitmask8;

#define INDEX_INVALID ((size_t)-1)

typedef enum {
	PoolState_allocated = 1,
} PoolState;

typedef struct {
    size_t      typesize;
    char*       type;

    void*       data;
    index_t*      free_indexes;
    

    void        (*destructor) (void*);
    size_t      capacity;
    size_t      count;
    size_t      free_count;
    size_t      max_size;
} Pool;

#ifndef POOL_MALLOC
#   define POOL_MALLOC(S) malloc(S)
#endif

#ifndef POOL_FREE
#   define POOL_FREE(P) free(P)
#endif

#ifndef POOL_REALLOC
#   define POOL_REALLOC(P,S) realloc(P,S)
#endif


#ifndef IGNORE_RETURN
#   define IGNORE_RETURN (void)
#endif

#define __POOL_typestring(T) #T

#ifndef POOL_DEFAULT_CAPACITY
#   define POOL_DEFAULT_CAPACITY 32
#endif

#define POOL_ITEM_POINTER(p,index) \
    p->data + index * (p->typesize + sizeof(pool_bitmask8));



//      //
/* API  */
//      //

#define     pool_new(T)                         pool__init(NULL, POOL_DEFAULT_CAPACITY, sizeof(T), __POOL_typestring(T))
#define     pool_init(P,T,S)                    IGNORE_RETURN pool__init(P, S, sizeof(T), __POOL_typestring(T))
void        pool_resize(Pool* p, size_t newsize);
index_t     pool_reserve(Pool* p);
void        pool_release(Pool* p, index_t i);
void*       pool_refer(Pool* p, index_t i);


Pool pool__init(Pool* self, size_t capacity, size_t typesize, char* type) {
    const size_t data_sz_bytes = capacity * ( sizeof(pool_bitmask8) + typesize );
    const size_t idxs_sz_bytes = capacity * sizeof(index_t);

    Pool new = {
        .type = type,
        .typesize = typesize,
        .capacity = capacity,
        .count = 0,
        .free_count = 0,
        .max_size = 0,

        // alloc memory,
        .data = POOL_MALLOC(data_sz_bytes),
        .free_indexes = POOL_MALLOC(idxs_sz_bytes),
    };

    if (!self) {
        return new;
    } 

    memcpy(self, &new, sizeof(new));
    return *self;
}

void pool_free(Pool* p) {
    free(p->data);
    free(p->free_indexes);
    memset(p,0,sizeof(*p));
}

void* pool_refer(Pool* p, index_t i) {
    hch_assert(p, "Expected to have valid pointer got NULL");
    hch_assert(p->data, "Expected to have valid data pointer initilized");
    void* ptr = POOL_ITEM_POINTER(p,i);
    pool_bitmask8 state = *((pool_bitmask8*)ptr);
    return (state) ? ptr : NULL;
}

void pool_resize(Pool* p, size_t newsize) {
    const size_t newsize_bytes = newsize * (sizeof(pool_bitmask8) + p->typesize);
    const size_t newsize_indexes_bytes = newsize * sizeof(index_t);

    if (p->capacity == newsize) 
        return;
    else if (p->capacity > newsize) {
        // TODO:
        // impl destructor
    } 
 
    p->capacity     = newsize;
    p->data         = POOL_REALLOC(p->data,         newsize_bytes           );
    p->free_indexes = POOL_REALLOC(p->free_indexes, newsize_indexes_bytes   );
}

#define pool_append(P, VAR) \
    pool__append(P, &(VAR), sizeof(VAR))

index_t pool__append(Pool* p, void* data, size_t typesize) {
    index_t i = pool_reserve(p);
    if (i == INDEX_INVALID) 
        return INDEX_INVALID;

    hch_assert(typesize == p->typesize, 
            "Expected to have a type that equal or less than pool typesize");
    memcpy(pool_refer(p,i),data,typesize);
    return i;
}

index_t pool_reserve(Pool* p) {
    index_t index = INDEX_INVALID;
    if (p->free_count > 0) {
        index = p->free_indexes[p->free_count-1];
        p->free_count--;
    } else {
        index = p->count;
    }
    //debug("Reserved id: %u\n",ptr);
    hch_assert(index != INDEX_INVALID, "Failed to reserve entity");
    hch_assert(p->count < p->capacity, "Attempt to buffer overflow");

    pool_bitmask8* state = POOL_ITEM_POINTER(p, index);
    *state |= PoolState_allocated;

    p->count++;
    if (p->count > p->max_size) {
        p->max_size = p->count;
    }
    return index;
}

void  pool_release(Pool* p, index_t i) {
    if (p->count==0)
        return;

    hch_assert(i < p->capacity, "Attempt to access Out of Bounds");
    pool_bitmask8* state = POOL_ITEM_POINTER(p,i);

    if (!(*state & PoolState_allocated))
        return;
 
    p->free_indexes[p->free_count] = i;
    p->free_count++;

    *state ^= PoolState_allocated;
    p->count--;
}

#endif //__HCH_POOL_H
//
// PRELUDE
//
/* stuff like:
 *  - types
 *  - common utility macros
 *  - bitmask handling
 *  - temporary allocator
 *
*/
#ifndef __HCH_PRELUDE_H
#define __HCH_PRELUDE_H


#include <stdio.h>   
#include <time.h>   
#include <string.h>  
#include <assert.h>  
#include <stdbool.h> 
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

typedef int8_t              i8;
typedef int16_t             i16;
typedef int32_t             i32;
typedef int64_t             i64;
typedef intmax_t            isize;

typedef int8_t              s8;
typedef int16_t             s16;
typedef int32_t             s32;
typedef int64_t             s64;
typedef intmax_t            ssize;

typedef uint8_t             u8;
typedef uint16_t            u16;
typedef uint32_t            u32;
typedef uint64_t            u64;
typedef size_t              usize;

typedef float               f32;
typedef double              f64;
                              
typedef const char*         istr;
typedef char*               mstr;

#ifndef HC_DEFINITION_TYPE_INDEX_T
#define HC_DEFINITION_TYPE_INDEX_T
    typedef size_t              index_t;
    typedef intmax_t            sindex_t;
#endif//HC_DEFINITION_TYPE_INDEX_T


#ifndef HC_DEFINITION_TYPE_BITMASK_T
#define HC_DEFINITION_TYPE_BITMASK_T
    typedef unsigned char       bitmask8;
    typedef unsigned short      bitmask16;
    typedef unsigned int        bitmask32;
    typedef uint64_t            bitmask64;
#endif //HC_DEFINITION_TYPE_BITMASK_T


typedef int64_t             stime;
typedef uint64_t            utime;


//
// MACROS
//

# define hc_max(A,B)            (A > B) ? A : B
# define hc_min(A,B)            (A < B) ? A : B
# define hc_loop(I,N)           for(size_t I = 0; I < (N); I++)
# define hc_loopt(TI,N)         for(TI = 0; I < (N); I++)
# define hc_range(n, min, max)  ((n)>=(min) && (n)<=(max))
# define hc_clamp(n, min, max)  \
     ((n) < (min)) ? (min) : ((n) > (max) ? (max) : (n)) 

# define hc_arrlen(a)           (sizeof(a)/sizeof(a[0]))
# define hc_cast(v, T)          ((T)v)
# define hc_transmute(v, T)     *((T*)&(v))
# define hc_zeroed(v)           memset(&(v), 0, sizeof(v))
# define hc_unused(v)           ((void) (v))
# define hc_roptr(v)            ((const void*) v)
# define hc_cmp(l,r)            (memcmp(&(l),&(r),hc_min(sizeof(l),sizeof(r)))==0)
# define hc_BREAKPOINT()        __asm__("int3")

#ifdef HCH_STRIP_MACRO_PREFIX
# define arrlen(a)       hc_arrlen(a) 
# define cast(v, T)      hc_cast(v, T)     
# define transmute(v, T) hc_transmute(v, T)
# define zeroed(v)       hc_zeroed(v)      
# define unused(v)       hc_unused(v)      
# define roptr(v)        hc_roptr(v)       
# define cmp(l,r)        hc_cmp(l,r)       
# define BREAKPOINT()    hc_BREAKPOINT()   

# define max(A,B)           hc_max(A,B)          
# define min(A,B)           hc_min(A,B)          
# define loop(I,N)          hc_loop(I,N)         
# define loopt(TI,N)        hc_loopt(TI,N)       
# define range(n, min, max) hc_range(n, min, max)
# define clamp(n, min, max) hc_clamp(n, min, max)
#endif//HCH_STRIP_PREFIX

//
// bitmasking
//

#ifndef HCH_FULL_BITMASK_PREFIX
#   define bit_check(N, M)               ((N) & (M))
#   define bit_toggle(N, M)              ((N) ^ (M))
#   define bit_set(N, M)                 ((N) | (M))
#   define bit_clear(N, M)               ((N) & (~(M)))
#   define bit_get_chunk(m, off, size)   __bm_get_chunk((m),(off),(sz))
#else
#   define bitmask_toggle(N, M)             ((N) ^ (M))
#   define bitmask_set(N, M)                ((N) | (M))
#   define bitmask_clear(N, M)              ((N) & (~(M)))
#   define bitmask_get_chunk(m, off, size)  __bm_get_chunk((m),(off),(sz))
#endif

u64 __bm_get_chunk(u64 mask, u8 offset, u8 size) {
    int select_mask = 0;
    for(int i = 0; i < size; i++) select_mask |= (1 << i);
    return (mask >> offset) & select_mask;
}


//
// allocation utilities
//

void* recalloc(void* ptr, size_t prev_size, size_t size) {
    void* new_ptr = calloc(size, 1);
    if(ptr) {
        memcpy(new_ptr, ptr, prev_size);
        free(ptr);
    }
    return new_ptr;
}

//
// temporary allocator
//
#ifndef TEMP_ALLOCATOR_SIZE // 4 megs
#   define TEMP_ALLOCATOR_SIZE 1024 * 1000 * 4
#endif

static unsigned int  __temp_allocator_current__ ;
static unsigned char __temp_allocator_buffer__  [TEMP_ALLOCATOR_SIZE];

void* temp_alloc(size_t size);
void* temp_put_sized(void* item, size_t size);
void* temp_string(const char* string);


void* temp_alloc(size_t size) {
    assert(size < TEMP_ALLOCATOR_SIZE);
    // reset if can't fit
    if (__temp_allocator_current__ + size > TEMP_ALLOCATOR_SIZE) 
        __temp_allocator_current__ = 0;
    void* mem = __temp_allocator_buffer__ +
                __temp_allocator_current__;
    __temp_allocator_current__ += size;
    return mem;
}

void* temp_put_sized(void* item, size_t size) {
    void* mem = temp_alloc(size);
    memcpy(mem, item, size);
    return mem;
}

void* temp_string(const char* string) {
    return temp_put_sized((void*)string, strlen(string) + 1);
}

//
// profiler
//

/* Just an array into which you log your profiling
 * results, index into array is your enum, time saved in nanoseconds.
 */


#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 199901L
    /* C11 or newer */

#ifndef PROFILER_ENTRY_CAPACITY // i believe 512 timer entries is more than enough
#   define PROFILER_ENTRY_CAPACITY 512
#endif

#define MICROSECOND 1000 
#define MILLISECOND 1000*1000 
#define SECOND      1000*1000*1000 

typedef struct {
    bool  finished;
    utime result;
    utime begin;
} __profiler_table_entry__;
static __profiler_table_entry__ 
    __PROFILER_TABLE__ 
        [PROFILER_ENTRY_CAPACITY];

void profiler_begin     (unsigned int entry_id);
void profiler_end       (unsigned int entry_id);
utime profiler_get_ns   (unsigned int entry_id);
utime profiler_get_ms   (unsigned int entry_id);
double profiler_get_sec (unsigned int entry_id);

void profiler_begin(unsigned int entry_id) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(entry_id < PROFILER_ENTRY_CAPACITY);
    __PROFILER_TABLE__[entry_id].finished = false;
    __PROFILER_TABLE__[entry_id].begin =  
        (utime)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void profiler_end(unsigned int entry_id) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(entry_id < PROFILER_ENTRY_CAPACITY);
    utime  after = (utime)
        ts.tv_sec * 1000000000LL + ts.tv_nsec;
    utime before = __PROFILER_TABLE__[entry_id].begin;
    utime time_ns = after-before;
    __PROFILER_TABLE__[entry_id].finished = true;
    __PROFILER_TABLE__[entry_id].result = time_ns;
}


double profiler_get_sec(unsigned int entry_id) {
    double time = (double)profiler_get_ns(entry_id)/((double)SECOND);
    return time;
}

utime profiler_get_ms(unsigned int entry_id) {
    utime time = profiler_get_ns(entry_id)/(MILLISECOND);
    return time;
}

utime profiler_get_ns(unsigned int entry_id) {
    assert(__PROFILER_TABLE__[entry_id].finished && "attempt to access a unfinished timer!");
    utime time = __PROFILER_TABLE__[entry_id].result;
    return time;
}

#endif


// custom assert

#ifndef FAULT_TRIGGER
#   ifdef  HCH_ASSERT_NO_BREAKPOINT
#       define FAULT_TRIGGER // does nothing 
#   else
#       define FAULT_TRIGGER __asm__("int3")
#   endif
#endif

#ifndef hch_assert
#define hch_assert(COND,...) \
    do { if (!(COND)) { \
        fprintf(stderr,"Assertion at [%s:%s:%d]: ",__FILE__,__func__,__LINE__); \
        fprintf(stderr,__VA_ARGS__); \
        fprintf(stderr,"\n"); \
        FAULT_TRIGGER;      \
        exit(1);            \
    }} while(0)
#endif

#endif // __HCH_PRELUDE_H
/*
   String Builder (Nob style)
*/

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  
#include <stdarg.h>
#include <stdbool.h>

#ifndef __HCH_SB_H
#define __HCH_SB_H

typedef struct {
    // transmutable -> DA , String
    char*  items;
    size_t count, capacity;

    const char* spacer;
} StringBuilder;

#define sb_arrlit(...)          ((const char*[]) {__VA_ARGS__})
#define sb_arrlen(arr)          (sizeof(arr) / sizeof((arr)[0]))
#define sb_arrlit_len(...)      (sb_arrlen((__VA_ARGS__)))

#define sb_min(a,b) ((a) > (b))? (b) : (a)
#define sb_max(a,b) ((a) < (b))? (b) : (a)

#ifndef __HCH_PRELUDE_H
void* recalloc(void* ptr, size_t prev_size, size_t size) {
    void* new_ptr = calloc(size, 1);
    if(ptr) {
        memcpy(new_ptr, ptr, prev_size);
        free(ptr);
    }
    return new_ptr;
}
#endif

bool sb_is_empty(StringBuilder sb) {
    return !sb.items || !sb.items;
}

void sb__append(StringBuilder* sb, const char** items, size_t count) {
    if (!count) return;
    size_t append_size = 0;
    size_t spacer_size = 0;

    if (sb->spacer) spacer_size = strlen(sb->spacer); 

    for(size_t i = 0; i < count; i++) {
        if (!items[i]) continue;
        append_size += strlen(items[i]);
        if (i != count - 1 || i) 
            append_size += spacer_size;
    }
    // null terminator
    if(append_size) append_size++;

    if (!sb->items || !sb->capacity) {
        sb->capacity = 32;
        sb->items = calloc(32, 1);
    }
    if (sb->count + append_size >= sb->capacity) {
        size_t new_size = sb_max(sb->capacity*2, sb->capacity + append_size);
        sb->items = recalloc(sb->items, sb->capacity, new_size);
        sb->capacity = new_size;
    }

    for(size_t i = 0; i < count; i++)  {
        if (!items[i]) continue;
        size_t len = strlen(items[i]);
        if(items[i]) {
            memcpy(sb->items + sb->count, items[i], len);
            sb->count += len;
            
            if (spacer_size && (i != count-1)) {
                memcpy(sb->items + sb->count, sb->spacer, spacer_size);
                sb->count += spacer_size;
            }

        }
    }
}

void sb_reverse(StringBuilder* s) {
	if (sb_is_empty(*s))
		return;

	char* ptr_cpy = calloc(s->count, sizeof(char));
	if (!ptr_cpy) assert(false && "Failed to allocate" "memory with calloc(n,s)"); 
    memcpy(ptr_cpy, s->items, s->count);
	// [ h i ! ] : len 3
	//   i i i
	//   0 1 2
	//     ^ ^
	//	   | (end) = (len - 1)
	//	   |
	//	   +-> (end) - i

	for(size_t i = 0; i < s->count; i++) {
		size_t reverse = (s->count-1) - i;
		s->items[i] = ptr_cpy[reverse];
	}
	free(ptr_cpy);
}

void sb_appendf(StringBuilder* sb, const char* fmt, ...) {
    va_list args, args_len;
    va_start(args, fmt);
    va_copy(args_len, args);
    size_t size = vsnprintf(0,0,fmt,args_len);
    va_end(args_len);

    const char* temp = calloc(size+1,1);
    vsnprintf((char*)temp, size+1, fmt, args);
    sb__append(sb, &temp, 1);
    free((void*)temp);
    
    va_end(args);
}

void sb_clear(StringBuilder* sb) {
    memset(sb->items, 0, sb->count);
    sb->count = 0;
}

#define sb_append(sb, ...) \
    sb__append(\
            sb,\
            sb_arrlit(__VA_ARGS__),\
            sb_arrlen(sb_arrlit(__VA_ARGS__)))\

#endif//__HCH_SB_H
#ifndef __HCH_STRING_H
#define __HCH_STRING_H

#include <stdlib.h>  
#include <stdio.h>   
#include <string.h>  
#include <assert.h>  
#include <stdbool.h> 

#define STR_TEMP_SIZE 1024
#define STR_NOPATTERN -1

#ifndef INLINE
#define INLINE static inline
#endif

#define str__max(A,B) (A) > (B) ? (A) : (B)
#define str__min(A,B) (A) > (B) ? (B) : (A)
#define str__clamp(n, min, max) \
     ((n) < (min)) ? (min) : ((n) > (max) ? (max) : (n)) 
#define str_loop(I,N) for(size_t I = 0; I < (N); I++)

// Dynamic array - "da", can be
// transmuted into String directly
// (with the loss of capacity ofc)
typedef struct {
	const char* ptr;
	size_t 	len;
} String;

// TODO:
// Write access is avilable for all functions that return void
// by default ptr is read only but some functions may modify it 
// via forceful mutable cast
INLINE String 	str_make(const char* cstr); 
INLINE String 	str_make_sized(const char* cstr, size_t len); 
INLINE String 	str_dup(String orig);
INLINE bool 	str_is_empty(String s);
INLINE void 	str_reset(String* s);

String      str_trim(String s);
String      str_trim_left(String s);
String      str_trim_right(String s);
String      str_substr(String orig, size_t index, size_t len);
String      str_split_by_chars(String *s, const char* chars);
bool        str_begins_with(String src, String pat);
bool        str_ends_with(String src, String pat);
bool        str_cmp(String l, String r);
bool        str_cmp_cstr(String l, const char* str);
int         str_has_pattern(String src, String pat);
bool        str_is_integer(String s);
bool        str_is_float(String s);
#define     str_print(s) str_print_fmt(s,0,0)
void	    str_print_fmt(String s, char* prefix, char* postfix);
const char*	str_temp_cstr(String s);
	
//
// IMPLEMENTATION
//

INLINE String str_zero(void) {
    String s = {0};
    return s;
}

INLINE String str_make_sized(const char* cstr, size_t size) {
    String s = {
        .len = size,
        .ptr = cstr,
    };
    return s;
}

INLINE String str_make(const char* cstr) {
    return str_make_sized(cstr, strlen(cstr));
}

INLINE String str_dup(String s) {
	String dup = {0};
    memcpy(&dup, &s, sizeof(s));
	return dup;
}


String str_range(String orig, size_t begin, size_t end) {
    assert(begin == end && "can't have slice of size 0");
    int b = str__min(begin, end);
    int e = str__max(begin, end);
    b = str__clamp(b, 0, (int)orig.len - 1);
    e = str__clamp(e, 0, (int)orig.len - 1);

    String s = {
        .ptr = orig.ptr + begin,
        .len = end - begin,
    };

    return s;
}

String str_substr(String orig, size_t index, size_t len) {
	assert(index < orig.len && 
			"Index overflows original string");
	assert(len < orig.len && 
			"Sub string length cannot be greated than origin len");
	assert(index + len <= orig.len && 
			"Slice go out of original string bounds");


    String sub = {
        .ptr = orig.ptr + index,
        .len = len,
    };
	return sub;
}



bool str_begins_with(String src, String pat) {
	assert(src.len != 0 && pat.len != 0 && 
			"Source and pattern do not allow length of 0");
	assert(src.len > pat.len && 
			"Pattern string cannot be "
			"equal or bigger to Source!"
			"Check str_equal(String,String)"
			);

	bool equal = true;
	for (size_t i = 0; i < pat.len; i++)
		equal = equal && (src.ptr[i] == pat.ptr[i]);
	return equal;
}

bool str_ends_with(String src, String pat) {
	assert(src.len != 0 && pat.len != 0 && 
			"Source and pattern do not allow length of 0");
	assert(src.len > pat.len && 
			"Pattern string cannot be "
			"equal or bigger to Source!"
			"Check str_equal(String,String)"
			);

	bool equal = true;
	size_t offset = src.len - pat.len;
	for (size_t i = (pat.len - 1); i > 0; i--) {
		equal = equal && (src.ptr[i+offset] == pat.ptr[i]);
		
	}
	return equal;
}


bool str_cmp_cstr(String s, const char* str) {
    return (s.len == strlen(str)) && strncmp(s.ptr, str, s.len) == 0;
}

bool str_cmp(String l, String r) {
	bool equal = true;
	if(l.len == r.len) {
		for (size_t i = 0; i < l.len; i++)
			equal = equal && (l.ptr[i] == r.ptr[i]);
	} 
	else 
		return false;
	return equal;
}

bool str_is_empty(String s) {
	return (s.ptr == NULL || s.len == 0);
}

int str_has_pattern(String src, String pat) {
	assert(src.len != 0 && pat.len != 0 && 
			"Source and pattern do not allow length of 0");

	assert(src.len > pat.len && 
			"Pattern string cannot be "
			"equal or bigger to Source!"
			"Check str_are_equal(String,String)"
			);

	// src: [ h e l l o ! ] : len 6
	// pat: [ o ! ]  		: len 2
	
	//   0 1 2 3 4
	//   | | | | |
	//   v v v v v
	// [ h e l l o ! ]
	//         [ o ! ]
	//  last index to check is inclusive src.len - pat.len

	size_t diff = src.len - pat.len; 

	if (pat.len == 1){
		for(size_t i = 0; i < src.len; i++) {
			if (src.ptr[i]==pat.ptr[0])
				return true;
		}
	}
	else
		for(size_t src_i = 0; src_i <= diff; src_i++) {
			bool equal = true;

			for(size_t c = 0; c < pat.len; c++)
				equal = equal && (src.ptr[src_i+c]==pat.ptr[c]);

			if (equal)
				return src_i;
		}
	return STR_NOPATTERN;
}

bool str_is_integer(String s) {
	bool is_a_num = true;
    if (!s.len) return false;
	for(size_t i = 0; i < s.len; i++)
		is_a_num = is_a_num && (
			(s.ptr[i] >= '0' &&  s.ptr[i] <= '9')
			||	s.ptr[0] == '-'
		);
	return is_a_num;
}

bool str_is_float(String s) {
	bool is_a_num = true;
    if (!s.len) return false;
	for(size_t i = 0; i < s.len; i++)
		is_a_num = is_a_num && (
			(s.ptr[i] >= '0' &&  s.ptr[i] <= '9')
			||	s.ptr[0] == '-' 
			||	s.ptr[i] == '.'
		);
	return is_a_num;
}


char* str_create_cstr(String s) {
	char* temp = calloc( (s.len+1)	,	sizeof(char));
	for(size_t i = 0; i < s.len+1; i++)
		temp[i] = 0;
	memcpy(temp,s.ptr,s.len);
	return temp;
}

const char* str_temp_cstr(String s) {
	static char buffer[STR_TEMP_SIZE];
	memset(buffer,0,STR_TEMP_SIZE);
	size_t len = (s.len < STR_TEMP_SIZE - 1) ? 
		s.len : STR_TEMP_SIZE - 1;
	strncpy(buffer,s.ptr,len);
	return (const char*)buffer;
}

// returns left part of split, source gets reduced
// if first matched character is in chars, returns left as str_zero(void)
// when soucre is empty does nothing.
String str_split_by_chars(String *s, const char* chars) {
    String r = {0};
    size_t chars_len = strlen(chars);
    if (!s->len) return r;
    r.ptr = s->ptr;
    size_t len = s->len;
    for(size_t i = 0; i < len; i++) {
        for(size_t lc = 0; lc < chars_len; lc++) {
            const bool match = *(s->ptr) == chars[lc];
            if(match) {
                s->ptr++;
                s->len--;
                if(!i) { 
                    return str_zero();
                } 
                return r;
            }
        }
        s->ptr++;
        s->len--;
        r.len++;
    }
    s->len = 0;
    return r;
}

String str_trim_right(String s) {
    String r = {
        .ptr = s.ptr,
        .len = s.len
    };
    for(int i = s.len-1; i >= 0; i--) {
        bool is_space = 
            r.ptr[i] == '\n' ||
            r.ptr[i] == '\t' ||
            r.ptr[i] == '\r' ||
            r.ptr[i] == ' '  ;
        if (!is_space) return r;
        r.len--;
    }
    return r;
}

String str_trim_left(String s) {
    String r = {
        .ptr = s.ptr,
        .len = s.len,
    };
    for(size_t i = 0; i < s.len; i++) {
        bool is_space = 
            *r.ptr == '\n' ||
            *r.ptr == '\t' ||
            *r.ptr == '\r' ||
            *r.ptr == ' '  ;
        if(!is_space) return r;
        r.len--;
        r.ptr++;
    }
    return r;
}

String str_trim(String s) {
    return str_trim_left(str_trim_right(s));
}


void str_print_fmt(String s, char* pref, char* pofx) {
	if (pref) printf("%s",pref);
	str_loop(i,s.len) {
		printf("%c",s.ptr[i]);
	}
	if (pofx) printf("%s",pofx);
}

INLINE void str_reset(String* s) {
	s->len = 0;
	s->ptr = NULL;
}


#endif // __HCH_STRING_H
