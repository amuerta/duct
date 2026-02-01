#include "declarations.h"

// since i use my generic tokenizer here,
// i need to initilize Duct tokenizer 
// with specific to it parameters.
Tokenizer DtTokenizer_init(const char* text, size_t len) {
    Tokenizer lexer = {0};
    static char scratch_buffer[512];

    static TokenTableEntry token_table[] = {
        { "$",   TI_LIST_OPERATOR},
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

String token_to_string(Token t) {
    assert(t.kind == TOKEN_KIND_WORD);
    String s = {
        t.data.as_word.data,
        t.data.as_word.length
    };
    return s;
}


bool dtp_is_ok(DtParseResult r) { 
    return r.kind >= DT_PNKIND_OK; 
}

DtParseResult dtp_ok(void) {
    const DtParseResult r = {.kind = DT_PNKIND_OK }; return r;
}

DtParseResult dtp_error(void) {
    const DtParseResult r = {0}; return r;
}

#define dtp_ok_or_abort(e)\
    if (!dtp_is_ok(e)) return e; 

// automatically append generated insturctions from parse result
#define dtp_on_ok_register_result_or_abort(this, other, e) \
    dtp_ok_or_abort( (other = (e)) ) else \
    this.instructions_generated += other.instructions_generated;

#define dtp_trace_recursion(p, name, depth, ...) if(p->tracef) {\
    fprintf(p->tracef, "%*.s> %s", depth, "\t", name);\
    fprintf(p->tracef, "%s", dt_temp_format(""__VA_ARGS__));\
    fprintf(p->tracef, "\n");\
}

#define dtp_va_string(...) #__VA_ARGS__

#define dtp_emmit_asm_text(sb, this, text)  \
    assert(text && "dtp_emmit_asm_text has no asm body");\
    sb_append(sb, text);       

#define dtp_emmit_asm(sb, this, ...)  \
    this.instructions_generated ++;         \
    assert(strlen(dtp_va_string(__VA_ARGS__))\
            && "dtp_emmit_asm_text has no asm body");\
    sb_appendf(sb, __VA_ARGS__);       

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


bool dtp_match_str(Token t, const char* text) {
    return 
        dtp_valid_token(t) &&
        t.kind == TOKEN_KIND_WORD && 
        t.data.as_word.data &&
        strlen(text) == t.data.as_word.length &&
        strncmp(t.data.as_word.data, text, t.data.as_word.length) == 0;
}

bool dtp_match_sym(Token t,char sym) {
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

void dtp_error_message(FILE* out, const char* message, int r, int c) {
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


DtParseResult dtp_error_token(FILE* out, Token t, const char* message) {
    dtp_error_message(out, message, t.row, t.col); 
    return dtp_error();
}

#define dtp_expect_str(P,T,S,E) (!dtp_match_str(T, S)) ? \
    dtp_error_token((P)->errorf, T, E) : dtp_ok();

#define dtp_expect_kind(P,T,K,E) if (!dtp_match_kind(T, K)) {\
    assert(0);\
    dtp_error_token((P)->errorf, T, E);\
    return dtp_error();\
}
    
#define dtp_expect_sym(P,T,S,E) if (!dtp_match_sym(T, S)) {\
    assert(0);\
    dtp_error_token((P)->errorf, T, E);\
    return dtp_error();\
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

const int PARENT_IS_EXPR = 1;

DtParseResult dtp_expression            (DtParser* p, int depth) ;
DtParseResult dtp_variable              (DtParser* p, int depth) ;
DtParseResult dtp_rvalue                (DtParser* p, int depth) ;
DtParseResult dtp_block                 (DtParser* p, int depth, bool step_at_last) ;
DtParseResult dtp_function_declaration  (DtParser* p, int depth) ;
DtParseResult dtp_function_call         (DtParser* p, int depth, bool parent_is_expression) ;
DtParseResult dtp_statement             (DtParser* p, int depth) ;
DtParseResult dtp_if_statement          (DtParser* p, int depth) ;
DtParseResult dtp_while_statement       (DtParser* p, int depth) ;
DtParseResult dtp_list                  (DtParser *p, int depth);
DtParseResult dtp_array                 (DtParser *p, int depth);
void          dtp_optional_semicolon    (DtParser* p);

DtParseResult dtp_comparison  (DtParser*, int);
DtParseResult dtp_equality    (DtParser*, int);
DtParseResult dtp_factor      (DtParser*, int);
DtParseResult dtp_term        (DtParser*, int);
DtParseResult dtp_primary     (DtParser*, int);
DtParseResult dtp_unary       (DtParser*, int);


const char* dtp_unique_label(void) {
    static int counter;
    static char temp_buffer[256];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    sprintf(temp_buffer, "label%06i", counter++);
    return temp_buffer;
}

void dtp_optional_semicolon(DtParser* p) {
try_semicolon_again: ;
    Token next = dtp_ahead(p);
    if(dtp_match_sym(next, ';')) {
        dtp_step(p);
        goto try_semicolon_again;
    }
}


// TODO: FIX DIRTY INSTRUCTION GENERATION HACKS HERE
DtParseResult dtp_while_statement(DtParser* p, int depth) {
    StringBuilder* sb = &(p->output);
    DtParseResult other_res = dtp_ok(),
                  this_res  = dtp_ok();
    dtp_trace_recursion(p, "while:", depth);

    Token keyword = dtp_step(p);
    dtp_expect_str(p, keyword, "while", "Expected while keyword");
    
    // label at zero-th location is not allowed, so 
    // just pad with nop
    dtp_emmit_asm(sb, this_res, "\tnop\n");

    // set the label
    char label[256] = {0};
    memcpy(label, dtp_unique_label(), sizeof(label));
    sb_appendf(sb, "\t@%s\n", label);

    size_t block_instructions = 0;

    // while expression
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_expression(p, depth + 1));

    // TODO: extract this non-sense into 
    // some single functions or multiple functions
    // that allocate instruction for jumps
    const char* padding = "      ";
    size_t assembly_address_text_pen = 0;
    sb_appendf(sb, "\tijif ", padding);
    assembly_address_text_pen = sb->count;
    sb_appendf(sb, "%s \n", padding);
    this_res.instructions_generated++;

    // pop condition result
    // sb_appendf(sb, "\tpop\n"); 
    // this_res.instructions_generated++;
    dtp_emmit_asm(sb, this_res, "\tpop\n");
    block_instructions++;

    // block
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_block(p, depth + 1, true));
    block_instructions += other_res.instructions_generated;

    char* addr = sb->items + assembly_address_text_pen;
    block_instructions++;
    int n = snprintf(addr, strlen(padding)-1, "%lu", block_instructions);
    addr[n] = ' ';

    dtp_emmit_asm(sb, this_res, "\tjmp %s\n", label);
    // sb_appendf(sb, "\tjmp %s\n", label);
    // this_res.instructions_generated++;

    return this_res;
} 

