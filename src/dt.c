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
#include "compiler/dtc.h"
#include "vm/dtvm.h"

// MAIN
#if 1

typedef struct {
    DtParser        parser;
    Dtvm            vm;
    FILE            *tracef,
                    *errorf,
                    *outputf;
} DtInterpreter;

bool dt_compile_assembly_from_string(DtInterpreter* interpreter, const char* source, size_t length) {
    bool result = true;
    DtInterpreter* itrp = interpreter;
    itrp->parser.lexer = DtTokenizer_init(source, length);
    itrp->parser.tracef = interpreter->tracef; 
    itrp->parser.tracef = interpreter->tracef; 
    itrp->vm.writef     = interpreter->outputf; 

    // DtNode* root = dtp_parse(&c->parser, 0);
    dtp_parse(&(itrp->parser), 0);
    //DtNode* root = dtp_block(&parser, 0, false);
    // dtp_print_ast(root, 0, stdout);
    arena_free(&itrp->parser.allocator);
    return result;
}

// TODO: api nob style like `cmd_append()`
#define sb_fmt(sb) (int)sb.count, sb.items
#define str_multiline(...) #__VA_ARGS__

void dt_run(DtInterpreter* interpreter, const char* entry_point) {
    Dtvm* vm = &(interpreter->vm);
    if(!interpreter->vm.writef) {
        interpreter->vm.writef     = interpreter->outputf; 
    }
    dtvm_call(vm, string(entry_point), NULL, 0);
    dtvm_print_stack(vm);
}

void dt_reset(DtInterpreter* interpreter) {
    FILE *tracef = interpreter->tracef, 
         *outputf = interpreter->outputf,
         *errorf = interpreter->errorf;
    dtvm_free(&interpreter->vm);
    interpreter->tracef = tracef;
    interpreter->outputf = outputf;
    interpreter->errorf = errorf;
}

void dt_run_reset(DtInterpreter* interpreter, const char* entry_point) {
    dt_run(interpreter, entry_point);
    dt_reset(interpreter);
}

void dt_compile(DtInterpreter* interpreter, const char* source) {
    DtInterpreter*  itrp    = interpreter;
    Dtvm*           vm      = &(itrp->vm);
    dt_compile_assembly_from_string(interpreter, source, strlen(source));
    String assembly = {
        itrp->parser.output.items,
        itrp->parser.output.count
    };
    dtvm_compile_from_asm(vm, assembly, interpreter->tracef);
}

void dt_compile_run_reset(DtInterpreter* interpreter, const char* source) {
    dt_compile(interpreter, source);

    // for(size_t i = 0; i<vm->functions.count; i++) {
    //     printf("vm.functions[%lu] = '%.*s'\n", i, str_fmt(vm->functions.items[i].id));
    //     Function fn = vm->functions.items[i];
    //     for(size_t in = 0; in < fn.code.count; in++) {
    //         printf("\t> %lu kind: %i\n", in, fn.code.items[in].kind);
    //     }
    // }
    // dtvm_add_function(&vm, fn1);
    // dtvm_add_function(&vm, fn2);
    dt_run_reset(interpreter, "main");
}

void dt_append_assembly(DtInterpreter* inter,  const char* assembly) {
    dtvm_compile_from_asm(&(inter->vm), str_make(assembly), inter->tracef);
}

// TODO:
// function signatures are checked and added at compilation time
// arrays are linear and one type only
// lists are type ignorant, can be of any type
// objects are like lists, but each member has NAME (identifier).
// implement loops
// implement if statements
// implement arrays and indexing.
// implement lists.
// 
// LATER:
// - constant folding

// TODO: 
// - dtp_progragate for applying generated_instructions info and possible parse errors.
// - rename dtp_ok_or_return to dtp_ok_or_abort
// - proper Token_to_string() function that isn't crappy
// - reduce boiler plate related to incrementing instructions emmited
// - sane reservation of space for instruction when compiling assembly.
// - make example interpreter emmit assembly of the code.

int main(void) {
    // const char* file = "./examples/parsing_00.dt";
    // char* txt = dt_load_file(file);
    // if (!txt) {
    //     printf("%s\n", strerror(errno));
    //     return 1;
    // }

    DtInterpreter interp = {
        .tracef  = stderr,
        .outputf = stdout,
    };

    // const char* assembly_if_source = 
    //         "fn main\n"
    //         // $ - nameless variable
    //         "push 1     \n" 
    //        
    //         // if $ == 10
    //         "cmp 1      \n"
    //         "ijif 5     \n"
    //         "pop        \n"
    //         "pop        \n"
    //         "push 10   \n"
    //         "wrt        \n"
    //         "jmp end     \n" // end
    //        
    //         // else if $ == 1
    //         "pop        \n"
    //         "cmp 2      \n"
    //         "ijif 5     \n"
    //         "pop        \n"
    //         "pop        \n"
    //         "push 20  \n"
    //         "wrt        \n"
    //         "jmp end    \n" // end
    //
    //         // else if $ == 5
    //         "pop        \n"
    //         "cmp 3      \n"
    //         "ijif 5     \n"
    //         "pop        \n"
    //         "pop        \n"
    //         "push 30    \n"
    //         "wrt        \n"
    //         "jmp end     \n" // end
    //
    //         // clean up if none of the branches match
    //         "pop\n"
    //         "pop\n"
    //         "nop\n"
    //
    //         "push 69\n"
    //         "wrt\n"
    //         // other code
    //         "nop\n"
    //         "@end\n"
    //         "endfn\n";
    // dt_append_assembly(&interp,  assembly_if_source);
    // /*
    // add(a,b) { return a + b }
        const char* source = str_multiline(
                main() {
                    a = 1
                    b = 21
                    if b < a { 
                        write(a+b)
                    } else {
                        write(69)
                    }
                }
        );
    // */

    dt_append_assembly(&interp, 
            "fn write v\n"
                "load v\n"
                "wrt\n"
                "pop\n"
            "endfn\n"
    );
    // dt_compile(&interp, source);
    dt_compile_run_reset(&interp, source);
    // dt_run_reset(&interp, "main");
    // dt_reset(&interp);

    // TODO: check for empty file
    // free(txt);
}



#endif
