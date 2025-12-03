#include "common.c"

typedef struct {
    const char* name;
    size_t      length;
} DtIdentifer;

// 
// TYPE INFORMATION
// 

typedef unsigned char dt_bitmask8;
typedef unsigned char dt_enum8;
typedef long long int dt_numeric;
typedef int           dt_error;

enum {
    DT_VALUE_CONSTANT     = (1 << 0),
    DT_VALUE_LOCKED       = (1 << 1),
    DT_VALUE_UNSIGNED     = (1 << 2),
    
    DT_VALUE_IS_DYNAMIC   = (1 << 5),
    DT_VALUE_IS_REFERENCE = (1 << 6),
    DT_VALUE_IS_ARRAY     = (1 << 7),
};

enum {
    DT_TYPE_NULL = 0,
    DT_TYPE_VOID,
    DT_TYPE_BOOL,
    DT_TYPE_BYTE,
    DT_TYPE_INT,
    DT_TYPE_LONG,
    DT_TYPE_FLOAT,
    DT_TYPE_DOUBLE,

    DT_TYPE_ERROR,
    DT_TYPE_TYPEDEF,
    DT_TYPE_STRING,
    DT_TYPE_OBJECT,
    DT_TYPE_FUNCTION,
};

enum {
    DT_ERROR_NONE = 0,
    DT_ERROR_UNKOWN_TYPE,
    DT_ERROR_TYPE_MISSMATCH,
    DT_ERROR_NOT_ARRAY,
    DT_ERROR_BUFFER_OVERFLOW,
    DT_ERROR_UNRESOLVABLE_TYPE,
    DT_ERROR_UNRESOLVABLE_COMPLEX_TYPE,
    DTR_ERROR_UNSUPPORTED_OPERAION,
};

struct DtObject;

typedef struct DtArray {
    void*   base_ptr;
    size_t  typesize;
    size_t  length;
} DtArray;

typedef struct DtFunc {
    dt_enum8    return_type;
    dt_bitmask8 return_properties;
    dt_bitmask8 properties;
    DtIdentifer         name;
    struct DtObject*    arguments;
    void*               entry;
} DtFunc;

typedef struct DtType {
    int         typeid;
    DtIdentifer name;
} DtType;

typedef struct DtValue {
    dt_enum8        type;
    dt_bitmask8     properties;
    // TODO: value.as.t_byte .. etc
    // anonymous unions are illegal in C99, they are feature since C11
    union {
        char            as_byte;
        int             as_int;
        long long       as_long;
        float           as_float;
        double          as_double;

        DtArray         as_array;
        DtFunc          as_function; 
        DtType          as_type;
    };
} DtValue;


// 
// MEMORY "OBJECTS"
// 

typedef struct DtObject {
    DtValue            value;
    DtIdentifer        identifier;
    struct DtObject*   next;
    struct DtObject*   children; 
    //dt_numeric              typetable_id;
} DtObject;


enum {
    DT_BINOP_NONE = 0,
    DT_BINOP_ADD,
    DT_BINOP_SUB,
    DT_BINOP_MUL,
    DT_BINOP_DIV
};


// 
// RUNTIME CONTEXT and SCOPE INFORMATION
// 

typedef struct DtScope {
    Map         head;
    DtObject*   objects;
    Arena       temporary_memory;
    size_t      count, capacity;
} DtScope;

typedef struct DtScopeList {
    DtScope* *items; // array of pointers to "visible" scopes
    size_t count, capacity;
} DtScopeList;

typedef struct DtStackFrame {
    DtScopeList scopes;
    DtObject    function_identity;
    DtObject    ret;
} DtStackFrame;

#define DT_MAX_CALL_DEPTH 256

