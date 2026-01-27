#ifndef __DT_SHARED_H
#define __DT_SHARED_H

#include "hc_plug.h"

enum {
    DT_OPT_TRACE_INTO_SINGLE_FILE = 1 << 0,
    DT_OPT_TRACE_VM_EXECUTION = 1 << 1,
    DT_OPT_TRACE_VM_ASSEMBLY_COMPILATION = 1 << 2,
    DT_OPT_TRACE_COMPILER_AST_WALKING = 1 << 3,
    
    DT_OPT_EXPECT_INITILIZED_STREAM_FILES = 1 << 10,
};

String string_alloc_to_arena(Arena* allocator, String it);
const char* dt_temp_format(const char* fmt, ...);

//
// IMPLEMENTATION
//


const char* dt_temp_format(const char* fmt, ...) {
    static char temp[DT_SCRATCH_BUFFER_SIZE];
    memset(temp, 0, sizeof(temp));
    va_list args, args_len;
    va_start(args, fmt);
    va_copy(args_len, args);
    size_t size = vsnprintf(0,0,fmt,args_len);
    va_end(args_len);

    assert(size <= DT_SCRATCH_BUFFER_SIZE);
    vsnprintf((char*)temp, size+1, fmt, args);
    
    va_end(args);
    return temp;
}


String string_alloc_to_arena(Arena* allocator, String it) {
    char* string = arena_alloc(allocator, it.len);
    memcpy(string, it.ptr, it.len);
    String moved = {
        .ptr = string,
        .len = it.len
    };
    return moved;
}

#endif // __DT_SHARED_H
