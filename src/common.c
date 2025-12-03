#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// DEBUGGING

#define BREAKPOINT() __asm__("int3")


//
// UTILITIES
//


#ifndef __UTIL_H
#define __UTIL_H


#define bm_toggle(N, M)     ((N) ^ (M))
#define bm_set(N, M)        ((N) | (M))
#define bm_clear(N, M)      ((N) & (~(M)))
#define transmute(v, T)     *((T*)&(v))
#define zeroed(V)           memset(&(V), 0, sizeof(V))
#define max(l,r) ((l) > (r)) ? (l) : (r) 
#define min(l,r) ((l) < (r)) ? (l) : (r) 

#define IGNORE_VALUE  (void)
#define INVALID_INDEX ((size_t) -1)

#define MUTCSTR(S) 			((char*) (S))
#define ABORT(MSG)          assert(0 && MSG)

#ifndef DT_ARRLEN 
#	define DT_ARRLEN(A) \
	(size_t)(sizeof(A) / sizeof(A[0]))
#endif

#ifndef DT_ARRLEN_LITERALL
#   define ARRAY_LITTERAL_LENGTH(...)  \
    (ARRAY_LENGTH((__VA_ARGS__)))
#endif

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

//
// DYNAMIC ARRAY
//

#ifndef da_append // if no da_append
                  // implement it
#define DA_GROW_FACTOR 2

#define DA_EXPECT_FIELD(DA) \
    (DA)->items;    /*EXPECTED ITEMS    TO EXIST*/\
    (DA)->count;    /*EXPECTED COUNT    TO EXIST*/\
    (DA)->capacity; /*EXPECTED CAPACITY TO EXIST*/\

#define da_append(DA, ...) do { \
    if ((DA)->capacity == 0) {(DA)->capacity = 32; (DA)->items = calloc(sizeof(*(DA)->items),32);}\
    if ((DA)->count >= (DA)->capacity) {\
        (DA)->capacity *= DA_GROW_FACTOR;\
        (DA)->items = realloc((DA)->items,sizeof(*(DA)->items) * (DA)->capacity);\
    }\
    (DA)->items[((DA)->count)++] = (__VA_ARGS__);\
    DA_EXPECT_FIELD(DA)\
} while(0);

#endif  // da_append end

// 
//  Primitive String builder
//
#ifndef __STRING_BUILDER_H
#define __STRING_BUILDER_H

typedef struct SbNode {
    // maybe add marker/replace functionality
    // int             marker;
    char*           data;
    size_t          count;

    struct SbNode*  next;
    struct SbNode*  prev;
} SbNode;

typedef struct {
    const char* c_string;
    SbNode* list;
    SbNode* last;
} StringBuilder;


void sb_append_one(StringBuilder* sb, int marker, char* string) {
    assert(string);
    size_t len = strlen(string);

    SbNode* node = sb->list;
    SbNode* prev = node;
    while(node) {
        prev = node;
        node = node->next;
    }

    node = calloc(sizeof(SbNode), 1);
    node->data = calloc(len, 1);
    memcpy(node->data, string, len);
    node->count = len;
    node->prev = prev;

    //TODO?
    //node->marker = marker;
    (void) marker;

    sb->last = node;

    if (sb->list)
        prev->next = node;
    else
        sb->list = node;
}

void sb_append_first_str(StringBuilder* sb, int marker, char* string) {
    assert(string);

    (void) marker;

    size_t len = strlen(string);
    SbNode* node = calloc(sizeof(SbNode),1);
    node->data = calloc(len, 1);
    memcpy(node->data, string, len);
    node->count = len;

    node->next = sb->list;
    sb->list = node;
}

#define SB_TEMP_BUFFER_SIZE 4096

#define sb_append_first(sb, ...) do {\
    static char temp[SB_TEMP_BUFFER_SIZE];\
    sprintf(temp, __VA_ARGS__);\
    sb_append_first_str(sb, 0, temp); \
} while(0)