typedef struct DtContext {
    Arena           main_allocator;
    Arena           name_allocator;
    
    DtScope         scratch; // for returns, and function arguments
    DtScope         types;
    DtScope         functions;
    DtScope         dynamic;
    DtScope         global;
    
    DtScope*        current;
    size_t          node_depth;
    size_t          call_depth;
    
    DtObject       ret;
    bool            eval_mode;
} DtContext;

//
// cosntants and defaults
//
#define DTV_SCOPE_INITIAL_CAPACITY 256
static const DtIdentifer DT_NO_INDENT = {0};
static const DtObject DT_OBJECT_NULL = {0};

//
// IMPLEMENTATION
//

int dto_serialize_numeric
(  char* buffer, size_t cap, 
   dt_bitmask8 print_flags, 
   dt_numeric value, dt_enum8 type) 
{
    (void) print_flags;
    switch(type) {
        case DT_TYPE_BOOL:   
            return snprintf(buffer, cap, "%s", value ? "true" : "false");
        case DT_TYPE_BYTE:   
            return snprintf(buffer, cap, "%d", transmute(value, char));
        case DT_TYPE_INT:    
            return snprintf(buffer, cap, "%i", transmute(value, int));
        case DT_TYPE_LONG:    
            return snprintf(buffer, cap, "%lli", transmute(value, long long));
        case DT_TYPE_FLOAT:    
            return snprintf(buffer, cap, "%f", transmute(value,float));
        case DT_TYPE_DOUBLE:    
            return snprintf(buffer, cap, "%lf", transmute(value, double));

        default: assert(0 && "called `dto_serialize_numeric` on non numeric type");
    }
}

dt_numeric dto_numeric_from_float(float flt) {
    dt_numeric n = 0;
    memcpy(&n, &flt, sizeof(flt));
    return n;
}

dt_numeric dto_numeric_from_double(double flt) {
    dt_numeric n = 0;
    memcpy(&n, &flt, sizeof(flt));
    return n;
}

DtObject dto_object_from_numeric(DtIdentifer name, dt_bitmask8 type, dt_numeric raw_bytes) {
    DtObject result = {0};
    switch(type) {
        case DT_TYPE_BYTE:
        case DT_TYPE_BOOL:
            result.value.as_byte = raw_bytes;
            break;
        case DT_TYPE_INT:
            result.value.as_int = raw_bytes;
            break;
        case DT_TYPE_FLOAT:
            memcpy( &result.value.as_float, 
                    &raw_bytes, 
                    sizeof(result.value.as_float));
            break;
        case DT_TYPE_DOUBLE:
            memcpy( &result.value.as_double, 
                    &raw_bytes, 
                    sizeof(result.value.as_double));
            break;

        
        // invalid object
        default: 
            return result;
    }

    result.identifier = name;
    result.value.type = type;
    return result;
}

DtObject dto_object_new(DtIdentifer name, dt_bitmask8 properties) {
    DtObject result = {
        .identifier = name,
        .value = {
            .type = DT_TYPE_OBJECT,
            .properties = properties,
        }
    };
    return result;
}

DtObject dto_object_error(dt_enum8 error_code) {
    DtObject result = {
        .value = {
            .type = DT_TYPE_ERROR,
            .as_int = error_code
        }
    };
    return result;
}

void dto_object_append(Arena* allocator, DtObject* o, DtObject field) {
    DtObject* item = arena_alloc(allocator, sizeof(field));
    memcpy(item, &field, sizeof(field));

    if (o->children) {
        DtObject* it = o->children;
        if (!it)  { it = item; return; }
        while(it) { if (!it->next) break; it = it->next; } 
        it->next = item;
    } else o->children = item;
}

size_t dto_type_size(dt_enum8 type) {
    switch(type) {
        case DT_TYPE_BYTE: 
        case DT_TYPE_BOOL: 
                            return 1;
        case DT_TYPE_INT: 
        case DT_TYPE_FLOAT: 
                            return 4;
        case DT_TYPE_LONG: 
        case DT_TYPE_DOUBLE: 
                            return 8;
        case DT_TYPE_OBJECT:
                            return sizeof(DtObject*);
    }
    return 0;
}


