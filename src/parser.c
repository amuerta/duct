#include <errno.h>
#include "common.c"

// TODO:
// write tree treveral error printer
// noisy but easy to find the error

//
//  TOKENIZER
//
#ifndef __TOKENIZER_H
#define __TOKENIZER_H


#define WIDE_SYMBOL_SZ 4

typedef unsigned char 	symbol_t;
typedef size_t          index_t;
typedef unsigned int	wsymbol_t;
typedef const char*		cstr_t;
typedef unsigned int	tokenid_t;

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
	TokenKind_null,
	TokenKind_symbol,
	TokenKind_word,
	TokenKind_literall_integer,
	TokenKind_literall_float,
	TokenKind_literall_string,
    TokenKind_comment_block,
	TokenKind_EOL,
	TokenKind_EOF,
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


void Tokenizer_init(
		Tokenizer* 			t, 
		TokenTableEntry* 	table,
		size_t				table_len,
		const char*			target,
		size_t				target_len,
		char*				scratch_buffer,
		symbol_t			quotations[2],
        const char*         comment_block[2],
        bool                skip_nl
    ) 
{
	assert(t && "Tokenizer pointer has to be valid");

	*t = (Tokenizer) {
		.scratch_buffer = scratch_buffer,
		.target 		= target,
		.target_length 	= target_len,
		.string_quotes 	= { quotations[0],      quotations[1]       },
        .comment_block  = { comment_block[0],   comment_block[1]    },
		.tokens         = {0},
        .token_table	= table,
		.token_table_count
						= table_len,
        .skip_newline  = skip_nl
	};

}

void Tokenizer_clear(Tokenizer* t) {
	assert(t && "Tokenizer has to be valid");
	memset(t->tokens.items, 0, sizeof(Token)*t->tokens.count);
    t->tokens.count = 0;
}

void Tokenizer_free(Tokenizer* t) {
	assert(t && "Tokenizer has to be valid");
    free(t->tokens.items);
    t->tokens.count = 0;
    t->tokens.capacity = 32; // default capacity
}



bool Token_compare_cstr(Token t, const char* cstr) {
    const size_t len = strlen(cstr);
    return (t.kind == TokenKind_word || t.kind == TokenKind_literall_string ) &&
        t.data.as_word.length == len &&
        (strncmp(t.data.as_word.data,cstr, len) == 0)
    ;
}

bool __tkn_cmp_from(
		TknSlice 	s, 
		const char* src, 
		size_t 		space_left)
{
	bool overflow = s.length > space_left; // "Attempt to overflow src"
	return !overflow && (strncmp(s.data,src,s.length) == 0);
}


index_t __tkn_match_table(Tokenizer t) {
	index_t token_i = INVALID_INDEX;
	const size_t space_left = t.target_length - t.position;
	const char* now = t.target + t.position;
	loop(i, t.token_table_count) {
		TokenTableEntry entry = t.token_table[i];
		TknSlice slice = {
			MUTCSTR(entry.txt) ,
			strlen(entry.txt)
		};

		if (__tkn_cmp_from(slice, now, space_left)) {
			token_i = i;
			break;
		}
	}
	return token_i;
}

unsigned int stoi(const char* str) {
	unsigned int i = 0;
	memcpy(&i,str,MIN(sizeof(int),strlen(str)));
	return i;
}

bool __is_eol(unsigned char c) {
	return (c == '\n');
}

bool __is_space(unsigned char c) {
	return (c == '\t') || (c == ' ');
}

bool __is_character(unsigned char c) {
	return 
		((c >= 'A') && (c<='Z')) ||
		((c >= 'a') && (c<='z')) ||
		c == '_';
}

bool __is_decimal(unsigned char c) {
	return 
		((c>='0')&&(c<='9'));
}



Token __tkn_get_word(Tokenizer t, size_t* step_sz) {
	const char* word = (t.target + t.position);
	Token result = {
		.kind = TokenKind_word,
		.id = 0,
		.data.as_word = {
			.data = MUTCSTR(word),
			.length = 0
		}
	};

	for(size_t i = t.position; i < t.target_length; i++) {
		const char  symbol = *word;
		if (__is_character(symbol) || __is_decimal(symbol)) 
			result.data.as_word.length++;
		else
			break;
		
		word++;
		(*step_sz)++;
	}

	return result;
}

Token __tkn_get_number(Tokenizer t, size_t* step_sz) {
	static char scratch[TEMP_CSTR_LENGTH];
	const char* word = (t.target + t.position);
	Token result = {
		.id = 0,
		.data.as_word = {
			.data 	= MUTCSTR(word),
			.length = 0
		}
	};
	size_t dot_count = 0;

	for(size_t i = t.position; i < t.target_length; i++) {
		const char  symbol = *word;
		bool is_only_dot = (symbol == '.' && dot_count <= 1);
		if (__is_decimal(symbol) || is_only_dot) {
			result.data.as_word.length++;
			if (symbol == '.')
				dot_count++;
		}
		else
			break;
		
		word++;
		(*step_sz)++;
	}
	// decide if float or not
	// perform conversion
	memset(scratch,0,TEMP_CSTR_LENGTH);
	strncpy(scratch, 
			result.data.as_word.data,
			result.data.as_word.length
	);
	result = (Token) {0};

	if (dot_count > 0) {
		result.kind = TokenKind_literall_float;
		result.data.as_float = atof(scratch);
	} else {
		result.kind = TokenKind_literall_integer;
		result.data.as_int = atoi(scratch);
	}

	return result;
}


Token __tkn_get_string_literall(Tokenizer t, symbol_t quotations[2], size_t* step_sz) {
	const size_t QOPEN	= 0;
	const size_t QCLOSE = 1;
	const char* word = (t.target + t.position);
	bool inside_string	= false;
	Token result = {
		.id = 0,
		.kind = TokenKind_literall_string,
		.data.as_word = {
			.data 	= MUTCSTR(word+1),
			.length = 0
		}
	};

	for(size_t i = t.position; i < t.target_length; i++) {
		
		const char  symbol 		= *word;
		const char  next_symbol = (i+1 < t.target_length)?  *(word+1) : 0;
		bool is_qopen 	= (symbol == quotations[QOPEN]);
		bool is_qclose  = (symbol == quotations[QCLOSE]);
		bool is_backslh = (symbol == '\\');

		if (is_qopen && !inside_string)  {
			// skip starting quote for string content
			word++;
			inside_string = true;
			continue;
		}
		else if (is_qclose) {
			break;
		}

		else if (is_backslh)
			if (next_symbol == quotations[QCLOSE]) {
				word+=2;
				(*step_sz)+=2;
				result.data.as_word.length+=2;
				continue;
			}

		result.data.as_word.length++;
		word++;
		(*step_sz)++;
	}

	// rewind by two to skip ending character and quote to exit string properly
	(*step_sz)+=2;

	return result;
}