// TODO: FIX DIRTY INSTRUCTION GENERATION HACKS HERE
DtParseResult dtp_if_statement            (DtParser* p, int depth)  {
    enum { 
        BRANCH_IF,
        BRANCH_ELIF, 
        BRANCH_ELSE, 
    } branch_type = BRANCH_IF;
    dtp_trace_recursion(p, "if:", depth);

    StringBuilder* sb = &(p->output);
    DtParseResult other_res = dtp_ok(),
                  this_res  = dtp_ok();
    size_t additional_instructions = 0;

    char label[256] = {0};
    memcpy(label, dtp_unique_label(), sizeof(label));

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
    
    // expression
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_expression(p, depth+1));

    // lazy hack, i reserve some amount for the line text
    // and then place actual address to jump over there.
    // TODO: USE LABELS FOR IJIF TOO!
    const char* padding = "      ";
    size_t assembly_address_text_pen = 0;

    switch(branch_type) {
        case BRANCH_IF: 
        case BRANCH_ELIF: 
            sb_appendf(sb, "\tijif ", padding);
            assembly_address_text_pen = sb->count;
            sb_appendf(sb, "%s \n", padding);
            additional_instructions++;
            
            sb_appendf(sb, "\tpop\n"); 
            additional_instructions++;

            // sb_appendf(sb, "\tpop\n");
            // additional_instructions++;
            break;

        case BRANCH_ELSE: 
            break;
    }

    // __asm__("int3");

    // block
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_block(p, depth + 1, true));

    // TODO: FIX UNSAFE OVERWRITE POSSIBILITY!
    // apply address to instruction where pen was saved.
    // additional_instructions++;
    
    if (branch_type != BRANCH_ELSE) {
        size_t instructions_in_block = other_res.instructions_generated + additional_instructions; 
        char* addr = sb->items + assembly_address_text_pen;
        int n = snprintf(addr, strlen(padding)-1, "%lu", instructions_in_block);
        addr[n] = ' ';
        // generate jump instruction
        sb_appendf(sb, "\tjmp %s\n", label);
        sb_appendf(sb, "\tpop\n"); 
    }
    // sb_appendf(sb, "\tnop\n", label);

    //BREAKPOINT();
    
    if (dtp_match_str(dtp_ahead(p), "else")) {
        additional_instructions = 0;
        if (dtp_match_str(dtp_aheadc(p,2), "if")) 
            branch_type = BRANCH_ELIF;
        else 
            branch_type = BRANCH_ELSE;
        //dtp_step(p); // ? not needed, i just got confused
        goto parse_branch_again;
    }


    sb_appendf(sb, "\t@%s\n", label);
    // sb_appendf(sb, "\tpop\n");
    return this_res;
}