DtObject dto_array_new(Arena* allocator, DtIdentifer name, dt_enum8 type, dt_bitmask8 properties, size_t length) {
    //#define temp_size 512
    //static char dt_temp_buffer[temp_size];
    size_t type_size = dto_type_size(type);
    //assert(length < temp_size/type_size);
    DtObject result = {
        .identifier = name,
        .value = {
            .type = type,
            .properties = bm_set(properties, DT_VALUE_IS_ARRAY),
            .as_array = {
                .base_ptr = arena_alloc(allocator, type_size * length),
                .typesize = type_size,
                .length = length,
            }
        }
    };

    return result;
}

DtObject dto_string_new(Arena* allocator, DtIdentifer name, dt_bitmask8 properties, size_t length) {
    DtObject array = dto_array_new(allocator, name, DT_TYPE_BYTE, properties, length); 
    array.value.type = DT_TYPE_STRING;
    return array;
}



dt_error dto_string_set_sized(DtObject* o, const char* str, size_t length) {
    size_t len = length;
    if(!(o->value.type == DT_TYPE_STRING))  return DT_ERROR_TYPE_MISSMATCH;
    if(len > o->value.as_array.length)     return DT_ERROR_BUFFER_OVERFLOW;
    memcpy(o->value.as_array.base_ptr, str, len);
    return DT_ERROR_NONE;
}

dt_error dto_string_set(DtObject* o, const char* str) {
    return dto_string_set_sized(o, str, strlen(str));
}

dt_error dto_array_set(Arena* allocator, DtObject* o, DtObject item, size_t i) {
    // TODO: check properties too
    if( !(o->value.type == item.value.type))        return DT_ERROR_TYPE_MISSMATCH;
    if( !(o->value.properties & DT_VALUE_IS_ARRAY)) return DT_ERROR_NOT_ARRAY;

    // if array will be overflowed, fail too
    if (o->value.as_array.length <= i) return DT_ERROR_BUFFER_OVERFLOW;

    void* array = o->value.as_array.base_ptr;

    switch(o->value.type) {
        case DT_TYPE_BYTE:
        case DT_TYPE_BOOL:
            ((char*)array)[i] = item.value.as_byte;
            break;
        case DT_TYPE_INT:
            ((int*)array)[i] = item.value.as_int;
            break;
        case DT_TYPE_FLOAT:
            ((float*)array)[i] = item.value.as_float;
            break;
        case DT_TYPE_LONG:
            ((long long*)array)[i] = item.value.as_long;
            break;
        case DT_TYPE_DOUBLE:
            ((double*)array)[i] = item.value.as_double;
            break;
        case DT_TYPE_OBJECT: 
        {
            // we store object pointers and allocate them in arena
            DtObject* obj_from_arena = arena_alloc(allocator, sizeof(item));
            memcpy(obj_from_arena, &item, sizeof(item));
            ((DtObject**)array)[i] = obj_from_arena;
            break;
        }
        default:
            return DT_ERROR_UNKOWN_TYPE;
    }

    return DT_ERROR_NONE;
}

dt_numeric dto_array_get_numeric(DtObject o, size_t i) {
    if (!(o.value.properties & DT_VALUE_IS_ARRAY))
        assert(0 && "CAN'T CONVERT ARRAY TO NUMERIC");

    // if array will be overflowed, fail too
    if (o.value.as_array.length <= i) assert(0 && "ATTEMPT TO OVERFLOW ARRAY");;

    void* array = o.value.as_array.base_ptr;
    switch(o.value.type) {
        case DT_TYPE_BYTE:
        case DT_TYPE_BOOL:
            return transmute(((char*)array)[i], dt_numeric);
        case DT_TYPE_INT:
            return transmute(((int*)array)[i], dt_numeric);
        case DT_TYPE_FLOAT:
            return transmute(((float*)array)[i], dt_numeric);
            break;
        case DT_TYPE_LONG:
            return transmute(((long long*)array)[i], dt_numeric);
            break;
        case DT_TYPE_DOUBLE:
            return transmute(((double*)array)[i], dt_numeric);
            break;
        default:
            assert(0 && "objects, functions, arrays are gotten by using different functions");
    }

    return 0;
}