typedef enum {
	TokenizerPrintFlag_wrap_in_curly 	= 1,
	TokenizerPrintFlag_display_text 	= 2,
	TokenizerPrintFlag_display_kind		= 4,
	TokenizerPrintFlag_display_id	 	= 8,
	TokenizerPrintFlag_use_tabs			= 16
} TokenizerPrintFlag;

typedef unsigned char bitmask8_t;

const char* __tkn_temp_cstr(Token t, bitmask8_t print_flags) {
	static char token	[TEMP_CSTR_LENGTH*3];
	static char cstr	[TEMP_CSTR_LENGTH];
	static char number	[TEMP_CSTR_LENGTH];

	// reset
	memset(token,0,TEMP_CSTR_LENGTH);
	memset(cstr,0,TEMP_CSTR_LENGTH);
	memset(number,0,TEMP_CSTR_LENGTH);

	switch(t.kind) {
		case TokenKind_null:
			strcpy(cstr, "(null)"); 
			break;
		case TokenKind_symbol:
			sprintf(cstr, "'%c'", t.data.as_symbol); 
			break;
		case TokenKind_word:
			strncpy(cstr, t.data.as_word.data, t.data.as_word.length);
			break;
		case TokenKind_literall_integer:
			snprintf(cstr,
					TEMP_CSTR_LENGTH,
					"%i",t.data.as_int);
			break;
		case TokenKind_literall_float:
			snprintf(cstr,
					TEMP_CSTR_LENGTH,
					"%f",t.data.as_float);
			break;
		case TokenKind_literall_string:
			{
				strcat(cstr,"\"");
				strncat(cstr, t.data.as_word.data, MIN(t.data.as_word.length,TEMP_CSTR_LENGTH-2));
				strcat(cstr,"\"");
			}
			break;
		case TokenKind_EOL:
			sprintf(cstr, "(EOL)");
			break;
		case TokenKind_EOF:
			sprintf(cstr, "!(EOF)!");
			break;
		default:
			sprintf(cstr, "UKNOWN");
	}

	const size_t half_capacity		= TEMP_CSTR_LENGTH/2;
	//const size_t capacity			= TEMP_CSTR_LENGTH;
	//const size_t double_capacity 	= TEMP_CSTR_LENGTH*2;
	const size_t triple_capacity	= TEMP_CSTR_LENGTH*3;

	const bool 
		use_curly = print_flags & TokenizerPrintFlag_wrap_in_curly,
		show_text = print_flags & TokenizerPrintFlag_display_text,
		show_kind = print_flags & TokenizerPrintFlag_display_kind,
		show_id   = print_flags & TokenizerPrintFlag_display_id,
		show_tab  = print_flags & TokenizerPrintFlag_use_tabs
	;

	char* kind_str = number;
	char* id_str = number + half_capacity;
	
	snprintf(kind_str, half_capacity, "%i",t.kind);
	snprintf(id_str, half_capacity, "%i",t.id);
	
	snprintf(token,triple_capacity,"%s %s%s%s%s%s%s%s%s %s",
			(use_curly) ? "{" : "",

			(show_text) ? 	TOKENIZER_TOKEN_FMT_CONTENT : "",	
			(show_text) ? 	cstr : "",

			(show_tab)	?	"\t" : "",
			(show_kind) ?	TOKENIZER_TOKEN_FMT_KIND : "",		
			(show_kind) ?	kind_str : "",
			
			(show_tab)	?	"\t" : "",
			(show_id)	?	TOKENIZER_TOKEN_FMT_ID : "",			
			(show_id)	?	id_str : "",
			
			(use_curly) ? "}" : ""
	);
	return token;
}

const char* Token_temp_cstr(Token t)  {
	return __tkn_temp_cstr(t,0xFF);
}

const char* Token_text_cstr(Token t)  {
	return __tkn_temp_cstr(t,TokenizerPrintFlag_display_text);
}

Token Tokenizer_next_token(Tokenizer* t) {
	Token 	result = {0};
	index_t table_i = INVALID_INDEX;
	size_t 	step_size = 0;

    enum { 
        NO_COMMENT,
        BLOCK_COMMENT,
        LINE_COMMENT
    };

    static int comment = NO_COMMENT;
    const char* cb_open = t->comment_block[0];
    const char* cb_close = t->comment_block[1];


retry:;
	const char* leftover = (t->target + t->position);
	const symbol_t current_symbol = *leftover;
    
// TODO: implement short-circuiting for this to improve performance of parsing
    //bool is_bc_char = cb_close[0] == current_symbol;
    bool is_bc_str = strncmp(leftover, cb_close, strlen(cb_close)) == 0;

	if(t->position < t->target_length) {
		table_i = __tkn_match_table(*t);

		//printf("\nmatching: {%s}", leftover);

        if (comment) {
            
            if(!is_bc_str)  {
                t->position += 1;
                goto retry;
            }

            t->position += strlen(cb_close);
            comment = NO_COMMENT;
            goto retry;
        }

        else if (strncmp(leftover, cb_open, strlen(cb_open)) == 0) {
            t->position += strlen(cb_open);
            comment = BLOCK_COMMENT;
            goto retry;
        }

        // symbol/keyword exists in tokenization table
        else if (table_i != INVALID_INDEX) {
			TokenTableEntry e = t->token_table[table_i];
			size_t			esz = strlen(e.txt);
			
			result.id = e.id;
			result.kind = esz > 1 
				? TokenKind_word : TokenKind_symbol;
			
			if (esz > 1) {
				result.kind = TokenKind_word;
				result.data.as_word = (TknSlice) {
                    .data = e.txt,
                    .length = esz
                };
			} else {
				result.kind = TokenKind_symbol;
				result.data.as_symbol = e.txt[0];
			}

			t->position += esz;
            t->col += esz;
		}


		else if (__is_character(current_symbol)) 
			result = __tkn_get_word(*t,&step_size);

		else if (__is_decimal(current_symbol)) 
			result = __tkn_get_number(*t,&step_size);
	
		else if (current_symbol == t->string_quotes[0]) {
			result = __tkn_get_string_literall(*t,t->string_quotes,&step_size);
		}

		else if (__is_eol(current_symbol)) {
			result.kind = TokenKind_EOL;
			t->position++;
            t->row++;
            t->col = 0;
            if (t->skip_newline) goto retry;
		}

		else if (__is_space(current_symbol)) {
			// skip until symbol is not space
			// if it overflow it means we reached the end
			for(size_t i = t->position; i <= t->target_length; i++)
				if (!__is_space(t->target[i])) {
					t->position += (i - t->position);
					goto retry;
				}	
		}
        else 
            // TODO:
            assert(0 && "Uknown to the tokenizer symbol");
            //result = (Token) {0};
        

        t->col += step_size;
		t->position += step_size;

        result.col = t->col;
        result.row = t->row;
		step_size = 0; // reset each time
	}

	// if we exausted a target string
	// return EOF
	else
		result.kind = TokenKind_EOF;

	return result;
}

