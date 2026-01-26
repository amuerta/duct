#ifndef __DT_COMPILER_DECLARATIONS
#define __DT_COMPILER_DECLARATIONS


/*
 * Handy C Header glue 
 */

#define HC_DA_MACRO_BASED
#include "../shared/hc.h"

#define da_append hc_da_append
#define IGNORE_VALUE (void)

#define min hc_min
#define max hc_max


/*
 *  TOKENIZER
 */

#define WIDE_SYMBOL_SZ 4
#define INVALID_INDEX (size_t)(-1)

#ifndef HC_DEFINITION_TYPE_INDEX_T
#define HC_DEFINITION_TYPE_INDEX_T
    typedef size_t              index_t;
#endif//HC_DEFINITION_TYPE_INDEX_T


typedef unsigned char 	symbol_t;
typedef unsigned int	wsymbol_t;
typedef const char*		cstr_t;
typedef unsigned int	tokenid_t;

#define MUTCSTR(s) ((char*)(s))


// TODO: remove this nonsense
#define TOKENIZER_TOKEN_FMT_CONTENT "$"

#ifndef TOKENIZER_TOKEN_FMT_CONTENT
#	define TOKENIZER_TOKEN_FMT_CONTENT "text: "
#endif
#ifndef TOKENIZER_TOKEN_FMT_KIND
#	define TOKENIZER_TOKEN_FMT_KIND "kind: "
#endif
#ifndef TOKENIZER_TOKEN_FMT_ID
#	define TOKENIZER_TOKEN_FMT_ID "id: "
#endif

#ifndef TEMP_CSTR_LENGTH 
#	define TEMP_CSTR_LENGTH 128
#endif

#ifndef TOKENIZER_TOKEN_INITIAL_COUNT
#	define TOKENIZER_TOKEN_INITIAL_COUNT 32
#endif


typedef struct { 
	const char* 	data;
	size_t 	length;
} TknSlice;

typedef struct {
	const char* 	txt;
	tokenid_t		id;
} TokenTableEntry;

typedef enum {
	TOKEN_KIND_NULL,
	TOKEN_KIND_SYMBOL,
	TOKEN_KIND_WORD,
	TOKEN_KIND_LITERALL_INTEGER,
	TOKEN_KIND_LITERALL_FLOAT,
	TOKEN_KIND_LITERALL_STRING,
    TOKEN_KIND_COMMENT_BLOCK,
	TOKEN_KIND_EOL,
	TOKEN_KIND_EOF,
} TokenKind;

typedef struct {
    size_t 
        row, 
        col;

	TokenKind 	kind;
	tokenid_t	id;
	union {
		symbol_t 	as_symbol;
		TknSlice	as_word;
		int			as_int;
		float		as_float;
		bool		as_bool;
	}			data;
} Token;

typedef struct {
    Token*  items;
    size_t  count;
    size_t  capacity;
} Tokens;

typedef struct {
	char*		scratch_buffer;
    size_t		scratch_buffer_sz;

    // config or user options
    bool                skip_newline;
    symbol_t 			string_quotes[2];
    const char*         comment_block[2];

    // line tracking
    size_t              row;
    size_t              col;
	
    // string info
    const char* 		target;
	size_t				target_length;
	size_t 				position;

    // user input table
	TokenTableEntry*	token_table;
	size_t				token_table_count;

    // memory for linear allocation of tokens
    // arena kind of
	Tokens		        tokens;
} Tokenizer;



//
// PARSER
//

#define PARSER_TOKEN_BUFFER_SIZE    8
#define PARSER_LOOK_AHEAD_COUNT     4

typedef enum {
    TI_NULL,
    TI_RANGE,
    TI_DOT,
    TI_COMMA,
    TI_PAREN_O,
    TI_PAREN_C,
    TI_SBRACK_O,
    TI_SBRACK_C,
    TI_CURLY_O,
    TI_CURLY_C,
    TI_COLON,
    TI_SEMICOLON,
    TI_QMARK,
    TI_EMARK,
    TI_MINUS,
    TI_PLUS,
    TI_MUL,
    TI_DIV,
    TI_EQUAL,
    TI_COMPARISON_EQUAL,
    TI_COMPARISON_NOT_EQUAL,
    TI_COMPARISON_GREATER,
    TI_COMPARISON_LESS,
    TI_COMPARISON_GREATER_EQUAL,
    TI_COMPARISON_LESS_EQUAL,
} TokenId;

