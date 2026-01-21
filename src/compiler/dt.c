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

#define PARSER_TRACE  
#include <stdio.h>
#define STACK_STARTING_CAPACITY 1024
#include "dtc.h"
#include "../vm/dtvm.h"

// MAIN
#if 1


typedef struct {
    DtParser        parser;
} DtCompiler;

bool dt_compile_assembly_from_string(void* compiler, const char* source, size_t length) {
    bool result = true;
    DtCompiler* c = compiler;
    c->parser.lexer = DtTokenizer_init(source, length);

    // DtNode* root = dtp_parse(&c->parser, 0);
    dtp_parse(&c->parser, 0);
    //DtNode* root = dtp_block(&parser, 0, false);
    
    printf("\n");
    // dtp_print_ast(root, 0, stdout);
    arena_free(&c->parser.allocator);
    return result;
}


#define sb_fmt(sb) (int)sb.count, sb.items
#define str_multiline(...) #__VA_ARGS__

void dt_compile_and_run(Hlvm* vm, DtCompiler* dtc, const char* source) {

    dt_compile_assembly_from_string(dtc, source, strlen(source));
    // printf("\n'\n%.*s'\n\n", sb_fmt(dtc->parser.output));

    String assembly = {
        dtc->parser.output.items,
        dtc->parser.output.count
    };

    dtvm_compile_from_asm(vm, assembly);

    // for(size_t i = 0; i<vm->functions.count; i++) {
    //     printf("vm.functions[%lu] = '%.*s'\n", i, str_fmt(vm->functions.items[i].id));
    //     Function fn = vm->functions.items[i];
    //     for(size_t in = 0; in < fn.code.count; in++) {
    //         printf("\t> %lu kind: %i\n", in, fn.code.items[in].kind);
    //     }
    // }

    // printf("\n\nCALLING 'main': \n");

    // dtvm_add_function(&vm, fn1);
    // dtvm_add_function(&vm, fn2);
    dtvm_call(vm, string("main"), NULL, 0);

    printf("\n\nSTACK: \n");
    dtvm_print_stack(vm);
    printf("-----: \n");
    // dtvm_function_free(&fn1);
    // dtvm_function_free(&fn2);
}

int main(void) {
    // const char* file = "./examples/parsing_00.dt";
    // char* txt = dt_load_file(file);
    // if (!txt) {
    //     printf("%s\n", strerror(errno));
    //     return 1;
    // }

    const char* source = str_multiline(
        main() {
            return 10 + 21 * (2 - 1)
        }
    );

    DtCompiler compiler = {0};
    Hlvm vm = {
        .writef = stdout,
        .tracef = stdout,
    };
    dt_compile_and_run(&vm, &compiler, source);


    
    dtvm_free(&vm);
    // TODO: check for empty file
    // free(txt);
}



#endif