void dtp_add_instructions(DtParseResult* this, DtParseResult other) {
    this->instructions_generated += other.instructions_generated;
}


// TODO: add whole bunch of stuff like:
// - if
// - for
// - loop
// - switch
// + return
// etc...
DtParseResult dtp_statement(DtParser* p, int depth) {
    StringBuilder* sb = &p->output;
    DtParseResult this_res = dtp_ok(),
                  other_res = dtp_ok();
    dtp_trace_recursion(p, "statement:", depth);
    // 'return'
    if (dtp_match_str(dtp_ahead(p), "return")) {
        // remember to step
        dtp_step(p);
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_expression(p, depth + 1));
        dtp_optional_semicolon(p);
        
        dtp_emmit_asm(sb, this_res, "\tret\n");
    } 

    //
    // BRANCHING:
    //
    // + if
    else if(dtp_match_str(dtp_ahead(p),"if")) {
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_if_statement(p, depth + 1));
    }


    //
    // LOOPS:
    //
    // + while
    else if(dtp_match_str(dtp_ahead(p),"while")) {
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_while_statement(p, depth + 1));
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
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, 
             dtp_function_call(p, depth + 1, !PARENT_IS_EXPR));
        dtp_optional_semicolon(p);

        // handle null return
        dtp_emmit_asm(sb, this_res, "\tpop\n");
    } 

    // variable
    else {
        Token before = p->current_token;//dtp_ahead(p);
        
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_variable(p, depth + 1));
        
        dtp_optional_semicolon(p);
    }
    return this_res;
}

DtParseResult dtp_function_return_type(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p); 
    // self->kind = NK_TYPE; 
    IGNORE_VALUE depth;
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work
    
    Token type = {0};
    dtp_expect_kind(p,(type = dtp_step(p)),TOKEN_KIND_WORD, "Expected return type");
    // self->identifier = type;
    
    return dtp_ok();
}

DtParseResult dtp_block(DtParser* p, int depth, bool step_at_last) {
    dtp_trace_recursion(p, "block:", depth);
    DtParseResult this_res  = dtp_ok(),
                  other_res = dtp_ok();

    // DtNode* self = dtp_node_new(p); 
    // DtNode* statement;
    // self->kind = NK_BLOCK; 
    // Splitter node kind is used to indicate LHS and RHS between many items
    // and prevents creation of "dummy" node to do orginization work

    // NOTE: this parsing code often caused "kms" comments.
    dtp_expect_sym(p, dtp_step(p), '{', "Expected '{' ");
    while((!dtp_match_sym(dtp_ahead(p), '}'))) {
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_statement(p, depth + 1));
    }

    Token last;
    if (step_at_last) last = dtp_step(p); else last = dtp_ahead(p);
    dtp_expect_sym(p, last, '}', "Expected '}' ");
    return this_res;
}