#define dt_zeroed(V) memset(&(V), 0, sizeof(V))

DtObject* dto_array_get_object(DtObject o, size_t i) {
    static DtObject dummy_as_error;
    dt_zeroed(dummy_as_error);
    
    if (!(o.value.properties & DT_VALUE_IS_ARRAY)) {
        dummy_as_error = dto_object_error(DT_ERROR_NOT_ARRAY);
        return &dummy_as_error;
    }
    
    if (o.value.as_array.length <= i) {
        dummy_as_error = dto_object_error(DT_ERROR_BUFFER_OVERFLOW);
        return &dummy_as_error;
    }

    if (o.value.type != DT_TYPE_OBJECT) {
        dummy_as_error = dto_object_error(DT_ERROR_TYPE_MISSMATCH);
        return &dummy_as_error;
    } 

    DtObject** array = o.value.as_array.base_ptr;
    return array[i];
}


//
// BINOP
//


// return resolved highest prec type
int dto_type_resolve(DtObject* l, DtObject* r) {
    dt_enum8 
        tl = l->value.type,
        tr = r->value.type
    ;
    dt_bitmask8 
        pl = l->value.properties,
        pr = r->value.properties
    ;

    // Nothing to do
    if ((tr == tl) && (pr == pl)) return tl;

    DtObject* higher = (tl > tr) ? l : r;
    DtObject* lower =  (tl < tr) ? l : r;

    bool h_is_integer = 
        higher->value.type == DT_TYPE_BYTE ||
        higher->value.type == DT_TYPE_BOOL ||
        higher->value.type == DT_TYPE_INT  ||
        higher->value.type == DT_TYPE_LONG 
    ;
    bool h_is_float = 
        higher->value.type == DT_TYPE_FLOAT  ||
        higher->value.type == DT_TYPE_DOUBLE 
    ;
    bool h_is_object = 
        higher->value.type == DT_TYPE_OBJECT 
    ;

    lower->value.type = higher->value.type;
    
    if (h_is_integer)
        lower->value.as_long = (long long)(lower->value.as_long);
    else if (h_is_float)
        lower->value.as_float = (float)(lower->value.as_long);
    else {
        (void) h_is_object;
        // TODO: add unique type-to-code error
        return 0;
    }

    //asm("int3");
    return higher->value.type;
}

enum {
    DT_COMPARE_NONE,

    DT_COMPARE_EQ, // pure equality
    DT_COMPARE_GT,
    DT_COMPARE_LT,

    // flag, used with Greater Than, Less Than
    DT_COMPARE_EQWITH = (1 << 7),
};

