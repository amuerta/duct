#ifndef __DTVM_DEFINITIONS_H 
#define __DTVM_DEFINITIONS_H 

/*  LIBC    */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

/*  STRING, SB, DA  */
#define HC_DA_MACRO_BASED
#include "../shared/hc.h"

/*  MACROS  */
#define da_append           hc_da_append
#define da_free             hc_da_free
#define str_fmt(s)          (int)(s).len,(s).ptr
#define arrlen(A)           (sizeof(A)/sizeof(A[0]))
#define va_wrap(T,...)      ((T[]) __VA_ARGS__)
#define va_array(T,...)     va_wrap(T,__VA_ARGS__),\
                                arrlen(va_wrap(T,__VA_ARGS__))
#define debug(...) fprintf(stderr, "DEBUG: "__VA_ARGS__)
#define inst(...) (Instruction) {__VA_ARGS__}

#define UNREACHABLE(...) do {\
    fprintf(stderr,"UNREACHABLE at [%s:%s:%d]: ",__FILE__,__func__,__LINE__); \
    fprintf(stderr,__VA_ARGS__); \
    fprintf(stderr,"\n"); \
    abort();\
} while(0)


/*  DEFINES OR CONSTANTS    */
#define         DTVM_SCOPES                         256
#define         DTVM_OBJECT_MAP_CAPACITY            256
#define         DTVM_STACK_CAPACITY                 1024
#define         DT_SCRATCH_BUFFER_SIZE              1024
#define         DTVM_MAX_ITERATORS                  32
#define         DTVM_TEMPORARY_STRING_BUILDER_CAP   512
#define         DT_SIZE_T_INVALID                   ((size_t)-1)

/*  ENUMS   */

typedef enum {
    OT_NULL, // should be an error if encountered. NULL's are bad.
    OT_BOOL,
    OT_BYTE,
    OT_CHAR,
    OT_INT,
    OT_FLOAT,
    OT_STRING,
    OT_TYPE,
    OT_RESULT, 
} EObjectType;

// VM
enum {
    DTI_NULL, // probably error
    DTI_NOP,
    DTI_STORE,
    DTI_LOAD,

    DTI_CALL,
    DTI_RET,

    DTI_PUSH,
    DTI_DUP,
    DTI_POP,
    DTI_ROT,
    DTI_ADD,
    DTI_SUB,
    DTI_DIV,
    DTI_MUL,
    DTI_AND,
    DTI_OR,
    DTI_NOT,


    DTI_EQ,     // ==
    DTI_LT,     // < 
    DTI_GT,     // >
    DTI_LTE,    // <=
    DTI_GTE,    // >=
         
    // WORK ON LITERALS:
    DTI_LEQ,     // ==
    DTI_LLT,     // < 
    DTI_LGT,     // >
    DTI_LLTE,    // <=
    DTI_LGTE,    // >=
    
    DTI_IJIF,   // INCREMENTAL JUMP IF
    DTI_JMP,    
    
    DTI_WRT,    // writes top of the stack to vm.fstdout

    DTI_COUNT,
} EInstruction;

enum {
    DTSTAT_OK       = 0,
    DTSTAT_IGNORE,
    DTSTAT_START_FUNCTION,
    DTSTAT_END_FUNCTION,
    DTSTAT_NOT_ERROR,

    DTSTAT_INVALID_OPERAND_TYPE,
    DTSTAT_NO_OPERAND,
    
    DTSTAT_COUNT,
} ECompileStatus;

enum {
    DTVM_CONFIG_TRACE_LOG
} EDtvmConfig; 

enum {
    ELINE_INSTRUCTION,
    ELINE_LABEL,
    ELINE_FUNCTION,
} ELineResult;

/* TYPEDEFS */
static String  STRING_NULL = {0};

// STRUCTS //

typedef struct {
    String  name;
    long    address;
} Label; 

typedef struct {
    Label* items;   
    size_t count, capacity, typesize;
} Labels;

// OBJECT(s)
typedef int Result;
typedef struct Object {
    String   id;
    int     type;
    union {
        bool            B;
        unsigned char   b;
        int             i;
        float           f;
        Result          r;
        String          s; // read only string
        EObjectType     T; // object type
    } as;
} Object;

typedef struct ObjectMap {
    Arena   local_allocator;  // temporary allocator for dynamic objects
    Object* items;
    MapHead map_head;
} ObjectMap;

typedef struct {
    ObjectMap   buf[DTVM_SCOPES];
    size_t      top, count;
} Scopes;

//  INSTRUCTIONS
typedef struct Instruction {
    int kind;
    union {
        struct {
            String ident;
        } load, store, call;

        struct {
            Object object;
        } push, leq, lgt, llt, lgte, llte;

        struct {
            size_t jumpto;
            String label; // never used in the actual vm, 
                          // exists for 2 assembler pass only.
        } ijif, jmp;
    } as;
} Instruction;

typedef struct {
    int kind;
    union {
        Instruction     instruction;
        Label           label;
    } as;
} LineResult;

typedef struct Instructions {
    Instruction* items;
    size_t count, capacity;
} Instructions;

//
// FUNCTIONS
//


// Duct Function is a handle with data on 
// function identity and memory pointer - under which
// all of the function data is stored.
//
// This way each function is self contained "piece" that
// can be treated as single unit which either initilized, or not.
typedef struct {
    String      id;
    String*     argv; size_t argc;
    size_t ip;

    // owned memory, it has everything that function needs,
    // packed tightly in linear block of memory.
    void* memory;
    // data section
    size_t  symbol_count; // total=(args + instruction ids) symbols
    // code section
    Instructions code;
} Function;

typedef struct {
    Function* items;
    size_t count, capacity;
} Functions;

typedef struct {
    String *items;
    size_t count, capacity, typesize;
} Strings;

// WM
typedef struct {
    unsigned int    config;

    // TODO: use arena allocator per stack frame
    //Arena           allocator; 
    Functions       functions;

    size_t          sp;
    Object          returned_object;
    Object          stack[DTVM_STACK_CAPACITY];
   
    Scopes          scopes;
    FILE            *tracef, *writef;
} Dtvm;

/*  FUNCTION DECLARATIONS */

// UTILITY
int string_to_integer       (String s);
float string_to_float       (String s);
bool string_is_text         (String s);
bool string_is_identifier   (String s);

String   string(const char* str);
bool     string_cmp(String l, String r);


// DT OBJECTS 
static inline Object object_byte(char c);
static inline Object object_int(int i);
static inline Object object_float(float f);


Object*     object_get  (ObjectMap* map, String id);
Object      object_store(Object* ref, Object value);
Object*     object_reserve_or_get(ObjectMap* map, String id);

// DT SCOPE(S)
ObjectMap*  dtvm_get_scope(Dtvm* vm);
void        dtvm_free_scope(ObjectMap* scope);

// VM INTERNALS
Function*   dtvm_get_function(Dtvm* vm, String fnid);
Object      dtvm_call(Dtvm* vm, String fnid, Object* argv, int argc);
void        dtvm_if(Dtvm* vm, Function* fn);
void        dtvm_push(Dtvm* vm, Object value);

// ASSEMBLER

// VM API
void dtvm_compile_from_asm(Dtvm* vm, String source, FILE* trace_file);

#endif//__DTVM_DEFINITIONS_H 
