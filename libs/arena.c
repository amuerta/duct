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

void arena_memcpy(Arena* a, void* data, size_t size) {
    void* cell = arena_alloc(a, size);
    assert(cell && "Unexprected null, failed to allocate");
    memcpy(cell, data, size);
}

#define arena_put(ARENA_PTR, ITEM) \
    arena_memcpy(ARENA_PTR, &ITEM, sizeof(ITEM))


#endif//__ARENA_H
