#include <errno.h>
#include "dtc.h"
#include "tokenizer.c"

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
    DtStatus*       status;
    StringBuilder   output;

    size_t      current;
    size_t      requested_tokens,
                parsed_tokens;
    // we buffer PTBS(size) of tokens to be able 
    // to lookup tokens before and after the current one
    Tokenizer   lexer;
    Token       current_token;
    Token       token_buffer
                    [PARSER_TOKEN_BUFFER_SIZE];
    
    Arena           allocator;
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
			hc_arrlen(token_table),
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

#ifdef PARSER_TRACE
#define dt_trace(name, depth, ...)\
        fprintf(stderr, "%*.s> %s", depth, "\t", name);\
        fprintf(stderr, ""__VA_ARGS__);\
        fprintf(stderr, "\n");
#else
#define dt_trace(name,depth, ...) /* __VA_ARGS__ */
#endif

void dtp_print_token_buffer(DtParser p) {
    printf("current: %lu\t", p.current);
    printf("buffer: [ ");
    hc_loop(i, PARSER_TOKEN_BUFFER_SIZE) 
        printf("%i ", p.token_buffer[i].kind);
    printf("] ");
 
}

void dtp_load_tokens_buffer(DtParser* p) {
    Tokenizer* lexer = &(p->lexer);
    // Buffer tokens in batches of set size
    // so that its easy to check previous 
    if (p->parsed_tokens == 0) {
        hc_loop(i, PARSER_TOKEN_BUFFER_SIZE) {
            p->token_buffer[p->parsed_tokens++] = 
                Tokenizer_next_token(lexer);
        }
    } else if (p->requested_tokens + PARSER_LOOK_AHEAD_COUNT > p->parsed_tokens) {
        hc_loop(i,PARSER_LOOK_AHEAD_COUNT)
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
    return p->current_token.kind != TOKEN_KIND_EOF;
}



bool dtp_valid_token(Token t) {
    return t.kind != TOKEN_KIND_NULL && t.kind != TOKEN_KIND_EOF;
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
        t.kind == TOKEN_KIND_WORD && 
        t.data.as_word.data &&
        strlen(text) == t.data.as_word.length &&
        strncmp(t.data.as_word.data, text, t.data.as_word.length) == 0;
}

bool dtp_match_sym(Token t,char sym) {
    token_dump_sym(t);
    return 
        dtp_valid_token(t) &&
        t.kind == TOKEN_KIND_SYMBOL && 
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

void dtp_error_message(const char* message, int r, int c) {
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

    fprintf(stderr, 
            "\n""[%i:%i] error: %s",
            r, 
            c, message
    );
    __asm__("int3");
}


void dtp_error_token(Token t, const char* message) {
    assert(0);
    dtp_error_message(message, t.row, t.col); //!dtp_valid_token(t));
}

#define dtp_expect_str(P,T,S,E) if (!dtp_match_str(T, S)) {\
    assert(0);\
    dtp_error_token(T, E);\
    return NULL;\
}

#define dtp_expect_kind(P,T,K,E) if (!dtp_match_kind(T, K)) {\
    assert(0);\
    dtp_error_token(T, E);\
    return NULL;\
}
    
#define dtp_expect_sym(P,T,S,E) if (!dtp_match_sym(T, S)) {\
    assert(0);\
    dtp_error_token(T, E);\
    return NULL;\
}

#define dtp_abort(P, E) do { fprintf(stderr,E); __asm__("int3"); } while(0)

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
    assert(t.kind == TOKEN_KIND_WORD);
    Slice s = {
        .data = t.data.as_word.data,
        .length = t.data.as_word.length,
    };
    return s;
}

bool tkn_strncmp(Token t, const char* cmp, size_t length) {
    return 
        t.kind == TOKEN_KIND_WORD && 
        length == t.data.as_word.length && 
        strncmp(t.data.as_word.data, cmp, t.data.as_word.length) == 0;
}

bool tkn_strcmp(Token t, const char* cmp) {
    return tkn_strncmp(t, cmp, strlen(cmp));
}

