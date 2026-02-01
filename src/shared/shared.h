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


long tkn_parse_string(
        const char* str, size_t length, 
        char* out, size_t* written,
        const char string_quote_character) 
{
    enum { PARSE_STRING_OPEN, PARSE_STRING_CLOSE };

    bool reached_string_end = false;
    size_t walked           = 0;
    if(written) *written    = 0;
    size_t string_count     = strlen(str);
    bool parsing_string     = false;
    
    for(size_t i = 0; i < length; i++) {
        char current = str[i];
        char out_char = 0;

        if (!parsing_string && current == string_quote_character) {
            parsing_string = true;
            walked++;
            continue;
        }

        if(parsing_string) {
            // check escape code.
            if(current == '\\') {
                i++;
                walked++;
                current = str[i];
                switch (current) {
                    case 'n':  out_char = '\n'; break;
                    case 't':  out_char = '\t'; break;
                    case 'r':  out_char = '\r'; break;
                    case '0':  out_char = '\0'; break;
                    case '\\': out_char = '\\'; break;
                    case '\'': out_char = '\''; break;
                    case '"':  out_char = '"';  break;
                    default: return -1;
                }
            }
            else if (current == string_quote_character) {
                parsing_string = false;
                walked++;
                break;
            }
            // after check if character was escaped, if not use current one
            // to write to out;

            if(out) 
                out[(*written)] = out_char ? out_char : current;
            if(written) (*written)++;
        }

        walked++;
    }
    return walked;
}

#endif // __DT_SHARED_H