DtParseResult dtp_symbol_declaration(DtParser* p, int depth) {
    IGNORE_VALUE depth;

    StringBuilder* sb = &(p->output);
    Token var  = {0};
    // Token type = {0};
    // DtNode* self = 0; 

    dtp_expect_kind(p, (var = dtp_step(p)), TOKEN_KIND_WORD, "Expected name of variable");
    TknSlice s = tkn_as_slice(var);
    sb_appendf(sb, "%.*s ", (int)s.length, s.data);
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
    return dtp_ok();
}

DtParseResult dtp_function_declaration(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p);
    // DtNode* args = dtp_node_new(p);
    //Token  name = {0};

    //DtNode* node = {0};
    // self->kind = NK_FUNCTION_DECL;
    // args->kind = NK_FUNCTION_ARGS;
    
    StringBuilder* sb = &p->output;
    dtp_expect_kind (p, p->current_token, TOKEN_KIND_WORD, "Expected word");
    // self->identifier = p->current_token;
    size_t argc = 0;
    
    Token  name = p->current_token;
    String name_string = token_to_string(p->current_token);

    dtp_trace_recursion(p, "function:", depth, "%s", token_as_cstr(p->current_token));
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
            dtp_ok_or_abort(dtp_symbol_declaration(p, depth + 1));
            argc++;
            goto dtp_array_next;
            break;

        case TOKEN_KIND_SYMBOL:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            }             
            break;

        default: 
            return dtp_error();
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


    DtParserFunctionSymbol fn_decl = {
        .name = name_string,
        .argc = argc,
    };

    // TODO: proper error logging.
    if(!dtp_validate_function(&p->function_symbols, fn_decl)) 
        assert(0 && "function was already used with different signature");

    return dtp_ok();
}

DtParseResult dtp_function_call(DtParser* p, int depth, bool parent_is_expression) {
    // DtNode* self = dtp_node_new(p);
    //DtNode* node = {0};
    // self->kind = NK_FUNCTION_CALL;
    // self->identifier = p->current_token;
    // self->source_location = p->current_token;
    
    DtParseResult this_res  = dtp_ok(),
                  other_res = dtp_ok();

    StringBuilder* sb = &(p->output);
    Token name = {0};
    size_t argc = 0;

    dtp_expect_kind (p,p->current_token, TOKEN_KIND_WORD, "Expected word");
    name = p->current_token;
    dtp_trace_recursion(p, "fncall ", depth, "'%s'", token_as_cstr(name));
    
    dtp_expect_sym  (p,dtp_step(p), '(', "Expected '('");

 dtp_array_next:;
    if (dtp_match_sym(dtp_ahead(p), ',')) {
        dtp_step(p);
        goto dtp_array_next;
    } else if (dtp_match_sym(dtp_ahead(p), ')'))
        ; // finish cycling
    else {
        argc++;
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_expression(p, depth+1));
        goto dtp_array_next;
    }
    
    dtp_emmit_asm(sb, this_res, "\tcall %.*s\n", 
            tkn_fmt(tkn_as_slice(name)));

    // TODO: proper error logging.
    DtParserFunctionSymbol fn_decl = {
        .name = token_to_string(name),
        .argc = argc,
    };
    if(!dtp_validate_function(&p->function_symbols, fn_decl)) 
        assert(0 && "function was already defined with different signature");

    dtp_expect_sym(p, dtp_step(p), ')', "Expected ')' at the end of the array.");
    return this_res;
}

DtParseResult dtp_object(DtParser* p, int depth) {
    IGNORE_VALUE depth;
    // DtNode* self = dtp_node_new(p);
    // DtNode* node = 0;
    Token ahead = {0};

    dtp_trace_recursion(p, "object:", depth);
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
    return dtp_error();
}

// DtNode* dtp_string(DtParser* p, int depth) {
//     IGNORE_VALUE depth;
//     DtNode* self = dtp_node_new(p);
//     //DtNode* node = {0};
//     Token value = dtp_step(p);
//     dtp_expect_kind(p, value, TOKEN_KIND_LITERALL_STRING, "Expected string literall");
//    
//     self->kind = NK_STRLIT;
//     return self;
// }

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
        || dtp_match_sym(t, '(')
        || dtp_match_sym(t, '-');
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

