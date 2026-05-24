#ifndef __DT_COMPILER_DECLARATIONS
#define __DT_COMPILER_DECLARATIONS

#include "../config.h"
#include "../shared/shared.h"

/*
 * Handy C Header glue 
 */
#include "../shared/hc_plug.h"

/*
 *  TOKENIZER
 */

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

#define TEMP_CSTR_LENGTH DT_SCRATCH_BUFFER_SIZE
#ifndef TOKENIZER_TOKEN_INITIAL_COUNT
#	define TOKENIZER_TOKEN_INITIAL_COUNT 32
#endif


typedef struct { 
	const char* data;
	size_t 	    length;
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
    Tokens              tokens;
} Tokenizer;



//
// PARSER
//


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
    TI_LIST_OPERATOR,
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

// 
// Compile-time analysis.
//

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

typedef enum {
    DT_PNKIND_ERROR,
    DT_PNKIND_OK,
    DT_PNKIND_VALUE,
} DtParseNodeKind;

typedef struct {
    unsigned type;
    union {
        byte b;
        char c;
        int i;
        long l;
        float f;
        double d;
    } as;
} DtAtomValue;

typedef struct {
    bool negative_sign;
    bool foldable_constant;
    DtAtomValue atom;
} DtParseValue;

typedef struct {
    DtParseNodeKind     kind;   
    size_t              instructions_generated;
    union {
        DtParseValue    value;
    } as;
} DtParseResult;



bool dtp_validate_function(DtParserFunctions* map, DtParserFunctionSymbol s);
void dtp_free_function_symbols(DtParserFunctions* s);
DtParserFunctions dtp_init_function_symbols(void);



typedef struct {
    int             options_mirror; // mirror of options from interpreter.
    StringBuilder   output;
    FILE*           tracef; // for tracing AST parsing in recursion.
    FILE*           errorf; // for logging errors from compilation.

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




bool            dtp_is_ok(DtParseResult r);
DtParseResult   dtp_ok(void);
DtParseResult   dtp_error(void);

#endif  //_DT_COMPILER_DECLARATIONS