#define sb_append(sb, ...) do {\
    static char temp[SB_TEMP_BUFFER_SIZE];\
    sprintf(temp, __VA_ARGS__);\
    sb_append_one(sb, 0, temp); \
} while(0)

#if 0 // maybe add marker/replace functionality
void sb_replace_one(StringBuilder* sb, int marker, char* newstring) {

}
#endif


const char* sb_collect(StringBuilder* sb, bool reverse) {
    char* result = 0;
    size_t size = 0;
    SbNode* node = reverse? sb->last : sb->list;
    
    while(node) {
        size_t len = node->count;
        result = realloc(result, size + len);
        for(size_t i = 0; i < len; i++)
            result[size + i] = node->data[i];
        size += len;
        
        if (reverse)
            node = node->prev;
        else
            node = node->next;
    }

    result = realloc(result, size + 1);
    result[size] = '\0';
    
    sb->c_string = result;
    return result;
}

void sb_clear(StringBuilder* sb) {
    SbNode* node = sb->list;
    SbNode* prev = 0;
    while(node) {
        prev = node;
        node = node->next;
        
        free(prev->data);
        free(prev);
    }
}
#endif//__STRING_BUILDER_H

//
// ARENA ALLOCATOR
//
#ifndef __ARENA_H
#define __ARENA_H

#ifndef ARENA_NODE_SIZE
#   define ARENA_NODE_SIZE 2048
#endif

#define ARENA_HEADER_SIZE sizeof(ArenaNode)

typedef struct ArenaNode {
    size_t              allocated;
    struct ArenaNode*   next;
    char                data[];
} ArenaNode;

typedef struct {
    size_t      totally_allocated;
    ArenaNode*  memory;
} Arena;

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

void arena_reset(Arena* a) {
    a->totally_allocated = 0;
    ArenaNode* node = a->memory;
    
    while(node) {
        ArenaNode* to_free = node;
        node = node->next;
        free(to_free);
    }
}

void arena_clear(Arena* a) {
    ArenaNode* node = a->memory;
    while(node) {
        memset(node->data, 0, ARENA_NODE_SIZE - ARENA_HEADER_SIZE);
        node = node->next;
    }
}

void* arena_memcpy(Arena* a, void* data, size_t size) {
    void* cell = arena_alloc(a, size);
    assert(cell && "Unexprected null, failed to allocate");
    memcpy(cell, data, size);
    return cell;
}

void* arena_put_string(Arena* a, const char* data) {
    size_t len = strlen(data);
    void* mem = arena_alloc(a, len + 1);
    memcpy(mem, data, len + 1);
    return mem;
}

#define arena_put(ARENA_PTR, ITEM) \
    arena_memcpy(ARENA_PTR, &ITEM, sizeof(ITEM))


#endif//__ARENA_H


//
// MAP
//

#ifndef __HCH_MAP_H
#define __HCH_MAP_H

#ifndef MAPS_DEFAULT_INIT_SIZE
#   define MAPS_DEFAULT_INIT_SIZE 2048
#endif

// Map uses slice instead of cstring
// for obvious reasons...
typedef struct {
    const char* items;
    size_t      count;
} MapKeySlice;

typedef struct {
    MapKeySlice*    keys;
    size_t count, capacity;
} Map;


// hash functions: 
// https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed#145633
static inline unsigned long djb2    (const char* str, size_t size, char shift);
static inline unsigned long fnv1a   (const char* data, size_t size);

// Key
MapKeySlice map_slice(const char* str, size_t count);
MapKeySlice map_key(const char* str);

// map(String)
static inline bool  map_key_is_ok   (long int index);
float               map_load        (Map  m); // in range from 0 to 1
long int            map_query       (Map  m, MapKeySlice string);
long int            map_reserve     (Map* m, MapKeySlice string);
void                map_clear       (Map* m);

