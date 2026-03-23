#ifndef __DTVM_DEFINITIONS_H 
#define __DTVM_DEFINITIONS_H 

#include "../config.h"
#include "../shared/shared.h"
#include "../shared/hc_plug.h"

/*  DEFINES OR CONSTANTS    */
#define         DT_SIZE_T_INVALID                   ((size_t)-1)

/*  ENUMS   */

typedef enum {
    OT_NULL = 0, // should be an error if encountered. NULL's are bad.
    OT_BOOL     = 1,
    OT_BYTE     = 2,
    OT_CHAR     = 3,
    OT_INT      = 4,
    OT_FLOAT    = 5,
    OT_CHARACTER= 6,
    OT_STRING   = 7,
    OT_TYPE     = 8,
    OT_RESULT   = 10, 

    OT_IS_ARRAY     = (1<<4),
    OT_IS_LIST      = (1<<5),
    OT_IS_OBJECT    = (1<<6),
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
    ELINE_INSTRUCTION,
    ELINE_LABEL,
    ELINE_FUNCTION,
} ELineResult;

/* TYPEDEFS */
static String  STRING_NULL = {0};

// STRUCTS //

struct DtList  ;
struct DtArray ;
struct Object  ;

typedef struct {
    String  name;
    long    address;
} Label; 

typedef struct {
    Label* items;   
    size_t count, capacity, typesize;
} Labels;

typedef struct DtArray {
    void*  items;
    size_t count, capacity, typesize;
} DtArray;

typedef struct DtComplex {
    struct Object 
        *next, *prev, *tail,
        *children
    ;
} DtComplex;

// OBJECT(s)
typedef int Result;
typedef struct Object {
    String   id;
    byte    properties;
    int     type;
    union {
        bool            B;
        unsigned char   b;
        int             i;
        float           f;
        Result          r;
        String          s; // read only string
        DtArray         A;
        DtComplex       C;
        EObjectType     T; // object represents "Type"
    } as;
} Object;

typedef struct ObjectMap {
    Arena   local_allocator;  // temporary allocator for dynamic objects
    Object* items;
    MapHead map_head;
} ObjectMap;

typedef struct {
    size_t prev_pointer, count;
} ScopeStackFrame;

typedef struct {
    ObjectMap       buf             [DTVM_SCOPES];
    ScopeStackFrame object_frames   [DTVM_SCOPES];
    size_t      top, count;
} Scopes;

//  INSTRUCTIONS
typedef struct Instruction {
    int kind;
    union {
        struct {
            String ident;
            size_t id;
        } load, store, call;

        struct {
            Object object;
        } push, leq, lgt, llt, lgte, llte;

        struct {
            size_t jumpto;
            String label; // never used in the actual vm, 
                          // exists strictly for second assembler pass.
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
// can be treated as single unit which either is valid, or not.
typedef struct {
    String      id;
    String*     argv; size_t argc;

    // owned memory, it has everything that function needs,
    // packed tightly in linear block of memory.
    void* memory;
    // data section
    size_t  symbol_count; // total=(args + instruction ids) symbols
    size_t  variable_symbols;
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

// VM
typedef struct {
    int             options_mirror; // mirror of options from interpreter.

    // TODO: use arena allocator per stack frame
    //Arena           allocator; 
    Functions       functions;

    size_t          top;
    size_t          work_sp, data_sp;
    Object          returned_object;
    Object          work_stack[DTVM_STACK_CAPACITY];
    Object          data_stack[DTVM_STACK_CAPACITY];
   
    Scopes          scopes;
    FILE            *logf, *asmtracef, *tracef, *writef;
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
static inline Object object_string(String s);

Object*     object_get  (Dtvm* map, size_t id);
Object      object_store(Object* ref, Object value);
Object*     object_reserve_or_get(ObjectMap* map, String id);

// DT SCOPE(S)
ObjectMap*  dtvm_get_scope(Dtvm* vm);
void        dtvm_free_scope(ObjectMap* scope);

// VM INTERNALS
//
void        dtvm_trace_execution(Dtvm* vm, Instruction inst, size_t ip);
Function*   dtvm_get_function(Dtvm* vm, String fnid);
Object      dtvm_call(Dtvm* vm, String fnid, Object* argv, int argc, size_t datasp);
void        dtvm_if(Dtvm* vm, Function* fn);
void        dtvm_push(Dtvm* vm, Object value);

// ASSEMBLER

#define dta_trace(tracef, ...) if(tracef) {\
    fprintf(tracef, __VA_ARGS__);\
}


// VM API
void dtvm_compile_from_asm(Dtvm* vm, String source);

#endif//__DTVM_DEFINITIONS_H 
