#ifndef __DT_BUNDLE_H
#define __DT_BUNDLE_H

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
// - dtp_progragate for applying generated_instructions info and possible parse errors.
// - reduce boiler plate related to incrementing instructions emmited
// - sane reservation of space for instruction when compiling assembly.
// - make example interpreter emmit assembly of the code.
// - push string literalls and manage memory for them.

#include "compiler/dtc.h"
#include "vm/dtvm.h"

// MAIN

#define dti_opt(dti, opt) ((dti).options & (opt))


// Duct

typedef struct {
    DtParser        parser;
    Dtvm            vm;
    bool            have_set_trace_file;
    FILE            *tracef,
                    *errorf;
    FILE    *trace_vm_execution_f,
            *trace_vm_assembly_f,
            *trace_compiler_ast_f;

    int options;
    FILE *outputf;
} DtInterpreter;

void dt__prepare_parser(DtInterpreter* interpreter) {
    if(interpreter->have_set_trace_file) return;

    interpreter->parser.function_symbols = dtp_init_function_symbols();
    interpreter->parser.tracef = interpreter->trace_compiler_ast_f; 
}

void dt__prepare_vm(DtInterpreter* interpreter) {
    Dtvm* vm = &(interpreter->vm);

    if(interpreter->have_set_trace_file) return;

    interpreter->vm.writef     = interpreter->outputf; 
    interpreter->vm.tracef     = interpreter->trace_vm_execution_f; 
    interpreter->vm.asmtracef  = interpreter->trace_vm_assembly_f; 

    // vm assembly tracing
    if(dti_opt(*interpreter, DT_OPT_TRACE_VM_ASSEMBLY_COMPILATION)) {
        if(dti_opt(*interpreter, DT_OPT_EXPECT_INITILIZED_STREAM_FILES)) 
            assert(vm->asmtracef && "expected to have assembler trace file to be initilized");
        else { if(!vm->asmtracef) vm->asmtracef = stderr; }
    }

    // vm execution tracing
    if(dti_opt(*interpreter, DT_OPT_TRACE_VM_EXECUTION)) {
        if(dti_opt(*interpreter, DT_OPT_EXPECT_INITILIZED_STREAM_FILES)) 
            assert(vm->tracef && "expected to have vm execution trace file to be initilized");
        else { if(!vm->tracef) vm->tracef = stderr; }
    }
}

void dt_prepare(DtInterpreter* interpreter) {
    if(!interpreter->have_set_trace_file) {
        dt__prepare_parser(interpreter);
        dt__prepare_vm(interpreter);
    }
}

bool dt_compile_assembly_from_string(DtInterpreter* interpreter, const char* source, size_t length) {
    bool result = true;
    DtInterpreter* itrp = interpreter;
    DtParser* parser = &(interpreter->parser);
    
    parser->lexer           = DtTokenizer_init(source, length);
    parser->options_mirror  = itrp->options;
    
    // check for initilized trace file.
    if(dti_opt(*interpreter, DT_OPT_TRACE_COMPILER_AST_WALKING)) {
        if(dti_opt(*interpreter, DT_OPT_EXPECT_INITILIZED_STREAM_FILES)) 
            assert(parser->tracef && "expected to have AST trace file to be initilized");
        else { if(!parser->tracef) parser->tracef = stderr; }
    }

    dtp_parse(parser, 0);
    if (parser->tracef) fprintf(parser->tracef, "GATHERED ASSEMBLY: ----\n%.*s\n----------\n",
            sb_fmt(parser->output)
    );
    //DtNode* root = dtp_block(&parser, 0, false);
    // dtp_print_ast(root, 0, stdout);
    arena_free(&itrp->parser.allocator);
    return result;
}

// TODO: api nob style like `cmd_append()`

void dt_run(DtInterpreter* interpreter, const char* entry_point) {
    Dtvm* vm = &(interpreter->vm);
    dtvm_call(vm, string(entry_point), NULL, 0, 0);
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
    dt_run  (interpreter, entry_point);

    if (interpreter->tracef) fprintf(interpreter->tracef, "\tData stack pointer : %lu\n", interpreter->vm.data_sp);
    dt_reset(interpreter);
}

void dt_compile(DtInterpreter* interpreter, const char* source) {
    DtInterpreter*  itrp    = interpreter;
    Dtvm*           vm      = &(itrp->vm);
    dt_prepare(interpreter);
    dt_compile_assembly_from_string(interpreter, source, strlen(source));
    String assembly = {
        itrp->parser.output.items,
        itrp->parser.output.count
    };
    dtvm_compile_from_asm(vm, assembly);
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
    dt_prepare(inter);
    dtvm_compile_from_asm(&(inter->vm), str_make(assembly));
}

// IF C99
#if __STDC_VERSION__ >= 199901L
typedef enum {
    DT_EXEC_SOURCE_CODE,
    DT_EXEC_SOURCE_ASSEMBLY,
} DtExecuteSourceKind;

typedef struct {
    const char*         override_entrypoint;
    const char*         source;
    DtExecuteSourceKind source_kind;
    bool                do_not_reset;
    bool                do_not_run;
} DtExecuteOptions;

#define dt_execute(interpreter, ...) \
    dt_exectue_ex(interpreter, (DtExecuteOptions){__VA_ARGS__})
void dt_exectue_ex(DtInterpreter* interpreter, DtExecuteOptions opt);

void dt_exectue_ex(DtInterpreter* interpreter, DtExecuteOptions opt) {
    assert(0 && "TODO: NOT INPLEMENTED YET");
}
#endif


#if 0

int main(void) {
    DtInterpreter interp = {0};
    dt_execute(&iterp, 
            .override_entrypoint = "start",
            .source = str_multiline(
                start() {
                    print("Hello world!")
                }
            ),
    );
}

#endif

#endif//__DT_BUNDLE_H