typedef enum {
    NKP_IS_ADD      = 1,
    NKP_IS_MUL      = 2,
    NKP_HAS_UNARY   = 4,
    NKP_IS_EQALITY  = 8,
    NKP_IS_CMP_EQ   = 16,
    NKP_IS_CMP_GT   = 32,
} DtNodeKindProperties;

typedef char            byte;
typedef unsigned char   ubyte;

// TODO: make this slice one and only slice in this project
// TL;DR: remove TknSlice struct
typedef struct {
    const char*   data; 
    size_t  length;
} Slice;

typedef struct {
    const char**    indent;
    size_t          count;
    size_t          capacity;
} TempIndentsList;

typedef struct {
    byte    type;
    byte    properties;
    Slice   identifier;
    union {
        bool        as_bool;
        byte        as_byte;
        int         as_int;
        float       as_float;
        int         as_type;
        Slice       as_slice;
    } memory;
} DtLiterall;

// TODO?: use union to make DtNode reuseable for both interpreting and parsing
// by orginizing them by mode of operation
typedef struct DtNode {
    Token           source_location;// for error checking

    // DtNodeKind      kind;
    int             properties; // is term add or subtract?
    int             type;
    Token           identifier;

    struct DtNode*  next;
    struct DtNode*  children;
} DtNode;


// Analysis

enum {
    DT_PS_NONE, // error, probably.
    DT_PS_FUNCTION_SIGNATURE,
    DT_PS_VARIABLE,
};

typedef struct {
    String  name;
    size_t  argc;
    bool    has_return_value;
    bool    already_seen;
    bool    used_in_expression;
} DtParserFunctionSymbol;

typedef struct {
    DtParserFunctionSymbol *items;
    MapHead                 map_head;
} DtParserFunctions;

#define DTP_FUNCTION_SYMBOLS_STARTING_CAPACITY 256


bool dtp_validate_function(DtParserFunctions* map, DtParserFunctionSymbol s);
void dtp_free_function_symbols(DtParserFunctions* s);
DtParserFunctions dtp_init_function_symbols(void);



typedef struct {
    StringBuilder   output;
    FILE*           tracef; // same as in `DtInterpreter`
    FILE*           errorf; // same as in `DtInterpreter`

    size_t      current;
    size_t      requested_tokens,
                parsed_tokens;
    // we buffer PTBS(size) of tokens to be able 
    // to lookup tokens before and after the current one
    Tokenizer   lexer;
    Token       current_token;
    Token       token_buffer
                    [PARSER_TOKEN_BUFFER_SIZE];
    
    DtParserFunctions   function_symbols;
    Arena               allocator;
} DtParser;





#ifdef PARSER_TRACE
#define dtp_trace_recursion(p, name, depth, ...) if(p->tracef) {\
        fprintf(p->tracef, "%*.s> %s", depth, "\t", name);\
        fprintf(p->tracef, ""__VA_ARGS__);\
        fprintf(p->tracef, "\n");\
}
#else
#define dtc_trace_recursion(name,depth, ...) /* __VA_ARGS__ */
#endif



typedef enum {
    DT_PNKIND_ERROR,
    DT_PNKIND_OK,
    DT_PNKIND_VALUE,
} DtParseNodeKind;

typedef struct {
    bool negative_sign;
    bool foldable_constant;
} DtParseValue;

typedef struct {
    DtParseNodeKind     kind;   
    size_t              instructions_generated;
    union {
        DtParseValue    value;
    } as;
} DtParseResult;

DtParseResult dtp_error(void) {
    const DtParseResult r = {0}; return r;
}

bool dtp_is_ok(DtParseResult r) { return r.kind >= DT_PNKIND_OK; };

DtParseResult dtp_ok(void) {
    const DtParseResult r = {.kind = DT_PNKIND_OK }; return r;
}

#define dtp_ok_or_return(e)\
    if (!dtp_is_ok(e)) return e; 



String string_alloc_to_arena(Arena* allocator, String it);



#endif  //_DT_COMPILER_DECLARATIONS
