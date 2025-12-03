// TODO:
// 
// NEXT:
//
// - swap temporary `static char temp` into shared temporary buffer
// - create type table instead of using just a build-in type ids
// - string validations and data storage
// - arrays syntax and validation
// - object syntax and validation
//
// - add build-in: print, cast, binop, typeof, sizeof - operator-functions
// 
//
// - ? continues error checking even after one error was found (skip the branch)
//      ? idk if i want to do this for this small language since it would overcompliate the parsing process.

// ITERATE ON/APPEND:
// ~ write ultimate resolver function for types, call it on binop
// - ~~write system for analisys of return types of each identifer~~
//      + now dti_eval_* functions do both analysis and interpreting and the role is decided but DtInterpreter.eval_mode boolean

// REFACTOR/ADD:
// - CLEAN UP THE API FOR END USED
//      - merge together tokenizer + parser + interpreter
//      - they all produce a tree that can be run or compiled
//      - "intepreted" program generates a hash that indicates correctness of program
//      - on change of source code when dti_interpret() is called, rehash program and rebuild if it has changed, 
//              otherwish run the program from existing tree.
//      - when interpreting code, minimal checks should be performed
// ~ if with elif extender
// - for loop and 'loop' loop
// - automated testing of parser and evaluator (interpreter)
// - add objects and arrays
// - cleanup DtValue functions
// ~ system to infer types of expressions and indetifiers
// - add cast(v, T) 

// - add interpreter analysis errors
// - add more stuff do stack frame so it polutes less the function arguments for 
//      all `dti_` functions
// - use hashmap instead of linear lookup of variables

// COMPLICATED
// - make all code compile to simple register based vm assembly
// - compiler and interpteter are two parts of one system
// - compile to C and have dtdl_load() function to load such code.
// - simple language runtime for dtdl functionalty

#include <stdio.h>
#define STACK_STARTING_CAPACITY 1024
#include "eval.c"

#if 0
int main(void) {
    enum {
        MARKER_NONE,
        MARKER_SOURCE_LOC,
        MARKER_ERROR,
        MARKER_LINE,
        MARKER_ARROW
    };

    StringBuilder sb = {0};
    sb_append(&sb, "%i:%i", 10, 10);
    sb_append(&sb, "\t");
    sb_append(&sb, "Error: failed to append\n");
    sb_append(&sb, "%*c",10,'>');
    sb_append(&sb, "\tline of error");
    
    const char* string = sb_collect(&sb);
    printf("%s",string);
    
    sb_clear(&sb);
}
#endif

#if 0
int main(void) {
    char* txt = dt_load_file("./examples/parsing_02.dt");

    if (!txt) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    Tokenizer t = DtTokenizer_init(txt);
    Token tok = {0};

    while((tok = Tokenizer_next_token(&t)).kind != TokenKind_EOF) {
        printf("%s\n", Token_temp_cstr(tok));
    }

    Tokenizer_free(&t);
    free(txt);
}
#endif

#if 0
int main(void) {
    Arena a = {0};
    int size = 512;
    char** ptr = arena_alloc(&a, sizeof(char*)*size);
    
    for(int i = 0; i < size; i++) {
         ptr[i] = arena_alloc(&a, 64);
         sprintf(ptr[i], "string #%i", i);
    }
    for(int i = 0; i < size; i++) {
         printf("#%i: '%s'\n", i, ptr[i]);
    }
    arena_clear(&a);
    for(int i = 0; i < size; i++) {
         printf("string #%i should be null: '%s'\n", i, ptr[i]);
    }
    arena_reset(&a);
}
#endif

// TODO: 
// // locking hashes source and ast
// // if either changes, recompile or re-eval
// dtrt_lock();
// dtrt_relock();
//
// // eval - does the type,function,symbol checking, runs code in shallow way
// dtrt_eval();
// // run - runs code fully with all the proper logic
// dtrt_run();
//
// // does both previous things
// dtrt_eval_and_run();
//
// // unline run, compiles the code to one of the targets, provided by user
// dtrt_compile();

// TODO: implement correct coredump in DtRuntime



// TODO: implement:
//  dtr_compile(DtRuntime*)
//  dtr_interpret(DtRuntime*)

// MAIN
#if 1
int main(void) {
    int exit_code = 0;
    const char* file = "./examples/parsing_02.dt";
    char* txt = dt_load_file(file);

    if (!txt) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    DtStatus status = {
        .file_name = file,
        .source = txt,
    };

    DtParser parser = {
        .lexer = DtTokenizer_init(txt, strlen(txt)),
        .stat = &status
    };

    
    DtNode* root = dtp_parse(&parser, 0);
    //DtNode* root = dtp_block(&parser, 0, false);
    if (status.abort) {
        for(size_t i = 0; i < status.errors.count; i++) {
            const char* error = status.errors.items[i];
            printf("%s",error);
        }
        exit_code = 1;
    }
    
    printf("\n");
    dtp_print_ast(root, 0, stdout);
    //__asm__("int3");

    DtContext ctx = {
        .functions = dto_scope_init(),
    };
    /*
     if (root) {
        dte_eval_prepass(&ctx, root);
        dte_eval_root(&ctx, root);
    }
    */
    for(size_t i = 0; i < status.errors.count; i++) {
        free((void*)status.errors.items[i]);
    }
    
    dto_scope_clear(&ctx.functions);
    free((void*)status.errors.items);
    sb_clear(&status.error_builder);
    arena_reset(&parser.nodes);
    free(txt);
    // TODO: check for empty file
    return exit_code;
}



#endif