// run tokenizer and save everything into growable stack
void Tokenizer_run(Tokenizer* t) {
	Token token = {0};
	Tokens* s = &(t->tokens);
	while( (token = Tokenizer_next_token(t)).kind != TokenKind_EOF ) {
		da_append(s,token);
	}
}

#endif

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
    NK_NULL,
    NK_SPLITTER,
    NK_ROOT,
    NK_FUNCTION_DECL,
    NK_FUNCTION_CALL,
    NK_FUNCTION_ARGS,
    NK_BLOCK,
    NK_SYMBOL_DECL,
    NK_STATEMENT,
    NK_IF_STATEMENT,
    NK_ELSEIFS,
    NK_IF,
    NK_ELSE,
    NK_ELSEIF,
    NK_RETURN,
    NK_EXPRESSION,
    NK_EQALITY,
    NK_COMPARISON,
    NK_TERM,
    NK_FACTOR,
    NK_PRIMARY,
    NK_RVALUE,
    NK_VARIABLE,
    NK_ARRAY,
    NK_OBJECT,
    NK_TYPE,
    NK_BOOLIT,
    NK_INTLIT,
    NK_FLTLIT,
    NK_CHRLIT,
    NK_STRLIT,
    NK_IDENTIFIER,
} DtNodeKind;

typedef enum {
    NKP_IS_ADD      = 1,
    NKP_IS_MUL      = 2,
    NKP_HAS_UNARY   = 4,
    NKP_IS_EQALITY  = 8,
    NKP_IS_CMP_EQ   = 16,
    NKP_IS_CMP_GT   = 32,
} DtNodeKindProperties;

const char* DT_NODE_KIND_STR[] = {
    [NK_NULL]                  = "(NULL)",
    [NK_SPLITTER]              = "--- split ---",
    [NK_ROOT]                  = "root",
    [NK_FUNCTION_DECL]         = "function decl",
    [NK_FUNCTION_CALL]         = "function call",
    [NK_FUNCTION_ARGS]         = "function args",
    [NK_SYMBOL_DECL]           = "symbol decl",
    [NK_STATEMENT]             = "statement",
    
    [NK_IF_STATEMENT]          = "if statement",
    [NK_IF]                    = "if",
    [NK_ELSE]                  = "else",
    [NK_ELSEIF]                = "elseif",
    [NK_ELSEIFS]               = "elseif-list",
    [NK_RETURN]                = "return",
    [NK_BLOCK]                 = "block",

    [NK_EQALITY]               = "equality",
    [NK_COMPARISON]            = "comparison",
    [NK_TERM]                  = "term",
    [NK_FACTOR]                = "factor",
    [NK_PRIMARY]               = "primary",
    [NK_EXPRESSION]            = "expression",

    [NK_TYPE]                  = "type",
    [NK_VARIABLE]              = "variable",
    [NK_RVALUE]                = "rvalue",
    [NK_ARRAY]                 = "array",
    [NK_OBJECT]                = "object",
    [NK_IDENTIFIER]            = "identifier",
    
    [NK_BOOLIT]                = "boolean",
    [NK_INTLIT]                = "int",
    [NK_FLTLIT]                = "float",
    [NK_CHRLIT]                = "char",
    [NK_STRLIT]                = "string",
};

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

    DtNodeKind      kind;
    int             properties; // is term add or subtract?
    int             type;
    Token           identifier;

    struct DtNode*  next;
    struct DtNode*  children;
} DtNode;

typedef struct {
    const char** items;
    size_t       count;
    size_t       capacity;
} DtStrings;

typedef struct {
    const char*     file_name;
    const char*     source;
    bool            abort;
    bool            recorded_error;
    int             error_count;
    int             warning_count;
    StringBuilder   error_builder;
    DtStrings       errors;
} DtStatus;

typedef struct {
    DtStatus*   stat;

    size_t      current;
    size_t      requested_tokens,
                parsed_tokens;
    // we buffer PTBS(size) of tokens to be able 
    // to lookup tokens before and after the current one
    Tokenizer   lexer;
    Token       current_token;
    Token       token_buffer
                    [PARSER_TOKEN_BUFFER_SIZE];
    
    DtNode*         root;
    Arena           nodes;
} DtParser;

// since i use my generic tokenizer here,
// i need to initilize Duct tokenizer 
// with specific to it parameters.
Tokenizer DtTokenizer_init(const char* text, size_t len) {
    Tokenizer lexer = {0};
    static char scratch_buffer[512];

    static TokenTableEntry token_table[] = {
        { "==",  TI_COMPARISON_EQUAL},
        { "!=",  TI_COMPARISON_NOT_EQUAL},
        { ">=",  TI_COMPARISON_GREATER_EQUAL},
        { "<=",  TI_COMPARISON_LESS_EQUAL},
        { "..",  TI_RANGE },
        { "." ,  TI_DOT },
        { "," ,  TI_COMMA },
        { "(", 	 TI_PAREN_O},
        { ")", 	 TI_PAREN_C},
        { "[", 	 TI_SBRACK_O},
        { "]", 	 TI_SBRACK_C},
        { "{", 	 TI_CURLY_O},
        { "}", 	 TI_CURLY_C},
        { ":" ,  TI_COLON },
        { ";" ,  TI_SEMICOLON },
        { "?" ,  TI_QMARK},
        { "!" ,  TI_EMARK},
        { "-" ,	 TI_MINUS},
        { "+" ,	 TI_PLUS},
        { "*" ,	 TI_MUL},
        { "/" ,	 TI_DIV},
        { "=" ,	 TI_EQUAL},
        { ">",   TI_COMPARISON_GREATER},
        { "<",   TI_COMPARISON_LESS},
    };

	Tokenizer_init(&lexer,
			token_table,
			DT_ARRLEN(token_table),
			text,
			len,
			scratch_buffer,
			(symbol_t[2]) { '"', '"' },
            (const char* [2]) { "/*", "*/" },
            true // TODO: check new lines places
	);
    lexer.row = 1;
    return lexer;
}

//
// FILE / FILES / IO
//

// TODO: make this funtion work correctly on any suppoerted os (win/linux/android/mac/etc)
char* dt_load_file(const char* path) {
    static char empty_string[64];
    size_t len = 0;
    FILE* file = 0;
    file = fopen(path, "r+");

    if (!file) return 0;

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    rewind(file);

    if (len == 0) {
        fclose(file);
        return empty_string;
    }

    char* content = calloc(len + 1, 1);
    fread(content, 1, len-1, file);

    fclose(file);
    return content;
}

//
// PARSER / LEXER / LEX / PAR
//
void dtp_print_token_buffer(DtParser p) {
    printf("current: %lu\t", p.current);
    printf("buffer: [ ");
    loop(i, PARSER_TOKEN_BUFFER_SIZE) 
        printf("%i ", p.token_buffer[i].kind);
    printf("] ");
 
}