// DtNode* dtp_branch(DtParser* p, DtNode* l, DtNode* r, DtNodeKind kind) {
//     DtNode* self = dtp_node_new(p);
//     dtp_node_append(self,l);
//     dtp_node_append(self,r);
//     if (self)
//         self->kind = kind;
//     return self;
// }

DtParseResult dtp_equality(DtParser* p, int depth) {
    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };

    StringBuilder*      sb  = &(p->output);
    DtParseResult this_res  = dtp_ok(),
                  other_res = dtp_ok();

    bool is_equality[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_comparison(p, depth + 1);
    
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_comparison(p,depth+1))
 
    if (dtp_is_eql(dtp_ahead(p))) {
        dtp_trace_recursion(p, "eq |",depth);
        
        if (dtp_match_str(dtp_step(p),"==")) 
            is_equality[OP_CMP_ONCE] = true;

        
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_factor(p,depth+1))


        if(is_equality[OP_CMP_ONCE]) {
            dtp_emmit_asm(sb, this_res, "\teq\n");
        } else {
            dtp_emmit_asm(sb, this_res, "\teq\n");
            dtp_emmit_asm(sb, this_res, "\tnot\n");
        }

        if (dtp_is_eql(dtp_ahead(p))) {
   
            if (dtp_match_str(dtp_step(p),"==")) 
                is_equality[OP_CMP_MANY] = true;
            
            // rr = dtp_term(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_EQALITY);
            
            dtp_on_ok_register_result_or_abort
                (this_res, other_res, dtp_term(p,depth+1));

            if(is_equality[OP_CMP_MANY]) {
                dtp_emmit_asm(sb, this_res, "\teq\n");
            } else {
                dtp_emmit_asm(sb, this_res, "\teq\n");
                dtp_emmit_asm(sb, this_res, "\tnot\n");
            }

            // r->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_MANY];
        }
        // DtNode* result = dtp_branch(p, l, r, NK_EQALITY);
        // dtp_branch(p, l, r, NK_EQALITY);
        // result->properties |= NKP_IS_EQALITY * is_equality[OP_CMP_ONCE];
        // return result;
    }
    return this_res;
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

DtParseResult dtp_comparison(DtParser* p, int depth) {

    enum {
        OP_CMP_ONCE,
        OP_CMP_MANY,
    };
    bool is_cmp_eq  [2] = {0};
    bool is_cmp_gt  [2] = {0};

    StringBuilder* sb = &(p->output);
    DtParseResult this_res = dtp_ok(),
                  other_res = dtp_ok();

    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_term(p, depth + 1);
    
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_term(p, depth + 1));

    if (dtp_is_cmp(dtp_ahead(p))) {
        dtp_trace_recursion(p, "compare |",depth);

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
        
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_term(p, depth + 1));

        if(is_cmp_gt[OP_CMP_ONCE]) {
            if(is_cmp_eq[OP_CMP_ONCE]) {dtp_emmit_asm(sb, this_res, "\tgte\n");}
            else                       {dtp_emmit_asm(sb, this_res, "\tgt\n") ;}
        } else {
            if(is_cmp_eq[OP_CMP_ONCE]) {dtp_emmit_asm(sb, this_res, "\tlte\n");}
            else                       {dtp_emmit_asm(sb, this_res, "\tlt\n") ;}
        }

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
            dtp_on_ok_register_result_or_abort
                (this_res, other_res, dtp_comparison(p, depth + 1));

            if(is_cmp_gt[OP_CMP_MANY]) {
                if(is_cmp_eq[OP_CMP_MANY]){ dtp_emmit_asm(sb, this_res, "\tgte\n");}
                else                      { dtp_emmit_asm(sb, this_res, "\tgt\n") ;}
            } else {
                if(is_cmp_eq[OP_CMP_MANY]){ dtp_emmit_asm(sb, this_res, "\tlte\n");}
                else                      { dtp_emmit_asm(sb, this_res, "\tlt\n") ;}
            }

            // r->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_MANY];
            // r->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_MANY];
        }

        // DtNode* result = dtp_branch(p, l, r, NK_COMPARISON);
        // result->properties |= NKP_IS_CMP_EQ  * is_cmp_eq[OP_CMP_ONCE];
        // result->properties |= NKP_IS_CMP_GT  * is_cmp_gt[OP_CMP_ONCE];
        // return result;
    }

    return this_res;
}

