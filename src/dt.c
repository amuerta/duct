#define PARSER_TRACE  
#include <stdio.h>
#define STACK_STARTING_CAPACITY 1024
#include "compiler/dtc.h"
#include "vm/dtvm.h"

// MAIN

#define dti_opt(dti, opt) ((dti).options & (opt))
enum {
    DT_OPT_TRACE_INTO_SINGLE_FILE = 1 << 0,
    DT_OPT_TRACE_VM_EXECUTION = 1 << 1,
    DT_OPT_TRACE_VM_ASSEMBLY_COMPILATION = 1 << 2,
    DT_OPT_TRACE_COMPILER_AST_WALKING = 1 << 3,
    
    DT_OPT_EXPECT_INITILIZED_STREAM_FILES = 1 << 10,
};


typedef struct {
    DtParser        parser;
    Dtvm            vm;
    FILE            *tracef,
                    *errorf;
    FILE    *trace_vm_execution_f,
            *trace_vm_assembly_f,
            *trace_compiler_ast_f;

    int options;
    FILE *outputf;
} DtInterpreter;

void dt__prepare_parser(DtInterpreter* itrp) {
    itrp->parser.function_symbols = dtp_init_function_symbols();
    itrp->parser.tracef = itrp->tracef; 
}

void dt__prepare_vm(DtInterpreter* itrp) {
    itrp->vm.writef     = itrp->outputf; 
    itrp->vm.tracef     = itrp->tracef; 
}

bool dt_compile_assembly_from_string(DtInterpreter* interpreter, const char* source, size_t length) {
    bool result = true;
    DtInterpreter* itrp = interpreter;
    itrp->parser.lexer  = DtTokenizer_init(source, length);

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
        if(dti_opt(*interpreter, DT_OPT_EXPECT_INITILIZED_STREAM_FILES)) 
            assert(0 && "expected output to be manually initilized");
        interpreter->vm.writef     = interpreter->outputf; 
    }
    dtvm_call(vm, string(entry_point), NULL, 0);
    dtvm_print_stack(vm);
}


void dt_reset(DtInterpreter* interpreter) {
    FILE *tracef = interpreter->tracef, 
         *outputf = interpreter->outputf,
         *errorf = interpreter->errorf;
    dtvm_free                   (&interpreter->vm);
    dtp_free_function_symbols   (&(interpreter->parser.function_symbols));
    free(interpreter->parser.output.items);
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
    dt__prepare_parser(interpreter);
    dt__prepare_vm(interpreter);
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
// implement arrays and indexing.
// implement lists.
// 
// LATER:
// - constant folding

// TODO: 
// - ALL DECLARATIONS FOR VM AND COMPILER ARE IN THEIR CORRESPONDING DECLARATION FILE. 
//      SHARED DECLARATIONS LIK ARE LOCATED IN SHARED FOLDER.
//      INTERPERETER LOCATED IN ITS SEPARATE FOLDER
// - dtp_progragate for applying generated_instructions info and possible parse errors.
// - rename dtp_ok_or_return to dtp_ok_or_abort
// - reduce boiler plate related to incrementing instructions emmited
// - sane reservation of space for instruction when compiling assembly.
// - make example interpreter emmit assembly of the code.
// - push string literalls and manage memory for them.

int main(void) {
    // const char* file = "./examples/parsing_00.dt";
    // char* txt = dt_load_file(file);
    // if (!txt) {
    //     printf("%s\n", strerror(errno));
    //     return 1;
    // }

    DtInterpreter interp = {

        // .tracef  = stderr,
        .outputf = stdout,
    };

    // const char* source = str_multiline(
    //         main() {
    //             a = 1
    //             b = 21
    //             if b < a { 
    //                 write(a+b)
    //             } else {
    //                 write(69)
    //             }
    //         }
    // );

    const char* source = str_multiline(
        add(l,r) {
            return l + r
        }
        main() {
            write(add(1,2))
        }
    );

    // const char* source = str_multiline(
    //         rec(i,n) {
    //             if i < n { return rec(i+1,n) } else { return i }
    //         }
    //         main() { write(rec(0,5)) }
    // );

    // dt_append_assembly(&interp, 
    //         "fn add l r\n"
    //             "load l\n"
    //             "load r\n"
    //             "add\n"
    //             "ret\n"
    //         "endfn\n"
    //
    //         "fn main\n"
    //             "push 11\n"
    //             "push 22\n"
    //             "call add\n"
    //             "call write\n"
    //         "endfn\n"
    // );

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

    // printf("\n");
    // TODO: check for empty file
    // free(txt);
}