void dtp_load_tokens_buffer(DtParser* p) {
    Tokenizer* lexer = &(p->lexer);
    // Buffer tokens in batches of set size
    // so that its easy to check previous 
    if (p->parsed_tokens == 0) {
        loop(i, PARSER_TOKEN_BUFFER_SIZE) {
            p->token_buffer[p->parsed_tokens++] = 
                Tokenizer_next_token(lexer);
        }
    } else if (p->requested_tokens + PARSER_LOOK_AHEAD_COUNT > p->parsed_tokens) {
        loop(i,PARSER_LOOK_AHEAD_COUNT)
            p->token_buffer[(p->parsed_tokens++) % PARSER_TOKEN_BUFFER_SIZE] = 
                Tokenizer_next_token(lexer);
    } 
}

Token dtp_step(DtParser* p) {
    dtp_load_tokens_buffer(p);
    p->current = p->requested_tokens;
    Token tk = p->token_buffer[(p->requested_tokens++) % PARSER_TOKEN_BUFFER_SIZE];
    p->current_token = tk;
    return tk;
}

Token dtp_back(DtParser* p) {
    if (p->parsed_tokens == 0) return p->token_buffer[p->parsed_tokens-1];
    Token tk = p->token_buffer[(p->requested_tokens--) % PARSER_TOKEN_BUFFER_SIZE];
    p->current_token = tk;
    return tk;
}

Token dtp_before(DtParser* p) {
    return p->token_buffer[(p->current-1) % PARSER_TOKEN_BUFFER_SIZE];
}

Token dtp_ahead(DtParser* p) {
    return p->token_buffer[(p->current+1) % PARSER_TOKEN_BUFFER_SIZE];
}

Token dtp_aheadc(DtParser* p, int step) {
    return p->token_buffer[(p->current+step) % PARSER_TOKEN_BUFFER_SIZE];
}

Token dtp_beforec(DtParser* p, int step) {
    return p->token_buffer[(p->current-step) % PARSER_TOKEN_BUFFER_SIZE];
}

bool dtp_have_tokens(DtParser* p) {
    return p->current_token.kind != TokenKind_EOF;
}



bool dtp_valid_token(Token t) {
    return t.kind != TokenKind_null && t.kind != TokenKind_EOF;
}

bool dtp_match_kind(Token t, TokenKind kind) {
    return 
        dtp_valid_token(t) &&
        t.kind == kind;
}

//#define DT_TOKEN_DEBUG

#ifdef DT_TOKEN_DEBUG
    #define token_dump_str(T) printf("T: %.*s\n", (int) T.data.as_word.length, T.data.as_word.data)
    #define token_dump_sym(T) printf("T: %c\n", T.data.as_symbol)
#else 
    #define token_dump_str(T) /* nothing */
    #define token_dump_sym(T) /* nothing */
#endif

bool dtp_match_str(Token t, const char* text) {
    token_dump_str(t);
    return 
        dtp_valid_token(t) &&
        t.kind == TokenKind_word && 
        t.data.as_word.data &&
        strlen(text) == t.data.as_word.length &&
        strncmp(t.data.as_word.data, text, t.data.as_word.length) == 0;
}

bool dtp_match_sym(Token t,char sym) {
    token_dump_sym(t);
    return 
        dtp_valid_token(t) &&
        t.kind == TokenKind_symbol && 
        t.data.as_symbol == sym;
}

bool is_space(char c) {
    return c == '\t' || c == ' ';
}

Slice dtp_get_line(const char* source, int r) {
    const char* line_end = 0;
    int newl_count = 1;

    while(newl_count != r && *source != 0) {
        if (*source == '\n') newl_count++;
        source++;
    }
 
    // trim left
    while(is_space(*source))
        source++;

    line_end = source;
    while(*line_end != '\n' && *line_end != 0)
        line_end++;

    Slice s = {
        .length = (line_end - source),
        .data = (char*) source
    }; 
    return s;
}

void dtp_error_message(DtStatus* stat, const char* message, int r, int c) {
    stat->abort = true;
    StringBuilder* sb = &(stat->error_builder);
    //Slice line = dtp_get_line(stat->source, r);
    /*
    sb_append(sb, 
            "\n"
            "%s:%i:%i error: %s"    "\n"
            "%10d | " "%.*s"        "\n" 
            "%10s | %*s%s"          "\n"
            ,

            stat->file_name, 
            r, 
            c, message,

               r,
               (int)line.length,
               line.data,
            "",
            c, "", "^ this line"
            );
    */

    sb_append(sb, 
            "\n""%s:%i:%i error: %s",
            stat->file_name, 
            r, 
            c, message
        );
}

void dtp_error(DtParser* p, const char* message) {
    assert(0);
    dtp_error_message(p->stat, message, p->lexer.row, p->lexer.col);
}


void dtp_error_push(DtParser* p) {
    assert(0);
    StringBuilder* sb = &(p->stat->error_builder);
    DtStrings* arr = &(p->stat->errors);
    const char* string = sb_collect(sb, true);
    da_append(arr, string);
}

void dtp_error_token(DtParser *p, Token t, const char* message) {
    assert(0);
    dtp_error_message(p->stat, message, t.row, t.col); //!dtp_valid_token(t));
}

#define dtp_expect_str(P,T,S,E) if (!dtp_match_str(T, S)) {\
    assert(0);\
    dtp_error_token(P, T, E);\
    return NULL;\
}

#define dtp_expect_kind(P,T,K,E) if (!dtp_match_kind(T, K)) {\
    assert(0);\
    dtp_error_token(P, T, E);\
    return NULL;\
}
    
#define dtp_expect_sym(P,T,S,E) if (!dtp_match_sym(T, S)) {\
    assert(0);\
    dtp_error_token(P, T, E);\
    return NULL;\
}

#define dtp_abort(P, E) do { dtp_error(P, E); return NULL; } while(0)

#if 0
void dtp_expect_kind(DtParser* p, Token t, int kind, const char* error) {
    bool match = dtp_match_kind(t, kind);
    if (!match) {
        fprintf(stderr,"PARSER: %s, got %s\n", error, Token_temp_cstr(t));
        exit(1);
    }
    //assert(match);
}

void dtp_expect_sym(DtParser* p, Token t, char sym, const char* error) {
    bool match = dtp_match_sym(t, sym);
    if (!match) {
        fprintf(stderr,"PARSER: %s, got %s\n", error, Token_temp_cstr(t));
        exit(1);
    }
    //assert(match);
}

DtNode* dtp_abort(DtParser* p, const char* error) {
    IGNORE_VALUE p;
    fprintf(stderr,"PARSER: %s\n", error);
    exit(1);
    return NULL;
}

#endif
Slice dt_slice_from_cstr(char* cstr) {
    Slice s = {
        .data = cstr,
        .length = strlen(cstr)
    };
    return s;
}

Slice dtp_slice_from_token(Token t) {
    assert(t.kind == TokenKind_word);
    Slice s = {
        .data = t.data.as_word.data,
        .length = t.data.as_word.length,
    };
    return s;
}