void sb_merge(StringBuilder *dest, StringBuilder *src) {
    sb_appendf(dest, "%*.s", (int)src->count, src->items);
}


// TODO: fix breaking on terminating term.
DtParseResult dtp_term(DtParser* p, int depth) {
    // StringBuilder before = p->output;
    DtParseResult this_res  = dtp_ok();
    DtParseResult other_res = dtp_ok();
    StringBuilder* sb = &p->output;
    // memset(after, 0, sizeof(*after));

    enum {
        OP_ADD_ONCE,
        OP_ADD_MANY,
    };

    bool is_plus[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    // l = dtp_factor(p, depth + 1);
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_factor(p, depth + 1));

    if (dtp_is_add(dtp_ahead(p))) {
        dtp_trace_recursion(p, "term |",depth);

        if (dtp_match_sym(dtp_step(p),'+')) 
            is_plus[OP_ADD_ONCE] = true;
         
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_factor(p, depth + 1));
        // sb_merge(&before, after);

        // *after = before;
 
        if(is_plus[OP_ADD_ONCE]) 
        {dtp_emmit_asm(sb, this_res, "\tadd\n"); } else 
        {dtp_emmit_asm(sb, this_res, "\tsub\n"); }


        // SECOND ADD 
        if (dtp_is_add(dtp_ahead(p))) {

            if (dtp_match_sym(dtp_step(p),'+')) 
                is_plus[OP_ADD_MANY] = true;
           
            // rr = dtp_term(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_TERM);

            dtp_on_ok_register_result_or_abort
                (this_res, other_res, dtp_term(p, depth + 1));

            if(is_plus[OP_ADD_MANY]) 
            {dtp_emmit_asm(sb, this_res, "\tadd\n"); } else 
            {dtp_emmit_asm(sb, this_res, "\tsub\n"); }

            // DtNode* result = dtp_branch(p, l, r, NK_TERM);


            // if(NKP_IS_ADD * is_plus[OP_ADD_ONCE]) {
            //
            // }
            // return result;
        }
    }
    // p->output = before;
    // free(sb->items);
    return this_res;
}

DtParseResult dtp_factor(DtParser* p, int depth) {

    StringBuilder* sb = &p->output;

    DtParseResult other_res = dtp_ok();
    DtParseResult this_res = dtp_ok();

    enum {
        OP_MUL_ONCE,
        OP_MUL_MANY,
    };

    bool is_mul[2] = {0};
    // DtNode *l = 0, *r = 0, *rr = 0;
    //l = dtp_primary(p, depth + 1);
    // l = dtp_unary(p, depth + 1);
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_unary(p, depth + 1));

    if (dtp_is_mul(dtp_ahead(p))) {
        dtp_trace_recursion(p, "factor   |",depth);

        if (dtp_match_sym(dtp_step(p),'*')) 
            is_mul[OP_MUL_ONCE] = true;
        //r = dtp_primary(p, depth + 1);
        // r = dtp_unary(p, depth + 1);
        // dtp_unary(p, depth + 1);

        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_unary(p, depth + 1));

        if(is_mul[OP_MUL_ONCE]) 
        { dtp_emmit_asm(sb, this_res, "\tmul\n");} else 
        { dtp_emmit_asm(sb, this_res, "\tdiv\n");}

        if (dtp_is_mul(dtp_ahead(p))) {

            if (dtp_match_sym(dtp_step(p),'*')) 
                is_mul[OP_MUL_MANY] = true;

            // rr = dtp_factor(p, depth + 1);
            // r = dtp_branch(p, r, rr, NK_FACTOR);

            dtp_on_ok_register_result_or_abort
                (this_res, other_res, dtp_factor(p, depth + 1));

            if(is_mul[OP_MUL_MANY]) 
            { dtp_emmit_asm(sb, this_res, "\tmul\n");} else 
            { dtp_emmit_asm(sb, this_res, "\tdiv\n");}

            // r->properties |= NKP_IS_MUL * is_mul[OP_MUL_MANY];
        }

        // DtNode* result = dtp_branch(p, l, r, NK_FACTOR);
        // result->properties |= NKP_IS_MUL * is_mul[OP_MUL_ONCE];
        // return result;
    }

    return this_res;
}