// DONE:
//  + pure equality
// TODO:
//  + greater than,
//  + less than,
//  + g/l than with equality
DtObject dto_object_compare(DtObject l, DtObject r, dt_enum8 cmp_type) {
    int tresult = 0;
    DtObject result = DT_OBJECT_NULL;

    if(!(tresult = dto_type_resolve(&l, &r))) // unrecoverable type error
        return dto_object_error(DT_ERROR_UNRESOLVABLE_TYPE); 
    result.value.type = DT_TYPE_BOOL;

    // TODO: handle unsigned
    switch(cmp_type) {

        // compare equality
        case DT_COMPARE_EQ:
            // if array, do same as for string
            if (r.value.properties & DT_VALUE_IS_ARRAY) goto as_array;
            switch(r.value.type) {
                case DT_TYPE_BOOL:
                case DT_TYPE_BYTE:
                case DT_TYPE_INT:
                case DT_TYPE_LONG:
                case DT_TYPE_FLOAT:
                case DT_TYPE_DOUBLE:
                    result.value.as_byte = (r.value.as_long == l.value.as_long);
                    break;
                
                // label
                as_array:
                case DT_TYPE_STRING: 
                {
                    void* lhsd = l.value.as_array.base_ptr;
                    void* rhsd = r.value.as_array.base_ptr;
                    size_t llen = l.value.as_array.length * l.value.as_array.typesize,
                           rlen = r.value.as_array.length * r.value.as_array.typesize;
                    if (rlen == llen)
                        r.value.as_byte = (memcmp(rhsd,lhsd,llen) == 0);
                    else
                        r.value.as_byte = false;
                    break;
                }
                case DT_TYPE_OBJECT:
                    assert(0 && "TODO: implement resurive and/or buffered comparison of objects");
                    break;


                default: 
                    assert(0 && "TODO: error handling or more comparisons");
                break;
            }
        break;
    }

    return result;
}

DtObject dto_object_binop(DtObject l, DtObject r, dt_enum8 binop_type) {
    
#define DT_BINOP(result, l,r,OP) \
    switch(l.value.type) {\
        case DT_TYPE_BOOL: \
        case DT_TYPE_BYTE: \
            (result).value.as_byte = l.value.as_byte OP r.value.as_byte;\
        break;\
        case DT_TYPE_INT: \
            (result).value.as_int = l.value.as_int OP r.value.as_int;\
        break;\
        case DT_TYPE_FLOAT: \
            (result).value.as_float = l.value.as_float OP r.value.as_float;\
        break;\
        case DT_TYPE_LONG: \
            (result).value.as_long = l.value.as_long OP r.value.as_long;\
        break;\
        case DT_TYPE_DOUBLE: \
            (result).value.as_double = l.value.as_double OP r.value.as_double;\
        break;\
        default:\
            return dto_object_error(DT_ERROR_UNRESOLVABLE_COMPLEX_TYPE);\
        break;\
    }

    int tresult = 0;
    DtObject result = DT_OBJECT_NULL;

    if(!(tresult = dto_type_resolve(&l, &r))) // unrecoverable type error
        return dto_object_error(DT_ERROR_UNRESOLVABLE_TYPE); 
    result.value.type = l.value.type;

    switch(binop_type) {
        case DT_BINOP_ADD: DT_BINOP(result, l, r, +); break;
        case DT_BINOP_SUB: DT_BINOP(result, l, r, -); break;
        case DT_BINOP_MUL: DT_BINOP(result, l, r, *); break;
        case DT_BINOP_DIV: DT_BINOP(result, l, r, /); break;

        default:
            return dto_object_error(DTR_ERROR_UNSUPPORTED_OPERAION);
    }
    return result;
#undef DT_BINOP
}


//
// SCOPE
//

DtIdentifer dto_ident(const char* str) {
    DtIdentifer id = {
        .name = str,
        .length = strlen(str)
    };
    return id;
}

DtScope dto_scope_init(void) {
    DtScope s = {
        .head    = map_alloc(0, DTV_SCOPE_INITIAL_CAPACITY),
        .objects = calloc(DTV_SCOPE_INITIAL_CAPACITY, sizeof(DtObject)),
        .count   = 0,
        .capacity= DTV_SCOPE_INITIAL_CAPACITY,
    };
    return s;
}

void dto_scope_clear(DtScope* s) {
    arena_reset(&s->temporary_memory);
    map_clear(&s->head);
    free(s->objects);
    memset(s, 0, sizeof(*s));
}

int dto_scope_push(DtScope* s, DtObject o) {
    Map* map = &s->head;
    DtIdentifer ident = o.identifier;

    MapKeySlice key = map_slice(ident.name, ident.length);
    long int oid = map_query(*map, key);
    if (oid == -1) {
        oid = map_reserve(map, key);
        assert(oid != -1);
    }


    s->objects[oid] = o;
    //transmute(&s->objects[oid], DtObject*) = o;

    return 0;
}