bool tkn_strncmp(Token t, const char* cmp, size_t length) {
    return 
        t.kind == TokenKind_word && 
        length == t.data.as_word.length && 
        strncmp(t.data.as_word.data, cmp, t.data.as_word.length) == 0;
}

bool tkn_strcmp(Token t, const char* cmp) {
    return tkn_strncmp(t, cmp, strlen(cmp));
}



DtNode* dtp_node_new(DtParser* p) {
    DtNode* node = arena_alloc(&(p->nodes), sizeof(DtNode));
    node->source_location = p->current_token; 
    return node;
}


DtNode* dtp_node_push(DtNode* begin, DtNode* item) {
    if (!begin)
        begin = item;
    else if (!begin->next)
        begin->next = item;
    else {
        DtNode* tail = begin;
        while(tail->next)
            tail = tail->next;
        tail->next = item;
    }
    return begin;
}

DtNode* dtp_node_append(DtNode* father, DtNode* children) {
    if(!father->children) {
        father->children = children;
    } else {
        DtNode* tail = father->children;
        while(tail->next)
            tail = tail->next;
        tail->next = children;
    }
    return father;
}


DtNode* dtp_expression              (DtParser* p, int depth) ;
DtNode* dtp_variable                (DtParser* p, int depth) ;
DtNode* dtp_rvalue                  (DtParser* p, int depth) ;
DtNode* dtp_function_declaration    (DtParser* p, int depth) ;
DtNode* dtp_statement               (DtParser* p, int depth) ;
DtNode* dtp_function_call           (DtParser* p, int depth) ;
DtNode* dtp_if_statement            (DtParser* p, int depth) ;
DtNode* dtp_block                   (DtParser* p, int depth, bool step_at_last) ;



// TODO: rename NK_[IF,ELSE,ELIF] to NK_BRANCH
DtNode* dtp_if_statement            (DtParser* p, int depth)  {
    DtNode* self = dtp_node_new(p); 
    self->kind = NK_IF_STATEMENT;
    DtNode* expr = 0;
    DtNode* block = 0;
    
    enum { 
        BRANCH_IF,
        BRANCH_ELIF, 
        BRANCH_ELSE, 
    } branch_type = BRANCH_IF;

parse_branch_again:
    Token keyword = dtp_step(p);
    
    switch (branch_type) {
        case BRANCH_IF: 
            dtp_expect_str(p, keyword, "if",  "Expected if");
            break;

        case BRANCH_ELIF: 
            dtp_expect_str(p, keyword, "else",  "Expected else");
            keyword = dtp_step(p);
            dtp_expect_str(p, keyword, "if",  "Expected else");
            break;

        case BRANCH_ELSE: 
            dtp_expect_str(p, keyword, "else",  "Expected else");
            break;
    }
    DtNode* branch = dtp_node_new(p);
    branch->kind = NK_IF;
    expr = dtp_expression(p, depth + 1);
    block = dtp_block(p, depth + 1, true);

    dtp_node_append(branch, expr);
    dtp_node_append(branch, block);
    dtp_node_append(self, branch);

    //BREAKPOINT();
    
    if (dtp_match_str(dtp_ahead(p), "else")) {
        if (dtp_match_str(dtp_aheadc(p,2), "if")) 
            branch_type = BRANCH_ELIF;
        else 
            branch_type = BRANCH_ELSE;
        //dtp_step(p); // ? not needed, i just got confused
        goto parse_branch_again;
    }

    return self;
}

// TODO: add whole bunch of stuff like:
// - if
// - for
// - loop
// - switch
// + return
// etc...
DtNode* dtp_statement(DtParser* p, int depth) {
    DtNode* self = 0;
    
    // return
    if (dtp_match_str(dtp_ahead(p), "return")) {
        self = dtp_node_new(p);
        self->kind = NK_RETURN;
        
        // remember to step
        dtp_step(p);

        dtp_node_append(self,dtp_expression(p, depth + 1));

        return self;
    } 

    // if
    else if(dtp_match_str(dtp_ahead(p),"if")) {
        self = dtp_if_statement(p, depth + 1);
        return self;
    }


    /*
    else if (dtp_match_sym(dtp_ahead(p), '{')) {
        self = dtp_block(p, depth + 1, false);
        return self;
    }
*/

    // function call
    else if (
            dtp_match_kind(dtp_ahead(p), TokenKind_word) && 
            dtp_match_sym(dtp_aheadc(p,2),'(')              )
    {
        dtp_step(p);
        DtNode* result = dtp_function_call(p, depth + 1);
        return result;
    } 

    // variable
    else {
        Token before = p->current_token;//dtp_ahead(p);
        self = dtp_variable(p, depth + 1);
        if (!self) {
            dtp_error_token(p, before, "Failed to parse variable inside statement");
            return NULL;
        }
        return self;
    }
}


DtNode* dtp_function_return_type(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p); 
    self->kind = NK_TYPE; 
    IGNORE_VALUE depth;
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work
    
    Token type = {0};

    dtp_expect_kind(p,(type = dtp_step(p)),TokenKind_word, "Expected return type");
    self->identifier = type;
    
    return self;
}

DtNode* dtp_block(DtParser* p, int depth, bool step_at_last) {
    DtNode* self = dtp_node_new(p); 
    DtNode* statement;
    self->kind = NK_BLOCK; 
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work

    // TODO: kms
    dtp_expect_sym(p, dtp_step(p), '{', "Expected '{' ");
    //while((!dtp_match_sym(dtp_ahead(p), '}')) && statement) {
    while((!dtp_match_sym(dtp_ahead(p), '}'))) {
        dtp_node_append(self, (statement = dtp_statement(p, depth + 1)));
        /*if (!statement) {
            dtp_error_token(p, p->current_token, "Failed to parse statement in block");
            return NULL;
        }*/    
    }
    Token last;
    if (step_at_last) last = dtp_step(p); else last = dtp_ahead(p);
    dtp_expect_sym(p, last, '}', "Expected '}' ");
    return self;
}


DtNode* dtp_symbol_declaration(DtParser* p, int depth) {
    IGNORE_VALUE depth;

    Token type = {0};
    Token var  = {0};
    DtNode* self = 0; 

    // TODO: add array and object parsing support
    
    dtp_expect_kind(p, (var = dtp_step(p)), TokenKind_word, "Expected name of variable");
    //dtp_expect_sym(p, dtp_step(p), ':', "Expected ':' between name and a type");
    dtp_expect_kind(p, (type = dtp_step(p)), TokenKind_word, "Expected type of variable");

    self = dtp_node_new(p);
    DtNode* nt = dtp_node_new(p);
    nt->kind = NK_TYPE;
    nt->identifier = type;
    self->kind = NK_SYMBOL_DECL;
    self->identifier = var;
    dtp_node_append(self, nt);

    // TODO: set identifer 
    //self->value.identifier 
    return self;
}