DtParseResult dtp_unary(DtParser* p, int depth) {
    // DtNode* self = 0;
    DtParseResult this_res = dtp_ok(), 
                  other_res = dtp_ok();
    StringBuilder* sb = &(p->output);
    if (dtp_is_unary(dtp_ahead(p))) {
        dtp_trace_recursion(p, "unary",depth);
        dtp_step(p); // consume unary token
        
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_unary(p, depth+1));
        
        dtp_emmit_asm(sb, this_res, "\tpush -1\n");
        dtp_emmit_asm(sb, this_res, "\tmul    \n");

    } else { 
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_primary(p, depth+1));
    }
    return this_res;
}



DtParseResult dtp_value(DtParser* p, int depth) {
    StringBuilder* sb = &p->output;

    DtParseResult other_res = dtp_ok(),
                  this_res = dtp_ok();

    // DtNode* self = dtp_node_new(p);
    //Token*  current_token = &(p->current_token);
    Token   name = {0};
    bool match_true = 0;

    switch((name = dtp_step(p)).kind) {
        case TOKEN_KIND_LITERALL_INTEGER: 
            // TODO: search and replace foldable constants
            dtp_emmit_asm(sb, this_res, "\tpush %i\n", name.data.as_int);
            break;

        case TOKEN_KIND_LITERALL_FLOAT: 
            dtp_emmit_asm(sb, this_res, "\tpush %f\n", name.data.as_float);
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
            
            // if `word` `(` -> function
            if (dtp_match_sym(dtp_ahead(p), '(')) {
                // self->kind = NK_FUNCTION_CALL;
                dtp_on_ok_register_result_or_abort
                    (this_res, other_res, 
                     dtp_function_call(p, depth + 1, PARENT_IS_EXPR));
            } 
            // else its variable
            else {
                dtp_emmit_asm(sb, this_res, 
                        "\tload %.*s\n", tkn_fmt(tkn_as_slice(name))); 
            }
            break;

        default:
            assert(0 && "Expected int, float or name indent (variable or function)");
    }

    dtp_trace_recursion(p, "value: ", depth, "%s", token_as_cstr(name));
    return this_res;
}


// TODO: implement negative numbers
DtParseResult dtp_primary(DtParser* p, int depth) {
    // DtNode *self = 0;// *r, *rr;

    DtParseResult this_res  = dtp_ok(),
                  other_res = dtp_ok();

    if (dtp_match_sym(dtp_ahead(p), '(')) {
        dtp_trace_recursion(p, "primary", depth);
        dtp_expect_sym(p, dtp_step(p), '(', "Expected '('");
        
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_equality(p, depth + 1));
        
        dtp_expect_sym(p, dtp_step(p), ')', "Expected ')'");
        return this_res;
    } else if (dtp_is_value(dtp_ahead(p))) {
        dtp_on_ok_register_result_or_abort
            (this_res, other_res, dtp_value(p, depth + 1));
    } else {
        // TODO: handle post expression symbols,
        // error if next token is not a valid post-expression token.
        // NOTE: THIS PRINTING IS ANNOYING WHEN DEBUGGING.
        // printf("edging at dtp_prime(DtParser*, int) with: %s", 
        //         Token_temp_cstr(p->current_token)
        //         );
    };
    return this_res;
}


DtParseResult dtp_expression(DtParser* p, int depth) {
    dtp_trace_recursion(p, "expr",depth);
    IGNORE_VALUE depth;
    DtParseResult this_res = dtp_ok();

    dtp_ok_or_abort(this_res = dtp_equality(p, depth + 1));
    return this_res;
}


DtParseResult dtp_list(DtParser *p, int depth) {
    DtParseResult this_res  = dtp_ok(),
                  other_res = dtp_ok();
    dtp_trace_recursion(p, "list ", depth);
    dtp_expect_sym (p, dtp_step(p), '$', "Expected '$'");
    dtp_ok_or_abort(this_res = dtp_array(p, depth + 1));
    return this_res;
}