DtNode* dtp_node_new(DtParser* p) {
    DtNode* node = arena_alloc(&(p->allocator), sizeof(DtNode));
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

typedef enum {
    DT_PNKIND_OK,
    DT_PNKIND_VALUE,
} DtParseNodeKind;

typedef struct {
    bool negative_sign;
    bool foldable_constant;
} DtParseValue;

typedef struct {
    DtParseNodeKind kind;   
    union {
        DtParseValue value;
    } as;
} DtParseNode;


bool dtp_expression              (DtParser* p, int depth) ;
bool dtp_variable                (DtParser* p, int depth) ;
bool dtp_rvalue                  (DtParser* p, int depth) ;
bool dtp_function_declaration    (DtParser* p, int depth) ;
bool dtp_statement               (DtParser* p, int depth) ;
bool dtp_function_call           (DtParser* p, int depth) ;
bool dtp_if_statement            (DtParser* p, int depth) ;
bool dtp_block                   (DtParser* p, int depth, bool step_at_last) ;



// TODO: rename NK_[IF,ELSE,ELIF] to NK_BRANCH
bool dtp_if_statement            (DtParser* p, int depth)  {
    enum { 
        BRANCH_IF,
        BRANCH_ELIF, 
        BRANCH_ELSE, 
    } branch_type = BRANCH_IF;

parse_branch_again: ;
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
    dtp_expression(p, depth + 1);
    dtp_block(p, depth + 1, true);

    //BREAKPOINT();
    
    if (dtp_match_str(dtp_ahead(p), "else")) {
        if (dtp_match_str(dtp_aheadc(p,2), "if")) 
            branch_type = BRANCH_ELIF;
        else 
            branch_type = BRANCH_ELSE;
        //dtp_step(p); // ? not needed, i just got confused
        goto parse_branch_again;
    }

    return true;
}

// TODO: add whole bunch of stuff like:
// - if
// - for
// - loop
// - switch
// + return
// etc...
bool dtp_statement(DtParser* p, int depth) {
    StringBuilder* sb = &p->output;
    dt_trace("statement:", depth, );
    // 'return'
    if (dtp_match_str(dtp_ahead(p), "return")) {
        // remember to step
        dtp_step(p);
        dtp_expression(p, depth + 1);
        sb_append(sb, "\tret\n");
        // return self;
    } 

    // if
    else if(dtp_match_str(dtp_ahead(p),"if")) {
        dtp_if_statement(p, depth + 1);
        // return self;
    }


    /*
    else if (dtp_match_sym(dtp_ahead(p), '{')) {
        self = dtp_block(p, depth + 1, false);
        return self;
    }
*/

    // function call
    else if (
            dtp_match_kind(dtp_ahead(p), TOKEN_KIND_WORD) && 
            dtp_match_sym(dtp_aheadc(p,2),'(')              )
    {
        dtp_step(p);
        dtp_function_call(p, depth + 1);
        // return result;
    } 

    // variable
    else {
        Token before = p->current_token;//dtp_ahead(p);
        if (dtp_variable(p, depth + 1)) {
            dtp_error_token(before, "Failed to parse variable inside statement");
            // return NULL;
        }
        // return self;
    }
    return true;
}

bool dtp_function_return_type(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p); 
    // self->kind = NK_TYPE; 
    IGNORE_VALUE depth;
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work
    
    Token type = {0};
    dtp_expect_kind(p,(type = dtp_step(p)),TOKEN_KIND_WORD, "Expected return type");
    // self->identifier = type;
    
    return true;
}

bool dtp_block(DtParser* p, int depth, bool step_at_last) {
    dt_trace("block:", depth, );
    
    // DtNode* self = dtp_node_new(p); 
    // DtNode* statement;
    // self->kind = NK_BLOCK; 
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work

    // TODO: kms
    dtp_expect_sym(p, dtp_step(p), '{', "Expected '{' ");
    //while((!dtp_match_sym(dtp_ahead(p), '}')) && statement) {
    while((!dtp_match_sym(dtp_ahead(p), '}'))) {
        dtp_statement(p, depth + 1);
        /*if (!statement) {
            dtp_error_token(p, p->current_token, "Failed to parse statement in block");
            return NULL;
        }*/    
    }
    Token last;
    if (step_at_last) last = dtp_step(p); else last = dtp_ahead(p);
    dtp_expect_sym(p, last, '}', "Expected '}' ");
    return true;
}