DtNode* dtp_function_declaration(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p);
    DtNode* args = dtp_node_new(p);
    //Token  name = {0};

    //DtNode* node = {0};
    self->kind = NK_FUNCTION_DECL;
    args->kind = NK_FUNCTION_ARGS;

    dtp_expect_kind (p, p->current_token, TokenKind_word, "Expected word");
    self->identifier = p->current_token;

    dtp_expect_sym  (p, dtp_step(p), '(', "Expected '('");

    Token arg_token = p->current_token;
    DtNode* n = 0;

 dtp_array_next:;
    switch ((arg_token = dtp_ahead(p)).kind) {
 
        case TokenKind_word:
            n = dtp_symbol_declaration(p, depth + 1);
            if (!n) dtp_abort(p, "Failed to parse function agrument");

            dtp_node_append(args, n);
            goto dtp_array_next;
            break;

        case TokenKind_symbol:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            }             
            break;

        default: 
            dtp_error_token(p, arg_token, "failed to parse function arguments, expected <ident type>"); 
            return NULL;
    }

    dtp_expect_sym(p, dtp_step(p), ')', "Expected ')' at the end of the function arguments.");

    dtp_node_append(self,args);

    // Function type
    if (dtp_match_sym(dtp_ahead(p), ':')) {
        dtp_expect_sym(p, dtp_step(p), ':', "Expected ':' due to provided aguments");
        
        // todo more complex types like arrays, objects
        dtp_expect_kind(p, dtp_ahead(p), TokenKind_word, "Expected 'word' that indicates return type");
        dtp_node_append(self, dtp_function_return_type(p, depth+1));
    }

    DtNode* block = dtp_block(p, depth+1, true);
    //if (!block) {
        //dtp_error_token_trace(p, p->current_token, "Failed to parse function body");
       // return NULL;
    //}
    dtp_node_append(self, block);
    return self;
}


DtNode* dtp_function_call(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    self->kind = NK_FUNCTION_CALL;
    self->identifier = p->current_token;
    self->source_location = p->current_token;
    
    dtp_expect_kind (p,p->current_token, TokenKind_word, "Expected word");
    dtp_expect_sym  (p,dtp_step(p), '(', "Expected '('");

 dtp_array_next:;
    if (dtp_match_sym(dtp_ahead(p), ',')) {
        dtp_step(p);
        goto dtp_array_next;
    } else if (dtp_match_sym(dtp_ahead(p), ')'))
        ; // finish cycling
    else {
        dtp_node_append(self, dtp_expression(p, depth+1));
        goto dtp_array_next;
    }

    dtp_expect_sym(p, dtp_step(p), ')', "Expected ')' at the end of the array.");
    return self;
}

DtNode* dtp_object(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    DtNode* self = dtp_node_new(p);
    DtNode* node = 0;
    Token ahead = {0};

    self->kind = NK_OBJECT;
    self->source_location = p->current_token;
    dtp_expect_sym(p, dtp_step(p), '{', "Expected '{' for object opening");
    
try_again:
    ahead = dtp_ahead(p);
    if(dtp_match_kind(ahead, TokenKind_word)) {
        dtp_node_append(self, dtp_variable(p, depth++));
        ahead = dtp_ahead(p);
        if(dtp_match_sym(ahead, ',')) {
            dtp_step(p);
            goto try_again;
        }
    } else if (dtp_match_sym(dtp_ahead(p), '}')) {
        goto end;
    } else {

        node = dtp_rvalue(p,depth++);
        if (!node) {
            dtp_error_token(p, ahead, "Failed to parse rvalue in object");
            dtp_error_push(p);
            return NULL;
        }
        dtp_node_append(self, node);
        ahead = dtp_ahead(p);

        if(dtp_match_sym(ahead, ',')) {
            dtp_step(p);
            goto try_again;
        } else goto end;
    }

end:
    dtp_expect_sym(p, dtp_step(p), '}', "Expected '}' for object closing");
    return self;
}

DtNode* dtp_string(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};

    Token value = dtp_step(p);
    dtp_expect_kind(p, value, TokenKind_literall_string, "Expected string literall");
    
    self->kind = NK_STRLIT;
    return self;
}

bool dtp_is_numeric(Token t) {
    return t.kind == TokenKind_literall_integer 
        || t.kind == TokenKind_literall_float;
}

bool dtp_is_value(Token t) {
    return t.kind == TokenKind_literall_integer 
        || t.kind == TokenKind_literall_float
        || t.kind == TokenKind_literall_string
        || t.kind == TokenKind_word;
}


// TODO: array literalls and object?literalls?
bool dtp_is_expression(Token t) {
    return t.kind == TokenKind_literall_integer 
        || t.kind == TokenKind_literall_float
        || t.kind == TokenKind_literall_string
        || t.kind == TokenKind_word
        || dtp_match_sym(t, '(');
}


bool dtp_is_add(Token t) {
    return dtp_match_sym(t, '+') || dtp_match_sym(t, '-');
}

bool dtp_is_mul(Token t) {
    return dtp_match_sym(t, '*') || dtp_match_sym(t, '/');
}

bool dtp_is_cmp(Token t) {
    return 
        dtp_match_sym(t, '>') || dtp_match_sym(t, '<') || 
        dtp_match_str(t, ">=") || dtp_match_str(t, "<=")
    ;
}

bool dtp_is_eql(Token t) {
    return 
        dtp_match_str(t, "==") || dtp_match_str(t, "!=")
    ;
}

bool dtp_is_unary(Token t) {
    return dtp_match_sym(t, '!') || dtp_match_sym(t, '-');
}

DtNode* dtp_branch(DtParser* p, DtNode* l, DtNode* r, DtNodeKind kind) {
    DtNode* self = dtp_node_new(p);
    dtp_node_append(self,l);
    dtp_node_append(self,r);
    if (self)
        self->kind = kind;
    return self;
}

DtNode* dtp_comparison  (DtParser*, int);
DtNode* dtp_equality    (DtParser*, int);
DtNode* dtp_factor      (DtParser*, int);
DtNode* dtp_term        (DtParser*, int);
DtNode* dtp_primary     (DtParser*, int);
DtNode* dtp_unary       (DtParser*, int);


DtNode* dtp_equality(DtParser* p, int depth) {
    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };

    bool is_equality[2] = {0};
    DtNode *l = 0, *r = 0, *rr = 0;
    l = dtp_comparison(p, depth + 1);

    if (dtp_is_eql(dtp_ahead(p))) {
        
        if (dtp_match_str(dtp_step(p),"==")) 
            is_equality[OP_CMP_ONCE] = true;

        r = dtp_factor(p, depth + 1);
        if (dtp_is_eql(dtp_ahead(p))) {
   
            if (dtp_match_str(dtp_step(p),"==")) 
                is_equality[OP_CMP_MANY] = true;
            
            rr = dtp_term(p, depth + 1);
            r = dtp_branch(p, r, rr, NK_EQALITY);
            
            r->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_MANY];
        }
        DtNode* result = dtp_branch(p, l, r, NK_EQALITY);
        result->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_ONCE];
        return result;
    }
    return l;
}

