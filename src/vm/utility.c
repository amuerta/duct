#include "vm_declaration.h"

bool string_is_identifier(String s) {
    bool istext = true;
    if(!s.len) return false;
    for(size_t i = 0; i < s.len; i++) {
        istext = istext && (
                hc_range(*(s.ptr + i), 'A', 'Z') ||
                hc_range(*(s.ptr + i), 'a', 'z') ||
                *(s.ptr + i) == ' '              ||
                *(s.ptr + i) == '_'              ||
                *(s.ptr + i) == '\t'             ||
                *(s.ptr + i) == '\n'             ||
                *(s.ptr + i) == '\r'             ||
                (hc_range(*(s.ptr + i), '0', '9') && i>0)
            )
            ;
    }
    return istext;
}

bool string_is_text(String s) {
    bool istext = true;
    if(!s.len) return false;
    for(size_t i = 0; i < s.len; i++) {
        istext = istext && (
                hc_range(*(s.ptr + i), 'A', 'Z') ||
                hc_range(*(s.ptr + i), 'a', 'z') ||
                *(s.ptr + i) == ' '              ||
                *(s.ptr + i) == '\t'             ||
                *(s.ptr + i) == '\n'             ||
                *(s.ptr + i) == '\r'             
            )
            ;
    }
    return istext;
}

int string_to_integer(String s) {
    static char temp[1024];
    memset(temp, 0, sizeof(temp));
    memcpy(temp, s.ptr, s.len);
    return atoi(temp);
}

float string_to_float(String s) {
    static char temp[1024];
    memset(temp, 0, sizeof(temp));
    memcpy(temp, s.ptr, s.len);
    return atof(temp);
}

bool string_cmp(String l, String r) {
    return 
        (l.len == r.len) && 
        ((l.ptr == r.ptr) || 
        (strncmp(l.ptr, r.ptr, r.len) == 0));
}

String string(const char* str) {
    String s;
    s.ptr  = str;
    s.len = strlen(str);
    return s;
}
