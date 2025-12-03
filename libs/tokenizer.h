#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// TODO: IMPLEMENT A STAND ALONE MODE WHERE STACK IS INCLUDED
// FOR FUTURE REUSE.
#include "stack.h"
#include "util.h"


#ifndef __TOKENIZER_H
#define __TOKENIZER_H


#define WIDE_SYMBOL_SZ 4

typedef unsigned char 	symbol_t;
typedef size_t index_t;
typedef unsigned int	wsymbol_t;
typedef const char*		cstr_t;
typedef unsigned int	tokenid_t;


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


#ifndef DT_MALLOC
#	define DT_MALLOC(S) malloc((S))
#endif

#ifndef DT_REALLOC
#	define DT_REALLOC(P,S) realloc((P),(S))
#endif

#ifndef DT_FREE
#	define DT_FREE(P) free((P))
#endif

#ifndef DT_ARRLEN 
#	define DT_ARRLEN(A) \
	(size_t)(sizeof(A) / sizeof(A[0]))
#endif

typedef struct { 
	char* 	data;
	size_t 	length;
} TknSlice;

typedef struct {
	const char* 	txt;
	tokenid_t		id;
} TokenTableEntry;

typedef enum {
	TokenKind_null,
	TokenKind_symbol,
	TokenKind_wide_symbol,
	TokenKind_word,
	TokenKind_literall_integer,
	TokenKind_literall_float,
	TokenKind_literall_string,
	TokenKind_EOL,
	TokenKind_EOF,
} TokenKind;

typedef struct {
	TokenKind 	kind;
	tokenid_t	id;
	union {
		symbol_t 	as_symbol;
		wsymbol_t	as_wide_symbol;
		TknSlice	as_word;
		int			as_int;
		float		as_float;
		bool		as_bool;
	}			data;
} Token;

typedef struct {
	char*		scratch_buffer;
	size_t		scratch_buffer_sz;

	const	char* 		target;
	size_t				target_length;
	size_t 				position;
	symbol_t 			string_quotes[2];

	TokenTableEntry*	token_table;
	size_t				token_table_count;


	Stack				tokens;
} Tokenizer;


void Tokenizer_init(
		Tokenizer* 			t, 
		TokenTableEntry* 	table,
		size_t				table_len,
		const char*			target,
		size_t				target_len,
		char*				scratch_buffer,
		symbol_t			quotations[2]) 
{
	assert(t && "Tokenizer pointer has to be valid");

	*t = (Tokenizer) {
		.scratch_buffer = scratch_buffer,
		.target 		= target,
		.target_length 	= target_len,
		.string_quotes 	= { quotations[0], quotations[1] },
		.token_table	= table,
		.token_table_count
						= table_len,
	};

	Stack* s = &(t->tokens);
	Stack_new(s,Token);

}

void Tokenizer_clear(Tokenizer* t) {
	assert(t && "Tokenizer has to be valid");
	Stack* s = &(t->tokens);
	Stack_clear(s);
}

void Tokenizer_free(Tokenizer* t) {
	assert(t && "Tokenizer has to be valid");
	Stack* s = &(t->tokens);
	Stack_free(s);
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


bool __is_wide_symbol(TokenTableEntry e) {
	const size_t sz = strlen(e.txt);
	return (sz > 1 && sz < WIDE_SYMBOL_SZ);
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
		case TokenKind_wide_symbol: 
			{
				const char* as_str = (void*)(&t.data.as_wide_symbol);
				strcat(cstr,"`");
				strncat(cstr,as_str,WIDE_SYMBOL_SZ); 
				strcat(cstr,"`");
			}
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
	
	snprintf(token,triple_capacity,"%c %s%s%s%s%s%s%s%s %c",
			(use_curly) ? '{' : 1,

			(show_text) ? 	TOKENIZER_TOKEN_FMT_CONTENT : "",	
			(show_text) ? 	cstr : "",

			(show_tab)	?	"\t" : "",
			(show_kind) ?	TOKENIZER_TOKEN_FMT_KIND : "",		
			(show_kind) ?	kind_str : "",
			
			(show_tab)	?	"\t" : "",
			(show_id)	?	TOKENIZER_TOKEN_FMT_ID : "",			
			(show_id)	?	id_str : "",
			
			(use_curly) ? '}' : 1
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

retry:;
	const char* leftover = (t->target + t->position);
	const symbol_t current_symbol = *leftover;

	if(t->position < t->target_length) {
		table_i = __tkn_match_table(*t);

		//printf("\nmatching: {%s}", leftover);

		// symbol/keyword exists in tokenization table
		if (table_i != INVALID_INDEX) {
			TokenTableEntry e = t->token_table[table_i];
			size_t			esz = strlen(e.txt);
			
			result.id = e.id;
			result.kind = __is_wide_symbol(e) 
				? TokenKind_wide_symbol : TokenKind_symbol;
			
			if (__is_wide_symbol(e)) {
				result.kind = TokenKind_wide_symbol;
				result.data.as_wide_symbol = stoi(e.txt);
			} else {
				result.kind = TokenKind_symbol;
				result.data.as_symbol = e.txt[0];
			}

			t->position += esz;
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
			result = (Token) {0};

		t->position += step_size;
		step_size = 0; // reset each time
	}

	// if we exausted a target string
	// return EOF
	else
		result.kind = TokenKind_EOF;
	
	return result;
}

void Tokenizer_run(Tokenizer* t) {
	Token token = {0};
	Stack* s = &(t->tokens);
	while( (token = Tokenizer_next_token(t)).kind != TokenKind_EOF ) {
		Stack_push(s,token);
	}
}

#endif