/*
DtNode* dtp_equality(DtParser* p, int depth) {
    DtNode *l = 0, *r = 0;
    // *rr = 0;
    l = dtp_comparison(p, depth + 1);

    if (dtp_is_eql(dtp_ahead(p))) {
        
        dtp_step(p); // skip cmp
        r = dtp_comparison(p, depth + 1);

        DtNode* result = dtp_branch(p, l, r, NK_EQALITY);
        return result;
    }
    return l;
}
*/

DtNode* dtp_comparison(DtParser* p, int depth) {
    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };
    bool is_cmp_eq  [2] = {0};
    bool is_cmp_gt  [2] = {0};

    DtNode *l = 0, *r = 0, *rr = 0;
    l = dtp_term(p, depth + 1);

    if (dtp_is_cmp(dtp_ahead(p))) {
        
        Token cmp = dtp_step(p);
        if (dtp_match_sym(cmp,'>')) 
            is_cmp_gt[OP_CMP_ONCE] = true;
        else if (dtp_match_sym(cmp,'<')) 
            is_cmp_gt[OP_CMP_ONCE] = false;
        else if (dtp_match_str(cmp, ">=")) {
            is_cmp_eq[OP_CMP_ONCE] = true;
            is_cmp_gt[OP_CMP_ONCE] = true;
        } else if (dtp_match_str(cmp, "<=")) {
            is_cmp_eq[OP_CMP_ONCE] = true;
            is_cmp_gt[OP_CMP_ONCE] = false;
        } else 
            assert(0);

        r = dtp_term(p, depth + 1);

        if (dtp_is_cmp(dtp_ahead(p))) {
   
            Token cmp = dtp_step(p);
            if (dtp_match_sym(cmp,'>')) 
                is_cmp_gt[OP_CMP_MANY] = true;
            else if (dtp_match_sym(cmp,'<')) 
                is_cmp_gt[OP_CMP_MANY] = false;
            else if (dtp_match_str(cmp, ">=")) {
                is_cmp_eq[OP_CMP_MANY] = true;
                is_cmp_gt[OP_CMP_MANY] = true;
            } else if (dtp_match_str(cmp, "<=")) {
                is_cmp_eq[OP_CMP_MANY] = true;
                is_cmp_gt[OP_CMP_MANY] = false;
            } else 
                assert(0);
            
            rr = dtp_comparison(p, depth + 1);
            r = dtp_branch(p, r, rr, NK_COMPARISON);
            
            r->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_MANY];
            r->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_MANY];
        }

        DtNode* result = dtp_branch(p, l, r, NK_COMPARISON);
        result->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_ONCE];
        result->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_ONCE];
        return result;
    }

    return l;
}

DtNode* dtp_term(DtParser* p, int depth) {
    enum {
        OP_ADD_ONCE,
        OP_ADD_MANY,
    };

    bool is_plus[2] = {0};
    DtNode *l = 0, *r = 0, *rr = 0;
    l = dtp_factor(p, depth + 1);

    if (dtp_is_add(dtp_ahead(p))) {
        
        if (dtp_match_sym(dtp_step(p),'+')) 
            is_plus[OP_ADD_ONCE] = true;
        r = dtp_factor(p, depth + 1);

        if (dtp_is_add(dtp_ahead(p))) {
   
            if (dtp_match_sym(dtp_step(p),'+')) 
                is_plus[OP_ADD_MANY] = true;
            
            rr = dtp_term(p, depth + 1);
            r = dtp_branch(p, r, rr, NK_TERM);
            
            r->properties |= NKP_IS_ADD * is_plus[OP_ADD_MANY];
        }
        DtNode* result = dtp_branch(p, l, r, NK_TERM);
        result->properties |= NKP_IS_ADD * is_plus[OP_ADD_ONCE];
        return result;
    }
    return l;
}


DtNode* dtp_factor(DtParser* p, int depth) {
    enum {
        OP_MUL_ONCE,
        OP_MUL_MANY,
    };

    bool is_plus[2] = {0};
    DtNode *l = 0, *r = 0, *rr = 0;
    //l = dtp_primary(p, depth + 1);
    l = dtp_unary(p, depth + 1);

    if (dtp_is_mul(dtp_ahead(p))) {
        
        if (dtp_match_sym(dtp_step(p),'*')) 
            is_plus[OP_MUL_ONCE] = true;
        //r = dtp_primary(p, depth + 1);
        r = dtp_unary(p, depth + 1);

        if (dtp_is_mul(dtp_ahead(p))) {
        
            if (dtp_match_sym(dtp_step(p),'*')) 
                is_plus[OP_MUL_MANY] = true;
         
            rr = dtp_factor(p, depth + 1);
            r = dtp_branch(p, r, rr, NK_FACTOR);

            r->properties |= NKP_IS_MUL * is_plus[OP_MUL_MANY];
        }

        DtNode* result = dtp_branch(p, l, r, NK_FACTOR);
        result->properties |= NKP_IS_MUL * is_plus[OP_MUL_ONCE];
        return result;
    }

    return l;
}

DtNode* dtp_unary(DtParser* p, int depth) {
    DtNode* self = 0;
    if (dtp_is_unary(dtp_ahead(p))) {
        dtp_step(p); // skip unary
        self = dtp_unary(p, depth + 1);
        self->properties |= NKP_HAS_UNARY;
    } else 
        self = dtp_primary(p, depth + 1);
    return self;
}



DtNode* dtp_value(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p);
    //Token*  current_token = &(p->current_token);
    Token   name = {0};
    bool match_true = 0;

    switch((name = dtp_step(p)).kind) {
        case TokenKind_literall_integer: 
            self->kind       = NK_INTLIT;
            self->identifier = name;
            break;

        case TokenKind_literall_float: 
            self->kind         = NK_FLTLIT;
            self->identifier   = name;
            break;

        case TokenKind_literall_string:
            self->kind         = NK_STRLIT;
            self->identifier   = name;
            break;

        case TokenKind_word: 
            if ( 
                (match_true = (dtp_match_str(name, "true"))) || 
                dtp_match_str(name, "false")
            ){
                self->kind = NK_BOOLIT;
                self->identifier = name;
                break; 
            }

            self->kind = NK_IDENTIFIER;
            self->identifier = name; 
            
            if (dtp_match_sym(dtp_ahead(p), '(')) {
                self->kind = NK_FUNCTION_CALL;
                return dtp_function_call(p, depth + 1);
            }
            break;

        default:
            assert(0 && "Expected int, float or name indent (variable or function)");
    }
    return self;
}