bool dtp_symbol_declaration(DtParser* p, int depth) {
    IGNORE_VALUE depth;

    // Token type = {0};
    Token var  = {0};
    // DtNode* self = 0; 

    // TODO: add array and object parsing support
    
    dtp_expect_kind(p, (var = dtp_step(p)), TOKEN_KIND_WORD, "Expected name of variable");

    sb_appendf(&p->output, "%.*s ", 
            (int)var.data.as_word.length,
            var.data.as_word.data
    );
    //dtp_expect_sym(p, dtp_step(p), ':', "Expected ':' between name and a type");
    // dtp_expect_kind(p, (type = dtp_step(p)), TOKEN_KIND_WORD, "Expected type of variable");

    // self = dtp_node_new(p);
    // DtNode* nt = dtp_node_new(p);
    // nt->kind = NK_TYPE;
    // nt->identifier = type;
    // self->kind = NK_SYMBOL_DECL;
    // self->identifier = var;
    // dtp_node_append(self, nt);

    // TODO: set identifer 
    //self->value.identifier 
    return true;
}

bool dtp_function_declaration(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p);
    // DtNode* args = dtp_node_new(p);
    //Token  name = {0};

    //DtNode* node = {0};
    // self->kind = NK_FUNCTION_DECL;
    // args->kind = NK_FUNCTION_ARGS;
    
    StringBuilder* sb = &p->output;
    dtp_expect_kind (p, p->current_token, TOKEN_KIND_WORD, "Expected word");
    // self->identifier = p->current_token;


    dt_trace("function:", depth, "%s", Token_text_cstr(p->current_token));
    sb_appendf(sb, "fn %.*s ", 
            (int)p->current_token.data.as_word.length,
            p->current_token.data.as_word.data
    );

    dtp_expect_sym  (p, dtp_step(p), '(', "Expected '('");

    Token arg_token = p->current_token;
    // DtNode* n = 0;

 dtp_array_next:;
    switch ((arg_token = dtp_ahead(p)).kind) {
 
        case TOKEN_KIND_WORD:
            dtp_symbol_declaration(p, depth + 1);

            // if (!n) dtp_abort(p, "Failed to parse function agrument");

            // dtp_node_append(args, n);
            goto dtp_array_next;
            break;

        case TOKEN_KIND_SYMBOL:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            }             
            break;

        default: 
            dtp_error_token(arg_token, "failed to parse function arguments, expected <ident>, ..."); 
            return NULL;
    }

    dtp_expect_sym(p, dtp_step(p), ')', "Expected ')' at the end of the function arguments.");
    sb_append(sb, "\n");

    // dtp_node_append(self,args);

    // Function type
    // if (dtp_match_sym(dtp_ahead(p), ':')) {
    //     dtp_expect_sym(p, dtp_step(p), ':', "Expected ':' due to provided aguments");
    //    
    //     // todo more complex types like arrays, objects
    //     dtp_expect_kind(p, dtp_ahead(p), TOKEN_KIND_WORD, "Expected 'word' that indicates return type");
    //     // dtp_node_append(self, dtp_function_return_type(p, depth+1));
    //     dtp_function_return_type(p, depth+1);
    // }

    dtp_block(p, depth+1, true);
    //if (!block) {
        //dtp_error_token_trace(p, p->current_token, "Failed to parse function body");
       // return NULL;
    //}
    // dtp_node_append(self, block);
    sb_append(sb, "endfn\n");
    return true;
}

bool dtp_function_call(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    // self->kind = NK_FUNCTION_CALL;
    // self->identifier = p->current_token;
    // self->source_location = p->current_token;
    
    dtp_expect_kind (p,p->current_token, TOKEN_KIND_WORD, "Expected word");
    dtp_expect_sym  (p,dtp_step(p), '(', "Expected '('");

 dtp_array_next:;
    if (dtp_match_sym(dtp_ahead(p), ',')) {
        dtp_step(p);
        goto dtp_array_next;
    } else if (dtp_match_sym(dtp_ahead(p), ')'))
        ; // finish cycling
    else {
        // dtp_node_append(self, dtp_expression(p, depth+1));
        dtp_expression(p, depth+1);
        goto dtp_array_next;
    }

    dtp_expect_sym(p, dtp_step(p), ')', "Expected ')' at the end of the array.");
    return true;
}