DtObject* dto_scope_ref(DtScope* s, DtIdentifer ident) {
    Map map = s->head;
    DtObject* result = 0; // invalid object

    MapKeySlice key = map_slice(ident.name, ident.length);
    long int oid = map_query(map, key);

    if (oid == -1)  return result;
    else            return &(s->objects[oid]);
}

DtObject dto_scope_get(DtScope s, DtIdentifer ident) {
    Map map = s.head;
    DtObject result = {0}; // invalid object

    MapKeySlice key = map_slice(ident.name, ident.length);
    long int oid = map_query(map, key);

    if (oid == -1)  return result;
    else            return s.objects[oid];
}


//
// SERIALIZER
//

enum {
    DT_SERIALIZE_WITH_NAMES     = (1 << 0),
    DT_SERIALIZE_PRETTIFY       = (1 << 1),
    DT_SERIALIZE_PUT_STRING_QUOTATIONS = (1 << 2),
};

typedef struct {
    dt_bitmask8 flags;
    const char* spacing;
} DtSerializeOpt;

typedef struct {
    int     error_code;
    size_t  write_size;
} DtSerializeResult;

static inline DtSerializeResult dt_serres(int error, size_t writesz) {
    DtSerializeResult r = {.error_code = error, .write_size = writesz};
    return r;
}

DtSerializeResult dto_serialize    (char* buffer, size_t cap, DtSerializeOpt opt, DtObject v);
DtSerializeResult dto_serialize_rec(char* buffer, size_t cap, DtSerializeOpt opt, DtObject v, int depth);

DtSerializeResult dto_serialize(char* buffer, size_t cap, DtSerializeOpt opt, DtObject v) {
    return dto_serialize_rec(buffer, cap, opt, v, 0);
} 