// TODO: implement negative numbers
DtNode* dtp_primary(DtParser* p, int depth) {
    DtNode *self = 0;// *r, *rr;

    if (dtp_match_sym(dtp_ahead(p), '(')) {
        dtp_expect_sym(p, dtp_step(p), '(', "Expected '('");
        
        self = dtp_equality(p, depth + 1);
       
        // TODO: URGENT: might have no effect or be a break-point  
#if 0
        if (!self) {
            self->kind = NK_TERM;
        }
#endif 

        dtp_expect_sym(p, dtp_step(p), ')', "Expected ')'");
        return self;
    } else if (dtp_is_value(dtp_ahead(p))) {
        return dtp_value(p, depth + 1);
    } else 
        // TODO: handle this edge case
        printf("edging at dtp_prime(DtParser*, int) with: %s", 
                Token_temp_cstr(p->current_token)
                );//assert(0 && "Failed to parse prime");
    return self;
}


DtNode* dtp_expression(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};

    DtNode* node = dtp_equality(p, depth + 1);
    //Token value = dtp_step(p);
    //dtp_expect_kind(value, TokenKind_literall_integer, "TODO: add more the just integers for expression");
    
    self->kind = NK_EXPRESSION;
    return dtp_node_append(self, node);
}



DtNode* dtp_array(DtParser *p, int depth) {
    DtNode* self = dtp_node_new(p);
    DtNode* node = {0};

    dtp_expect_sym (p, dtp_step(p), '[', "Expected '['");
    
dtp_array_next:;
    switch (dtp_ahead(p).kind) {
        
        case TokenKind_literall_integer:
        case TokenKind_literall_float:
            dtp_node_append(self, (node = dtp_expression(p, depth + 1)));
            goto dtp_array_next;
            break;

        case TokenKind_literall_string:
            dtp_node_append(self, (node = dtp_string(p, depth + 1)));
            goto dtp_array_next;
            break;

        case TokenKind_symbol:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            }             
            break;

        default: assert(0); break;
    }

    dtp_expect_sym(p, dtp_step(p), ']', "Expected ']' at the end of the array.");

    self->kind = NK_ARRAY;

    return self;
}

DtNode* dtp_variable(DtParser* p, int depth) {
    DtNode* self = 0;//dtp_node_new(p);
    Token name = {0};
    //Token lhs = {0};

    name = dtp_step(p);
    dtp_expect_kind (p, name, TokenKind_word, "Expected word");
    dtp_expect_sym  (p, dtp_step(p), '=', "Expected '='");
    
    self = dtp_rvalue(p, depth++);
    if (!self) {
        dtp_error_token(p, dtp_step(p), "Failed to parse variable");
        dtp_error_push(p);
        return NULL;
    }
    self->kind = NK_VARIABLE;

    self->identifier = name;

    return self;
}

DtNode* dtp_rvalue(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p);
    DtNode* node = {0};
    Token lhs = {0};
    
    lhs = p->current_token;
    self->kind = NK_RVALUE;
    switch (dtp_ahead(p).kind) {
        /*
        case TokenKind_literall_string:
            dtp_node_append(self, (node = dtp_string(p, depth + 1)));
            break;
            */
        case TokenKind_literall_string:
        case TokenKind_literall_integer:
        case TokenKind_literall_float:
        case TokenKind_symbol:
        case TokenKind_word:
            {
                Token ahead = dtp_ahead(p);
                if (dtp_match_sym(ahead, '[')) {
                    if ((node = dtp_array(p, depth + 1)))
                        dtp_node_append(self, node);
                } else if (dtp_match_sym(ahead,'{')) {
                    if ((node = dtp_object(p, depth + 1))) {
                        dtp_node_append(self, node);
                    } else {
                        //return NULL;
                    }
                } else if (dtp_is_expression(ahead)) {
                    dtp_node_append(self,(node = dtp_expression(p, depth + 1))); 
                } else {
                    dtp_error_token(p, lhs, "expected value of any kind");
                    return (self = 0);
                }
            } break;

        default: assert(0); break;
    }


    return self;
}

DtNode* dtp_parse(DtParser* p, int depth) {
    DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    Token name = {0};
    (void) name;

    self->kind = NK_ROOT;
    while(dtp_have_tokens(p)) {
        //dtp_load_tokens_buffer(p);
        Token t     = dtp_step(p);//dtp_aheadc(p, 0);
        Token nt    = dtp_ahead(p);

        //dtp_print_token_buffer(*p);
        //printf("%i, %i\n", t.kind, nt.kind);

        // we creating a type
        if(dtp_match_str(t, "type")) {
            dtp_error(p, "TODO: implement type keyword");
        } else if (dtp_match_str(t, "include")) {
            dtp_error(p, "TODO: implement include keyword");

        }

        /*
        else if (dtp_match_sym(t, '{')) {
            DtNode* b = dtp_block(p, depth+1, false);
            dtp_node_append(self, b);
        } 
        */

        else if (dtp_match_kind(t, TokenKind_word)) {
            name = t;
            if (dtp_valid_token(nt) && dtp_match_sym(nt, '(')) {
                //dtp_step(p);
                DtNode* f = dtp_function_declaration(p,depth+1);
                if(!f) {
                    dtp_error_token(p, nt, "Failed to parse function declaration");
                    dtp_error_push(p);
                    return f;
                }

                // TODO: replace this and remove dtv_from_token function
                //f->value = dtv_from_token(name, VT_FUNCTION);
                dtp_node_append(self, f);
                
            } 

            else {
                dtp_node_append(self, dtp_statement(p, depth+1));
            }

            //else return dtp_abort(p, "Expected symbols after");

        } else if (!dtp_have_tokens(p)) {
            return self;
        }
        else {
            //printf("failed with token %s\n", Token_temp_cstr(t));
            dtp_abort(p, "Unexpected high level statement");
        }
    }
    return self;
}

DtNode* dtp_node_index(DtNode* node, size_t i) {
    size_t counter = 0;
    DtNode* next = node;
    while(next) {
        if (counter == i) return next;
        counter++;
        next = next->next;
    }
    return 0;
}

DtNode* dtp_node_get(DtNode* node, DtNodeKind kind) {
    if (!node) return NULL;
    if (node && node->kind == kind) return node;
    DtNode* next = node->children;
    DtNode* result = 0;
    while(next) {
        if (next->kind == kind) return next;
        result = dtp_node_get(next->children, kind);
        if(result) return result;
        next = next->next;
    }
    return NULL;
}

// TODO: print literalls
// TODO: print expressions in parenthesis as if they where 
//          written by a person
//              E.G: 1 + ((10 / 2) * 3 )
//
void dtp_print_ast(DtNode* node, int depth, FILE* output_redir) {
    if (!node) return;
    DtNode* list = node;
    while(list) {
        fprintf(output_redir, 
                "%*s%s: "
                "%c%s\t",
                
                depth*4, "", 
                DT_NODE_KIND_STR[list->kind],

                (list->properties & NKP_HAS_UNARY) ? '-' : ' ',
                Token_text_cstr(list->identifier)
        );

        //fprintf(output_redir, " -> %i", list->value.type);

        fprintf(output_redir, "\n");
        
        dtp_print_ast(list->children, depth+1, output_redir);
        list = list->next;
    }    
}