bool dtp_object(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    // DtNode* self = dtp_node_new(p);
    // DtNode* node = 0;
    Token ahead = {0};

    // self->kind = NK_OBJECT;
    // self->source_location = p->current_token;
    dtp_expect_sym(p, dtp_step(p), '{', "Expected '{' for object opening");
    
try_again:
    ahead = dtp_ahead(p);
    if(dtp_match_kind(ahead, TOKEN_KIND_WORD)) {
        // dtp_node_append(self, dtp_variable(p, depth++));
        dtp_variable(p, depth++);
        ahead = dtp_ahead(p);
        if(dtp_match_sym(ahead, ',')) {
            dtp_step(p);
            goto try_again;
        }
    } else if (dtp_match_sym(dtp_ahead(p), '}')) {
        goto end;
    } else {
        dtp_rvalue(p,depth++);
        // if (!node) {
        //     dtp_error_token(ahead, "Failed to parse rvalue in object");
        //     return NULL;
        // }
        // dtp_node_append(self, node);
        ahead = dtp_ahead(p);

        if(dtp_match_sym(ahead, ',')) {
            dtp_step(p);
            goto try_again;
        } else goto end;
    }

end:
    dtp_expect_sym(p, dtp_step(p), '}', "Expected '}' for object closing");
    return true;
}

DtNode* dtp_string(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    Token value = dtp_step(p);
    dtp_expect_kind(p, value, TOKEN_KIND_LITERALL_STRING, "Expected string literall");
    
    self->kind = NK_STRLIT;
    return self;
}

bool dtp_is_numeric(Token t) {
    return t.kind == TOKEN_KIND_LITERALL_INTEGER 
        || t.kind == TOKEN_KIND_LITERALL_FLOAT;
}

bool dtp_is_value(Token t) {
    return t.kind == TOKEN_KIND_LITERALL_INTEGER 
        || t.kind == TOKEN_KIND_LITERALL_FLOAT
        || t.kind == TOKEN_KIND_LITERALL_STRING
        || t.kind == TOKEN_KIND_WORD;
}

// TODO: array literalls and object?literalls?
bool dtp_is_expression(Token t) {
    return t.kind == TOKEN_KIND_LITERALL_INTEGER 
        || t.kind == TOKEN_KIND_LITERALL_FLOAT
        || t.kind == TOKEN_KIND_LITERALL_STRING
        || t.kind == TOKEN_KIND_WORD
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
        dtp_match_sym(t, '>')  || dtp_match_sym(t, '<')  || 
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

bool dtp_comparison  (DtParser*, int);
bool dtp_equality    (DtParser*, int);
bool dtp_factor      (DtParser*, int);
bool dtp_term        (DtParser*, int);
bool dtp_primary     (DtParser*, int);
bool dtp_unary       (DtParser*, int);

bool dtp_equality(DtParser* p, int depth) {
    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };

    bool is_equality[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_comparison(p, depth + 1);
    dtp_comparison(p, depth + 1);

    if (dtp_is_eql(dtp_ahead(p))) {
        dt_trace("eq |",depth, );
        
        if (dtp_match_str(dtp_step(p),"==")) 
            is_equality[OP_CMP_ONCE] = true;

        // r = dtp_factor(p, depth + 1);
        dtp_factor(p, depth + 1);
        if (dtp_is_eql(dtp_ahead(p))) {
   
            if (dtp_match_str(dtp_step(p),"==")) 
                is_equality[OP_CMP_MANY] = true;
            
            // rr = dtp_term(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_EQALITY);
            dtp_term(p, depth + 1);
            // dtp_branch(p, r, rr, NK_EQALITY);
            
            // r->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_MANY];
        }
        // DtNode* result = dtp_branch(p, l, r, NK_EQALITY);
        // dtp_branch(p, l, r, NK_EQALITY);
        // result->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_ONCE];
        // return result;
    }
    return true;
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

bool dtp_comparison(DtParser* p, int depth) {

    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };
    bool is_cmp_eq  [2] = {0};
    bool is_cmp_gt  [2] = {0};

    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_term(p, depth + 1);
    dtp_term(p, depth + 1);

    if (dtp_is_cmp(dtp_ahead(p))) {
        dt_trace("compare |",depth, );

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

        // r = dtp_term(p, depth + 1);
        dtp_term(p, depth + 1);

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
            
            // rr = dtp_comparison(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_COMPARISON);
            dtp_comparison(p, depth + 1);
            
            // r->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_MANY];
            // r->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_MANY];
        }

        // DtNode* result = dtp_branch(p, l, r, NK_COMPARISON);
        // result->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_ONCE];
        // result->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_ONCE];
        // return result;
    }

    return true;
}