DtSerializeResult dto_serialize_rec(char* buffer, size_t cap, DtSerializeOpt opt, DtObject v, int depth) {
    dt_bitmask8 print_flags = opt.flags;
    int write_size = 0;
    size_t array_index = 0;
    size_t spacing_size = strlen(opt.spacing);
    DtObject* field = v.children;
    DtSerializeResult result = {0};

    // TODO: use dynamic storage?
    char temp[cap];

    const bool PRETTIFY = (print_flags & DT_SERIALIZE_PRETTIFY);
    const bool NAMED    = (print_flags & DT_SERIALIZE_WITH_NAMES);
    const bool QUOTE    = (print_flags & DT_SERIALIZE_PUT_STRING_QUOTATIONS);
    const bool ARRAY    = (v.value.properties & DT_VALUE_IS_ARRAY);
    const bool STRING   = (v.value.type == DT_TYPE_STRING );

    if (ARRAY && !STRING) {
        write_size+= 2;
        strncat(buffer, "[ ", cap);
    }
serialize_again:
    memset(temp, 0, cap);
    switch(v.value.type) {

        case DT_TYPE_BOOL:   
        case DT_TYPE_BYTE:   
        case DT_TYPE_INT:    
        case DT_TYPE_LONG:    
        case DT_TYPE_FLOAT:    
        case DT_TYPE_DOUBLE:    
            {
                if (ARRAY) {
                    dt_numeric as_numeric = dto_array_get_numeric(v, array_index);
                    write_size += dto_serialize_numeric( temp, cap, 0, 
                            transmute(as_numeric, dt_numeric),  
                            v.value.type
                    ); 
                    strncat(buffer,temp,cap);
                } else {
                    write_size = dto_serialize_numeric( buffer, cap, 0, 
                            transmute(v.value.as_byte, dt_numeric),  
                            v.value.type
                    ); 
                    return dt_serres(DT_ERROR_NONE, write_size);
                }
            }
        break;

        case DT_TYPE_STRING:
            assert(ARRAY && "TODO: error checking, supposed to be array");
            if (QUOTE)
                snprintf(temp, cap, "\"%.*s\"", 
                        (int)v.value.as_array.length, 
                        (const char*)v.value.as_array.base_ptr
                );
            else
                snprintf(temp, cap, "%.*s", 
                        (int)v.value.as_array.length, 
                        (const char*)v.value.as_array.base_ptr
                );
            strncat(buffer, temp, cap);
        break;

        case DT_TYPE_OBJECT:
        {
            if (ARRAY) {
                DtObject* arr_item = dto_array_get_object(v, array_index);
                DtObject empty = DT_OBJECT_NULL;
                empty.value.type = DT_TYPE_OBJECT;
                //printf("%p\n", arr_item);
                //assert(arr_item);
                result = dto_serialize_rec(temp, cap, opt, 
                        arr_item ? *arr_item : empty,
                        depth + 1);
                write_size += result.write_size;
                strncat(buffer, temp, cap);
            }
            else { 
                if (field) {
                    int i = 0;
                    (void) i; // stupid warning doesn't see 'i' is used
                    write_size += 2;
                    strncat(buffer, "{ ", cap);

                    if (PRETTIFY) {
                        strncat(buffer, "\n", cap);
                        write_size++;
                    }

                    while(field) {
                        if (PRETTIFY) {
                            for(int i = 0; i<depth+1; i++) {
                                strncat(buffer, opt.spacing, cap);
                                write_size+=spacing_size;
                            }
                        }
                        if (NAMED) {
                            write_size += sprintf(temp, "%.*s = ", (int)field->identifier.length, field->identifier.name);
                            strncat(buffer, temp, cap);
                            memset(temp, 0, cap);
                        }

                        DtSerializeResult

                        result = dto_serialize_rec(temp, cap, opt, *field, depth + 1);
                        write_size += result.write_size;
                        strncat(buffer, temp, cap);
                        memset(temp, 0, cap);
                        
                        if (field->next) {
                            write_size += 2;
                            strncat(buffer, ", ", cap);
                        }

                        if (PRETTIFY) {
                            write_size++;
                            strncat(buffer, "\n", cap);
                        }

                        // post loop
                        field = field->next; 
                        i++;
                    }
                    if (PRETTIFY) {
                        for(int i = 0; i<depth; i++) {
                            strncat(buffer, opt.spacing, cap);
                            write_size+=spacing_size;
                        }
                        strncat(buffer, "}", cap);
                        write_size++;
                    } else {
                        strncat(buffer, " }", cap);
                        write_size+=2;
                    }
                } else {
                    strncat(buffer, "{}", cap);
                }
                return dt_serres(DT_ERROR_NONE, write_size);
            }
        }
        break;

        default: return dt_serres(DT_ERROR_UNKOWN_TYPE, 0);
    }

    if (ARRAY && !STRING) {
        write_size+= 2;

        if (array_index >= v.value.as_array.length - 1) {
            strncat(buffer, " ]", cap);
        } else {
            strncat(buffer, ", ", cap);
            array_index++;
            goto serialize_again;
        }
    }

    return dt_serres(DT_ERROR_NONE, write_size);
}




// TODO:
// - Functions:
//  - have void* for their body that is a DtNode* where execution should start.
//  - (has its own scope when treversing ast)
//  - can have its own function pointer to user defined function that is
//      executed instead of the AST body node.
//
// - Globals 
// - dynamically grow arrays -> implement a daynamic arena node
// - consider adding list as a buildin data structure, maybe a map too?
// - Operations: bitshifts, comparison

#if 0

