#include "declarations.h"
//
//  TOKENIZER
//

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
    return (t.kind == TOKEN_KIND_WORD || t.kind == TOKEN_KIND_LITERALL_STRING ) &&
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
	hc_loop(i, t.token_table_count) {
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
	memcpy(&i,str,min(sizeof(int),strlen(str)));
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
		.kind = TOKEN_KIND_WORD,
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
		result.kind = TOKEN_KIND_LITERALL_FLOAT;
		result.data.as_float = atof(scratch);
	} else {
		result.kind = TOKEN_KIND_LITERALL_INTEGER;
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
		.kind = TOKEN_KIND_LITERALL_STRING,
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
		case TOKEN_KIND_NULL:
			strcpy(cstr, "(null)"); 
			break;
		case TOKEN_KIND_SYMBOL:
			sprintf(cstr, "'%c'", t.data.as_symbol); 
			break;
		case TOKEN_KIND_WORD:
			strncpy(cstr, t.data.as_word.data, t.data.as_word.length);
			break;
		case TOKEN_KIND_LITERALL_INTEGER:
			snprintf(cstr,
					TEMP_CSTR_LENGTH,
					"%i",t.data.as_int);
			break;
		case TOKEN_KIND_LITERALL_FLOAT:
			snprintf(cstr,
					TEMP_CSTR_LENGTH,
					"%f",t.data.as_float);
			break;
		case TOKEN_KIND_LITERALL_STRING:
			{
				strcat(cstr,"\"");
				strncat(cstr, t.data.as_word.data, min(t.data.as_word.length,TEMP_CSTR_LENGTH-2));
				strcat(cstr,"\"");
			}
			break;
		case TOKEN_KIND_EOL:
			sprintf(cstr, "(EOL)");
			break;
		case TOKEN_KIND_EOF:
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


TknSlice tkn_as_slice(Token t) {
    assert(t.kind == TOKEN_KIND_WORD || t.kind == TOKEN_KIND_LITERALL_STRING);
    return t.data.as_word;
}

const char* token_as_cstr(Token t)  {
    static char temp[DT_SCRATCH_BUFFER_SIZE];
    memset(temp, 0, sizeof(temp));
    TknSlice str;
	switch(t.kind) {
		case TOKEN_KIND_NULL:
			strcpy(temp, "(null)"); 
			break;
		case TOKEN_KIND_SYMBOL:
			sprintf(temp, "%c", t.data.as_symbol); 
			break;
		case TOKEN_KIND_WORD:
            str = tkn_as_slice(t);
            assert(str.length < DT_SCRATCH_BUFFER_SIZE - 1);
            memcpy(temp, str.data, str.length);
			break;
		case TOKEN_KIND_LITERALL_INTEGER:
			sprintf(temp,"%i",t.data.as_int);
			break;
		case TOKEN_KIND_LITERALL_FLOAT:
			sprintf(temp,"%f",t.data.as_float);
			break;
		case TOKEN_KIND_LITERALL_STRING:
			{
                str = tkn_as_slice(t);
                assert(str.length < DT_SCRATCH_BUFFER_SIZE - 1);
				strcat (temp,"\"");
				strncat(temp, str.data, min(str.length,TEMP_CSTR_LENGTH-2));
				strcat (temp,"\"");
			}
			break;
		case TOKEN_KIND_EOL:
			sprintf(temp, "(EOL)");
			break;
		case TOKEN_KIND_EOF:
			sprintf(temp, "(EOF)");
			break;
		default:
            assert(0);
		
    }

    return temp;
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
				? TOKEN_KIND_WORD : TOKEN_KIND_SYMBOL;
			
			if (esz > 1) {
				result.kind = TOKEN_KIND_WORD;
				result.data.as_word = (TknSlice) {
                    .data = e.txt,
                    .length = esz
                };
			} else {
				result.kind = TOKEN_KIND_SYMBOL;
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
			result.kind = TOKEN_KIND_EOL;
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
		result.kind = TOKEN_KIND_EOF;

	return result;
}

// run tokenizer and save everything into growable stack
void Tokenizer_run(Tokenizer* t) {
	Token token = {0};
	Tokens* s = &(t->tokens);
	while( (token = Tokenizer_next_token(t)).kind != TOKEN_KIND_EOF ) {
		da_append(s,token);
	}
}