#define maps_get    (M, S)                      maps_query   (M, map_key(S))
#define maps_put    (M, S)                      maps_reserve (M, map_key(S))
//      maps_resize (m, new_size, {CODE BLOCK}) /*macro*/




static inline unsigned long djb2(const char* str, size_t size, char shift) {
    unsigned long h = 5381;
    for (size_t i = 0; i < size; i++) 
        h = ((h << shift) + h) + str[i];
    return h;
}

static inline unsigned long fnv1a(const char* data, size_t size) {
    unsigned long h = 2166136261UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }
    return h;
}

MapKeySlice map_key(const char* str) {
    MapKeySlice s = {.items=str, .count=strlen(str)};
    return s;
}

MapKeySlice map_slice(const char* str, size_t count) {
    MapKeySlice s = {.items=str, .count=count};
    return s;
}

static inline bool  map_key_is_ok   (long int index) {
    return index >= 0;
}

float map_load(Map m) {
    assert(m.capacity);
    return (m.count == 0) ? 0 : m.count/m.capacity;
}

Map map_alloc(Map* m, size_t cap) {
    static Map local_map;

    if(!m) m = &local_map;
    
    if (cap == 0) m->capacity = MAPS_DEFAULT_INIT_SIZE;
    else m->capacity = cap;

    m->keys = calloc(m->capacity, sizeof(*m->keys));
    return *m;
}

// the idea is that for each map you implement, you call this 
// generic thing to implement map resize
//
// check example for details: ./examples/maps.c

// for now i don't handle undersizing the map.
#define map_resize(m, new_size, ...) do {\
    if (new_size <= (m)->capacity) break; \
    Map new_map = map_alloc(0, new_size);\
    for(size_t i = 0; i < (m)->capacity; i++) {\
        MapKeySlice key = (m)->keys[i];\
        if (!key.items || !key.count) continue;\
        long int oldid = (long int) i;\
        long int newid = map_reserve(&new_map, key);\
        assert(newid != -1 && "Should always be sucessful");\
        new_map.keys[newid] = key;\
        {__VA_ARGS__}\
    }\
    free((m)->keys);\
    m->keys = new_map.keys;\
    m->capacity = new_map.capacity;\
} while(0)

void map_clear(Map* m) {
    m->count = 0;
    m->capacity = 0;
    free(m->keys);
}


long int map_reserve(Map* m, MapKeySlice string) {
#define     hf1(c, size) (fnv1a(c, size))
#define     hf2(c, size) (djb2(c, size, 33))
    
    assert(m->keys && "call map_alloc() first");

    unsigned long 
        h1 = hf1(string.items, string.count),
        h2,
        index = -1
    ;
    MapKeySlice key = m->keys[(index = (h1 % m->capacity))];
    const size_t cap = m->capacity;

    // collision
    if (key.items) {
        h2 = hf2(string.items, string.count);
        // we hit again
        if (m->keys[(index = (h1+h2)% m->capacity)].items) {
            // "fuck it - iterative approach"
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

#undef  hf1
#undef  hf2
}

long int map_query(Map m, MapKeySlice string) {
 
#define hf1(c, size) (fnv1a(c, size))
#define hf2(c, size) (djb2(c, size, 33))
#define mks_eq(k,s) (strncmp(k.items, s.items, s.count) == 0)

    // initlized
    if(!m.capacity || !m.keys) return -1;
    //  good practice
    assert(string.items && string.count);


    unsigned long
        h1 = hf1(string.items, string.count),
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
    h2 = hf2(string.items, string.count);
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
                              // gap indicated end of this key lookup 
                              // or buggy/corrputed key
        if(key.count != len) continue;
        if (mks_eq(key, string))  
            return (long int)index;
    }

    return -1;

#undef mks_eq
#undef hf1
#undef hf2
}

#endif//__HCH_MAP_H


//
// END OF MAP
//