void sb_merge(StringBuilder *dest, StringBuilder *src) {
    sb_appendf(dest, "%*.s", (int)src->count, src->items);
}


// TODO: fix breaking on terminating term.
bool dtp_term(DtParser* p, int depth) {
    // StringBuilder before = p->output;
    StringBuilder* sb = &p->output;
    // memset(after, 0, sizeof(*after));

    enum {
        OP_ADD_ONCE,
        OP_ADD_MANY,
    };

    bool is_plus[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_factor(p, depth + 1);
    dtp_factor(p, depth + 1);

    if (dtp_is_add(dtp_ahead(p))) {
        dt_trace("term |",depth, );

        if (dtp_match_sym(dtp_step(p),'+')) 
            is_plus[OP_ADD_ONCE] = true;
         
        // r = dtp_factor(p, depth + 1);
        dtp_factor(p, depth + 1);
        // sb_merge(&before, after);

        // *after = before;
        if(is_plus[OP_ADD_ONCE]) 
            sb_appendf(sb, "\tadd\n"); else 
            sb_appendf(sb, "\tsub\n");


        // SECOND ADD 
        if (dtp_is_add(dtp_ahead(p))) {

            if (dtp_match_sym(dtp_step(p),'+')) 
                is_plus[OP_ADD_MANY] = true;
           
            // rr = dtp_term(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_TERM);

            dtp_term(p, depth + 1);

            if(is_plus[OP_ADD_MANY]) 
                sb_appendf(sb, "\tadd\n"); else 
                sb_appendf(sb, "\tsub\n");

            // DtNode* result = dtp_branch(p, l, r, NK_TERM);


            // if(NKP_IS_ADD * is_plus[OP_ADD_ONCE]) {
            //
            // }
            // return result;
        }
    }
    // p->output = before;
    // free(sb->items);
    return true;
}

bool dtp_factor(DtParser* p, int depth) {

    StringBuilder* sb = &p->output;

    enum {
        OP_MUL_ONCE,
        OP_MUL_MANY,
    };

    bool is_mul[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    //l = dtp_primary(p, depth + 1);
    // l = dtp_unary(p, depth + 1);
    dtp_unary(p, depth + 1);

    if (dtp_is_mul(dtp_ahead(p))) {
        dt_trace("factor   |",depth, );

        if (dtp_match_sym(dtp_step(p),'*')) 
            is_mul[OP_MUL_ONCE] = true;
        //r = dtp_primary(p, depth + 1);
        // r = dtp_unary(p, depth + 1);
        dtp_unary(p, depth + 1);


        if(is_mul[OP_MUL_ONCE]) 
            sb_appendf(sb, "\tmul\n"); else 
            sb_appendf(sb, "\tdiv\n");

        if (dtp_is_mul(dtp_ahead(p))) {
        
            if (dtp_match_sym(dtp_step(p),'*')) 
                is_mul[OP_MUL_MANY] = true;
         
            // rr = dtp_factor(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_FACTOR);

            dtp_factor(p, depth + 1);

            if(is_mul[OP_MUL_MANY]) 
                sb_appendf(sb, "\tmul\n"); else 
                sb_appendf(sb, "\tdiv\n");

            // r->properties |= NKP_IS_MUL * is_mul[OP_MUL_MANY];
        }

        // DtNode* result = dtp_branch(p, l, r, NK_FACTOR);
        // result->properties |= NKP_IS_MUL * is_mul[OP_MUL_ONCE];
        // return result;
    }

    return true;
}

bool dtp_unary(DtParser* p, int depth) {

    DtNode* self = 0;
    if (dtp_is_unary(dtp_ahead(p))) {
        dt_trace("unary",depth, );
        dtp_step(p); // skip unary
        dtp_unary(p, depth + 1);
        // self = dtp_unary(p, depth + 1);
        // self->properties |= NKP_HAS_UNARY;
    } else 
        // self = dtp_primary(p, depth + 1);
        dtp_primary(p, depth + 1);
    return self;
}



bool dtp_value(DtParser* p, int depth) {

    StringBuilder* sb = &p->output;

    // DtNode* self = dtp_node_new(p);
    //Token*  current_token = &(p->current_token);
    Token   name = {0};
    bool match_true = 0;

    switch((name = dtp_step(p)).kind) {
        case TOKEN_KIND_LITERALL_INTEGER: 
            // TODO: search and replace foldable constants
            sb_appendf(sb, "\tpush %i\n", name.data.as_int); 
            // self->kind       = NK_INTLIT;
            // self->identifier = name;
            break;

        case TOKEN_KIND_LITERALL_FLOAT: 
            // self->kind         = NK_FLTLIT;
            // self->identifier   = name;
            break;

        case TOKEN_KIND_LITERALL_STRING:
            // self->kind         = NK_STRLIT;
            // self->identifier   = name;
            break;

        case TOKEN_KIND_WORD: 
            if ( 
                (match_true = (dtp_match_str(name, "true"))) || 
                dtp_match_str(name, "false")
            ){
                // self->kind = NK_BOOLIT;
                // self->identifier = name;
                break; 
            }

            // self->kind = NK_IDENTIFIER;
            // self->identifier = name; 
            
            if (dtp_match_sym(dtp_ahead(p), '(')) {
                // self->kind = NK_FUNCTION_CALL;
                dtp_function_call(p, depth + 1);
                // return dtp_function_call(p, depth + 1);
            }
            break;

        default:
            assert(0 && "Expected int, float or name indent (variable or function)");
    }

    dt_trace("value:", depth, "%s", Token_text_cstr(name));
    return true;
}


// TODO: implement negative numbers
bool dtp_primary(DtParser* p, int depth) {
    // DtNode *self = 0;// *r, *rr;

    if (dtp_match_sym(dtp_ahead(p), '(')) {
        dt_trace("primary", depth, );
        dtp_expect_sym(p, dtp_step(p), '(', "Expected '('");
        
        dtp_equality(p, depth + 1);
        // self = dtp_equality(p, depth + 1);
       
        // TODO: URGENT: might have no effect or be a break-point  
#if 0
        if (!self) {
            self->kind = NK_TERM;
        }
#endif 

        dtp_expect_sym(p, dtp_step(p), ')', "Expected ')'");
        return true;
    } else if (dtp_is_value(dtp_ahead(p))) {
        return dtp_value(p, depth + 1);
    } else 
        // TODO: handle post expression symbols,
        // error if next token is not a valid post-expression token.
        printf("edging at dtp_prime(DtParser*, int) with: %s", 
                Token_temp_cstr(p->current_token)
                );//assert(0 && "Failed to parse prime");
    return true;
}


bool dtp_expression(DtParser* p, int depth) {
    dt_trace("expr",depth, );
    IGNORE_VALUE depth;
    // DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};

    // DtNode* node = dtp_equality(p, depth + 1);
    dtp_equality(p, depth + 1);
    //Token value = dtp_step(p);
    //dtp_expect_kind(value, TOKEN_KIND_literall_integer, "TODO: add more the just integers for expression");
    
    // self->kind = NK_EXPRESSION;
    // return dtp_node_append(self, node);
    return true;
}



bool dtp_array(DtParser *p, int depth) {

    dt_trace("array",depth, );
    // DtNode* self = dtp_node_new(p);
    // DtNode* node = {0};

    dtp_expect_sym (p, dtp_step(p), '[', "Expected '['");
    
dtp_array_next:;
    switch (dtp_ahead(p).kind) {
        
        case TOKEN_KIND_LITERALL_INTEGER:
        case TOKEN_KIND_LITERALL_FLOAT:
            // dtp_node_append(self, (node = dtp_expression(p, depth + 1)));
            dtp_expression(p, depth + 1);
            goto dtp_array_next;
            break;

        case TOKEN_KIND_LITERALL_STRING:
            // dtp_node_append(self, (node = dtp_string(p, depth + 1)));
            dtp_string(p, depth + 1);
            goto dtp_array_next;
            break;

        case TOKEN_KIND_SYMBOL:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            }             
            break;

        default: assert(0); break;
    }

    dtp_expect_sym(p, dtp_step(p), ']', "Expected ']' at the end of the array.");

    // self->kind = NK_ARRAY;

    return true;
}

bool dtp_variable(DtParser* p, int depth) {
    // DtNode* self = 0;//dtp_node_new(p);
    Token name = {0};
    //Token lhs = {0};

    name = dtp_step(p);
    dtp_expect_kind (p, name, TOKEN_KIND_WORD, "EXPECTED word");
    dtp_expect_sym  (p, dtp_step(p), '=', "Expected '='");
    

    dt_trace("variable",depth, "%s", Token_text_cstr(name));

    // self = dtp_rvalue(p, depth++);
    dtp_rvalue(p, depth++);
    // if (!self) {
    //     dtp_error_token( dtp_step(p), "Failed to parse variable");
    //     return NULL;
    // }
    // self->kind = NK_VARIABLE;
    // self->identifier = name;

    return true;
}

bool dtp_rvalue(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p);
    // DtNode* node = {0};
    Token lhs = {0};
    
    lhs = p->current_token;
    // self->kind = NK_RVALUE;
    switch (dtp_ahead(p).kind) {
        /*
        case TOKEN_KIND_literall_string:
            dtp_node_append(self, (node = dtp_string(p, depth + 1)));
            break;
            */
        case TOKEN_KIND_LITERALL_STRING:
        case TOKEN_KIND_LITERALL_INTEGER:
        case TOKEN_KIND_LITERALL_FLOAT:
        case TOKEN_KIND_SYMBOL:
        case TOKEN_KIND_WORD:
            {
                Token ahead = dtp_ahead(p);
                if (dtp_match_sym(ahead, '[')) {
                    dtp_array(p, depth + 1);
                    // if ((node = dtp_array(p, depth + 1)))
                    //     dtp_node_append(self, node);
                } else if (dtp_match_sym(ahead,'{')) {
                    dtp_object(p, depth + 1);
                    // if ((node = dtp_object(p, depth + 1))) {
                    //     dtp_node_append(self, node);
                    // } else {
                    //     //return NULL;
                    // }
                } else if (dtp_is_expression(ahead)) {
                    // dtp_node_append(self,(node = dtp_expression(p, depth + 1))); 
                    dtp_expression(p, depth + 1); 
                } else {
                    dtp_error_token(lhs, "expected value of any kind");
                    return false;
                }
            } break;

        default: assert(0); break;
    }


    return true;
}