int main(void) {
    char buffer[1024];
    DtScope stack = dto_scope_init();
#if 0
    printf(
            "Specs:\n"
            "sizeof(Object) = %lu\n"
            "sizeof(DtFunc) = %lu\n"
            "sizeof(DtArray) = %lu\n"
            "\n"
            , 
            sizeof(DtObject), 
            sizeof(DtFunc),
            sizeof(DtArray)
    );
#endif

    DtContext ctx = {
        .current = &stack,
        .global = dto_scope_init(),
        .depth = 0,
    };
    Arena* temp = &stack.temporary_memory;


    DtObject o = dto_object_new(dto_ident("object"),0);
    DtObject o2 = dto_object_new(dto_ident("object_child"),0);

    DtObject str = dto_string_new(temp, dto_ident("string"), DT_TYPE_OBJECT, 0, 32);
    DtObject oa = dto_array_new(temp, dto_ident("array_objects"), DT_TYPE_OBJECT, 0, 5);
    DtObject a = dto_array_new(temp, dto_ident("array"), DT_TYPE_INT, 0, 5);
    
    dto_string_set(&str, "Hello world");

    DtObject n1 = dto_object_from_numeric(
            dto_ident("number"), 
            DT_TYPE_FLOAT,
            dto_numeric_from_float(0)
        );


    DtObject n2 = dto_object_from_numeric(
            dto_ident("number2"), 
            DT_TYPE_INT,
            10//dto_numeric_from_float(3.14159)
        );


    // returned Object is nameless.
    DtIdentifer n1_ident = n1.identifer;
    n1 = dto_object_binop(n1, n2, DT_BINOP_ADD);
    n1.identifer = n1_ident;

#if 1
    dto_array_set(temp, &a, dto_object_from_numeric(DT_NO_INDENT, DT_TYPE_INT, -128),0);
    dto_array_set(temp, &a, dto_object_from_numeric(DT_NO_INDENT, DT_TYPE_INT, 1),1);
    dto_array_set(temp, &a, dto_object_from_numeric(DT_NO_INDENT, DT_TYPE_INT, 2),2);
    dto_array_set(temp, &a, dto_object_from_numeric(DT_NO_INDENT, DT_TYPE_INT, 3),3);
    dto_array_set(temp, &a, dto_object_from_numeric(DT_NO_INDENT, DT_TYPE_INT, 69),4);
#endif

    dto_object_append(temp,&o, n1);
    dto_object_append(temp,&o2,n2);
    dto_object_append(temp,&o, o2);
    

    dto_array_set(temp, &oa, o, 0);
    dto_array_set(temp, &oa, o, 2);
    dto_scope_push(&stack, o);
    dto_scope_push(&stack, a);
    dto_scope_push(&stack, oa);
    dto_scope_push(&stack, str);
    
    DtSerializeOpt opt = { 
        .flags = 0 |
            DT_SERIALIZE_PRETTIFY | DT_SERIALIZE_WITH_NAMES |
            DT_SERIALIZE_PUT_STRING_QUOTATIONS
        ,
        .spacing = "  ",
    };

    dto_serialize(buffer, 1024, opt, dto_scope_get(stack, dto_ident("object")));
    printf("%s\n", buffer);

    memset(buffer, 0 , 1024);
    dto_serialize(buffer, 1024, opt, dto_scope_get(stack, dto_ident("array")));
    printf("%s\n", buffer);
 
    memset(buffer, 0 , 1024);
    dto_serialize(buffer, 1024, opt, dto_object_compare(n1,n2, DT_COMPARE_EQ));
    printf("%s\n", buffer);
 

    memset(buffer, 0 , 1024);
    dto_serialize(buffer, 1024, opt, dto_scope_get(stack, dto_ident("string")));
    printf("%s\n", buffer);


    dto_object_append(temp,&o, str);

    memset(buffer, 0 , 1024);
    dto_serialize(buffer, 1024, opt, dto_scope_get(stack, dto_ident("array_objects")));
    printf("%s\n", buffer);

    dto_scope_clear(&stack);
    dto_scope_clear(&ctx.global);
}
#endif