DtParseResult dtp_array(DtParser *p, int depth) {

    dtp_trace_recursion(p, "array",depth);
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
            // dtp_string(p, depth + 1);
            goto dtp_array_next;
            break;

        case TOKEN_KIND_SYMBOL:
            if (dtp_match_sym(dtp_ahead(p), ',')) {
                dtp_step(p);
                goto dtp_array_next;
            } else if (
                    dtp_match_sym(dtp_ahead(p), '$') |
                    dtp_match_sym(dtp_ahead(p), '[') |
                    dtp_match_sym(dtp_ahead(p), '{')
            ) {
                dtp_rvalue(p, depth + 1);
                goto dtp_array_next;
            }

            break;

        default: assert(0); break;
    }
    dtp_expect_sym(p, dtp_step(p), ']', "Expected ']' at the end of the array.");
    return dtp_ok();
}


DtParseResult dtp_variable(DtParser* p, int depth) {
    // DtNode* self = 0;//dtp_node_new(p);
    DtParseResult this_res = dtp_ok(),
                  other_res = dtp_ok();
    StringBuilder* sb = &(p->output);
    Token name = {0};
    //Token lhs = {0};

    name = dtp_step(p);
    dtp_expect_kind (p, name, TOKEN_KIND_WORD, "EXPECTED word");
    dtp_expect_sym  (p, dtp_step(p), '=', "Expected '='");
    
    dtp_trace_recursion(p, "variable", depth, " '%s'", token_as_cstr(name));

    // self = dtp_rvalue(p, depth++);
    dtp_on_ok_register_result_or_abort
        (this_res, other_res, dtp_rvalue(p, depth++));

    dtp_emmit_asm(sb, this_res, "\tstore %.*s\n", tkn_fmt(tkn_as_slice(name)));    this_res.instructions_generated++;
    return this_res;
}

DtParseResult dtp_rvalue(DtParser* p, int depth) {
    // DtNode* self = dtp_node_new(p);
    // DtNode* node = {0};
    Token lhs = {0};
    DtParseResult this_res = dtp_ok(),
                  other_res = dtp_ok();
    
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
                // array
                if (dtp_match_sym(ahead, '[')) {
                    dtp_array(p, depth + 1);
                    // if ((node = dtp_array(p, depth + 1)))
                    //     dtp_node_append(self, node);
                } 

                // list
                else if (dtp_match_sym(ahead, '$')) {
                    dtp_list(p, depth + 1);
                } 

                else if (dtp_match_sym(ahead,'{')) {
                    dtp_object(p, depth + 1);
                    // if ((node = dtp_object(p, depth + 1))) {
                    //     dtp_node_append(self, node);
                    // } else {
                    //     //return NULL;
                    // }
                } else if (dtp_is_expression(ahead)) {
                    // dtp_node_append(self,(node = dtp_expression(p, depth + 1))); 
                    dtp_on_ok_register_result_or_abort
                        (this_res, other_res, dtp_expression(p, depth + 1));
                } else {
                    return dtp_error();
                }
            } break;

        default: assert(0); break;
    }


    return this_res;
}


bool dtp_parse(DtParser* p, int depth) {
    dtp_trace_recursion(p, "ROOT:", depth);

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

// TODO: print literalls
// TODO: print expressions in parenthesis as if they where 
//          written by a person
//              E.G: 1 + ((10 / 2) * 3 )
//
// void dtp_print_ast(DtNode* node, int depth, FILE* output_redir) {
//     if (!node) return;
//     DtNode* list = node;
//     while(list) {
//         fprintf(output_redir, 
//                 "%*s%s: "
//                 "%c%s\t",
//                
//                 depth*4, "", 
//                 DT_NODE_KIND_STR[list->kind],
//
//                 (list->properties & NKP_HAS_UNARY) ? '-' : ' ',
//                 Token_text_cstr(list->identifier)
//         );
//
//         //fprintf(output_redir, " -> %i", list->value.type);
//
//         fprintf(output_redir, "\n");
//        
//         dtp_print_ast(list->children, depth+1, output_redir);
//         list = list->next;
//     }    
// }