bool dtp_parse(DtParser* p, int depth) {

    dt_trace("ROOT:", depth, );

    // DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    Token name = {0};
    (void) name;

    // self->kind = NK_ROOT;
    while(dtp_have_tokens(p)) {
        //dtp_load_tokens_buffer(p);
        Token t     = dtp_step(p);//dtp_aheadc(p, 0);
        Token nt    = dtp_ahead(p);

        //dtp_print_token_buffer(*p);
        //printf("%i, %i\n", t.kind, nt.kind);

        // we creating a type
        if(dtp_match_str(t, "type")) {
            dtp_abort(p, "TODO: implement type keyword");
        } else if (dtp_match_str(t, "include")) {
            dtp_abort(p, "TODO: implement include keyword");

        }

        /*
        else if (dtp_match_sym(t, '{')) {
            DtNode* b = dtp_block(p, depth+1, false);
            dtp_node_append(self, b);
        } 
        */

        else if (dtp_match_kind(t, TOKEN_KIND_WORD)) {
            name = t;
            if (dtp_valid_token(nt) && dtp_match_sym(nt, '(')) {
                //dtp_step(p);
                // DtNode* f = dtp_function_declaration(p,depth+1);
                dtp_function_declaration(p,depth+1);
                // if(!f) {
                //     dtp_error_token(nt, "Failed to parse function declaration");
                //     return f;
                // }

                // TODO: replace this and remove dtv_from_token function
                //f->value = dtv_from_token(name, VT_FUNCTION);
                // dtp_node_append(self, f);
                
            } 

            else {
                // dtp_node_append(self, dtp_statement(p, depth+1));
                dtp_statement(p, depth+1);
            }

            //else return dtp_abort(p, "Expected symbols after");

        } else if (!dtp_have_tokens(p)) {
            return true;
        }
        else {
            //printf("failed with token %s\n", Token_temp_cstr(t));
            dtp_abort(p, "Unexpected high level statement");
        }
    }
    return true;
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
