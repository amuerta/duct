#include "declarations.h"

#ifndef __DTVM_ASSEMBLER_H
#define __DTVM_ASSEMBLER_H


void dtvm_instructions_free(Instructions* code) {
    if(code->items) free(code->items);
    memset(code, 0, sizeof(*code));
}

const char* dtvm_get_error(int status) {
    static char temp[DTVM_TEMPORARY_STRING_BUILDER_CAP];
    memset(temp, 0, DTVM_TEMPORARY_STRING_BUILDER_CAP);
    StringBuilder sb = {
        .items    = temp,
        .capacity = DTVM_TEMPORARY_STRING_BUILDER_CAP,
    };

    switch(status) {
        case DTSTAT_INVALID_OPERAND_TYPE:
            sb_appendf(&sb, "Invalid operand type"); break;

        case DTSTAT_NO_OPERAND:
            sb_appendf(&sb, "No operand"); break;
        
        default:
            sb_appendf(&sb, "No error!");
    }

    return (const char*) sb.items;
}

const char* dtvm_decompile_instruction(Instruction i) {
    static char out[DTVM_TEMPORARY_STRING_BUILDER_CAP];
    memset(out, 0, DTVM_TEMPORARY_STRING_BUILDER_CAP);
    StringBuilder sb = {
        .items = out,
        .capacity = DTVM_TEMPORARY_STRING_BUILDER_CAP
    };

    switch (i.kind) {

             /*INSTRUCTIONS WINO ARGUMENTS*/
        case DTI_NOP:
             sb_appendf(&sb, "nop"); break;
        case DTI_DUP:
             sb_appendf(&sb, "dup"); break;
        case DTI_POP:
             sb_appendf(&sb, "pop"); break;
        case DTI_ROT:
             sb_appendf(&sb, "rot"); break;

        case DTI_ADD:
             sb_appendf(&sb, "add"); break;
        case DTI_SUB:
             sb_appendf(&sb, "sub"); break;
        case DTI_DIV:
             sb_appendf(&sb, "div"); break;
        case DTI_MUL:
             sb_appendf(&sb, "mul"); break;
        case DTI_NOT:
             sb_appendf(&sb, "not"); break;

        case DTI_RET:
             sb_appendf(&sb, "ret"); break;
        case DTI_WRT:
             sb_appendf(&sb, "wrt"); break;

             /*INSTRUCTIONS WITH ARGUMENTS*/
        case DTI_PUSH:
             sb_appendf(&sb, "push"); 
             switch(i.as.push.object.type) {
                 case OT_INT:   sb_appendf(&sb, " i.%i",    i.as.push.object.as.i); break;
                 case OT_BYTE:  sb_appendf(&sb, " b.%i",    i.as.push.object.as.i); break;
                 case OT_FLOAT: sb_appendf(&sb, " f.%f",    i.as.push.object.as.f); break;
                 case OT_BOOL:  sb_appendf(&sb, " B.%s",    i.as.push.object.as.B ? "true" : "false"); break;
                 case OT_STRING:sb_appendf(&sb, " s(%.*s)",  str_fmt(i.as.push.object.as.s)); break;
             }
             break;

        case DTI_LOAD:
             sb_appendf(&sb, "load %.*s", str_fmt(i.as.load.ident));
             break;

        case DTI_STORE:
             sb_appendf(&sb, "store %.*s", str_fmt(i.as.store.ident));
             break;

        case DTI_CALL:
             sb_appendf(&sb, "call %.*s", str_fmt(i.as.call.ident));
             break;

        case DTI_IJIF:
             sb_appendf(&sb, "ijif %i", i.as.ijif.jumpto);
             break;

        case DTI_JMP:
             sb_appendf(&sb, "jmp %i", i.as.jmp.jumpto);
             break;

             // operate on the stack values like add/sub/mul/div
        case DTI_EQ: 
             sb_appendf(&sb, "eq "); 
             break;
        case DTI_LT: 
             sb_appendf(&sb, "lt "); 
             break;
        case DTI_GT: 
             sb_appendf(&sb, "gt "); 
             break;
        case DTI_GTE: 
             sb_appendf(&sb, "gt "); 
             break;
        case DTI_LTE: 
             sb_appendf(&sb, "gt "); 
             break;

             // work on literally values, have `l` perfix
        case DTI_LEQ: 
             sb_appendf(&sb, "leq ");
             goto cmp_body;
        case DTI_LLT:
             sb_appendf(&sb, "llt ");
             goto cmp_body;
        case DTI_LGT:
             sb_appendf(&sb, "lgt ");
             goto cmp_body;
        case DTI_LGTE:
             sb_appendf(&sb, "lgte ");
             goto cmp_body;
        case DTI_LLTE:
             {
                 sb_appendf(&sb, "llte ");
        cmp_body:   ;
                 switch(i.as.leq.object.type) {
                     case OT_BOOL:
                         sb_appendf(&sb, "%s", 
                                 i.as.leq.object.as.B ? "true" : "false"
                                 ); break;
                     case OT_CHAR:
                         sb_appendf(&sb, "c.%c", i.as.leq.object.as.b); 
                         break;

                     case OT_BYTE:
                         sb_appendf(&sb, "b.%i", i.as.leq.object.as.i); 
                         break;

                     case OT_INT:
                         sb_appendf(&sb, "i.%i", i.as.leq.object.as.i); 
                         break;

                     case OT_FLOAT:
                         sb_appendf(&sb, "f.%f", i.as.leq.object.as.f); 
                         break;

                     default:
                         UNREACHABLE("unsuported eq value for displaying");
                         break;
                 }
             }
             break;
    }

    return (const char*) sb.items;
}

// TODO: all symbol slices and other string data is stored
// in special linear memory (string builder)
LineResult dtvm_compile_instruction(Arena* allocator, String line, int* status) {
    #define split_next(L) str_split_by_chars(L, " ")
    #define match(STR, CSTR) str_cmp_cstr(STR, CSTR)
    
    String l = line;
    String 
        type = {0}, operand = {0},
        inst_str = split_next(&l),
        leftover = {0};

    LineResult  result  = {0};
    Instruction inst    = {0};

    if(match(inst_str, "endfn")) {
        *status = DTSTAT_END_FUNCTION;
        goto end;
    }

    else if(match(inst_str, "fn")) {
        *status = DTSTAT_START_FUNCTION;
        goto end;
    }

    // comment, set status to ignore this entire line
    else if(*inst_str.ptr == '#') {
        *status = DTSTAT_IGNORE;
    }

    // label, ignore line too, but emmit label.
    else if(*inst_str.ptr == '@') {
        result.kind = ELINE_LABEL;
        String label = string_alloc_to_arena(allocator, inst_str);
        if(str_is_empty(label)) {
            *status = DTSTAT_NO_OPERAND;
            goto end;
        }
        result.as.label.name = label;
    }

    else if(match(inst_str, "push")) {
        inst.kind = DTI_PUSH;
        leftover = l;
        operand  = split_next(&l);
        int     n   = 0;
        float   nf  = 0;
        
        if (str_is_empty(operand)) {
            *status = DTSTAT_NO_OPERAND;
            goto end;
        }

        if (str_is_integer(operand)) {
            n = string_to_integer(operand);
            inst.as.push.object = object_int(n);
        } 
        else if (str_is_float(operand)) {
            nf = string_to_float(operand);
            inst.as.push.object = object_float(nf);
        } else { // must be string literall

            // fprintf(stderr, "\t\tBEFORE: '%.*s':%li\n", (int)operand.len, operand.ptr, operand.len);
            // fprintf(stderr, "\t\tLEFT: '%.*s':%li\n", (int)leftover.len, leftover.ptr, leftover.len);
            // I should really remake this assembler with tokenize.h...
            // NOTE: USE LEFTOVER NOT OPERAND!!
            size_t n = tkn_parse_string(leftover.ptr, leftover.len, NULL, NULL, '\"');
            if(!n || n == -1) {
                *status = DTSTAT_INVALID_OPERAND_TYPE;
            } else {
                char* str     = arena_alloc(allocator, n);
                size_t written_size = 0;

                tkn_parse_string(leftover.ptr, leftover.len, str, &written_size, '\"');
                String parsed_string = {
                    .ptr = str, .len = written_size
                };
                // fprintf(stderr, "\t\tAFTER: '%.*s':%li\n", (int)parsed_string.len, parsed_string.ptr, parsed_string.len);
                // fprintf(stderr, "---------------------\n");
                inst.as.push.object = object_string(parsed_string);
            }

            //else 
        }


    } 

    else if (
            match(inst_str, "load")     ||
            match(inst_str, "store")    ||
            match(inst_str, "call")     
            ) {

        if (match(inst_str, "load"))    inst.kind = DTI_LOAD;
        if (match(inst_str, "store"))   inst.kind = DTI_STORE;
        if (match(inst_str, "call"))    inst.kind = DTI_CALL;
        operand = split_next(&l);
        if(str_is_empty(operand)) 
            *status = DTSTAT_NO_OPERAND;
        inst.as.store.ident = operand;
    }


    // TODO: make this less ugly?
    else if (
            match(inst_str, "leq")   || match(inst_str, "lcmp")   ||
            match(inst_str, "llt")   ||
            match(inst_str, "lgt")   ||
            match(inst_str, "llte")  ||
            match(inst_str, "lgte") 
            ) {

        if(match(inst_str, "leq") || match(inst_str, "lcmp"))   
            inst.kind = DTI_LEQ;
        if(match(inst_str, "llt"))   inst.kind = DTI_LLT;
        if(match(inst_str, "lgt"))   inst.kind = DTI_LGT;
        if(match(inst_str, "llte"))  inst.kind = DTI_LLTE;
        if(match(inst_str, "lgte"))  inst.kind = DTI_LGTE;
        operand = split_next(&l);
        int n = 0;

        if (str_is_integer(operand)) {
            n = string_to_integer(operand);
            if     (match(inst_str, "leq") || match(inst_str, "lcmp"))   
                                              inst.as.leq .object = object_int(n);
            else if(match(inst_str, "llt"))   inst.as.llt .object = object_int(n);
            else if(match(inst_str, "lgt"))   inst.as.lgt .object = object_int(n);
            else if(match(inst_str, "llte"))  inst.as.llte.object = object_int(n);
            else if(match(inst_str, "lgte"))  inst.as.lgte.object = object_int(n);
            else *status = DTSTAT_INVALID_OPERAND_TYPE;
        }
    }

    else if (match(inst_str, "ijif")) {
        inst.kind = DTI_IJIF;
        operand = split_next(&l);
        int n = 0;

        if (str_is_integer(operand)) {
            n = string_to_integer(operand);
            inst.as.ijif.jumpto = n;
        } else if (string_is_identifier(operand)) {
            inst.as.ijif.jumpto = DT_SIZE_T_INVALID; 
            inst.as.ijif.label  = string_alloc_to_arena(allocator, operand);
        }

        else *status = DTSTAT_INVALID_OPERAND_TYPE;
    }

    else if (match(inst_str, "jmp")) {
        inst.kind = DTI_JMP;
        operand = split_next(&l);
        int n = 0;

        if (str_is_integer(operand)) {
            n = string_to_integer(operand);
            inst.as.jmp.jumpto = n;
        } else if (string_is_identifier(operand)) {
            inst.as.ijif.jumpto = DT_SIZE_T_INVALID; 
            inst.as.ijif.label  = string_alloc_to_arena(allocator, operand);
        }
        else *status = DTSTAT_INVALID_OPERAND_TYPE;
    }

    else if (match(inst_str, "nop"))     { inst.kind = DTI_NOP;     }
    else if (match(inst_str, "ret"))     { inst.kind = DTI_RET;     }
    else if (match(inst_str, "wrt"))     { inst.kind = DTI_WRT;     }
    else if (match(inst_str, "dup"))     { inst.kind = DTI_DUP;     }
    else if (match(inst_str, "pop"))     { inst.kind = DTI_POP;     }
    else if (match(inst_str, "rot"))     { inst.kind = DTI_ROT;     }
    else if (match(inst_str, "add"))     { inst.kind = DTI_ADD;     }
    else if (match(inst_str, "sub"))     { inst.kind = DTI_SUB;     }
    else if (match(inst_str, "mul"))     { inst.kind = DTI_MUL;     }
    else if (match(inst_str, "div"))     { inst.kind = DTI_DIV;     }
    else if (match(inst_str, "and"))     { inst.kind = DTI_AND;     }
    else if (match(inst_str, "or" ))     { inst.kind = DTI_OR;      }
    else if (match(inst_str, "not"))     { inst.kind = DTI_NOT;     }


    else if (match(inst_str, "eq" ))     { inst.kind = DTI_EQ;      }
    else if (match(inst_str, "lt" ))     { inst.kind = DTI_LT;      }
    else if (match(inst_str, "gt" ))     { inst.kind = DTI_GT;      }
    else if (match(inst_str, "gte"))     { inst.kind = DTI_GTE;     }
    else if (match(inst_str, "lte"))     { inst.kind = DTI_LTE;     }

    // If not label or function set value to instruction.
    if(!(inst.kind == DTI_NULL && (*status) == DTSTAT_OK))
        result.as.instruction = inst;
end:
    return result;
}



String dtvm_bytecode_compile(
        FILE* trace_file,
        Arena* allocator, 
        Instructions* code, 
        String source, 
        String* out_name,
        Strings* out_args) 

{
    String line = {0}, all = source;
    String leftover = source;
    Labels labels = {0};

    int stat;
    
    // compile instructions
    for(int i = 0, ip = 0; !str_is_empty(all); i++) {
        stat = 0; // update stat per intruction
        line = str_split_by_chars(&all, "\n\r");
        line = str_trim(line);
        if(str_is_empty(line)) continue;
        LineResult result = dtvm_compile_instruction(allocator, line, &stat);
        
        if (stat == DTSTAT_START_FUNCTION) {
            leftover = all;
            // skip fn
            assert(match(str_split_by_chars(&line, " "), "fn"));

            // get name 
            String fn_name = str_split_by_chars(&line, " ");
            // get args names
            size_t args     =  0 ;
            String fn_arg   = {0};
            dta_trace(trace_file, "# Emmiting bytecode for '%.*s': \n", str_fmt(fn_name));
            
            do {
                fn_arg = str_split_by_chars(&line, " ");
                if (!str_is_empty(fn_arg)) {
                    assert(string_is_identifier(fn_name) && "expected to have identifier-like argument name");
                    da_append(out_args, fn_arg);
                    args++;
                }
            } while(!str_is_empty(fn_arg));
            *out_name = fn_name;
            
            assert(!str_is_empty(fn_name) && "expected to have `fn <name> <argc>`");
        }

        else if (stat == DTSTAT_END_FUNCTION) {
            leftover = all;
            break;
        }

        else if (result.kind == ELINE_LABEL) {
            if (stat <= DTSTAT_NOT_ERROR) {
                if(!ip) {
                    fprintf(stderr, 
                            "Failed at line when parsing label: #%i:'%.*s', error: %s\n", 
                            i, 
                            str_fmt(line), 
                            "Labels with 0th index are not allowed."
                    );
                    assert(0);
                    break;
                }
                else if(str_is_empty(result.as.label.name)) {
                    fprintf(stderr, 
                            "Failed at line when parsing label: #%i:'%.*s', error: %s\n", 
                            i, 
                            str_fmt(line), 
                            "Label cannot have empty name."
                    );

                    assert(0);
                    break;
                }

                result.as.label.address = ip - 1;
                result.as.label.name.len--;
                result.as.label.name.ptr++;

                dta_trace(trace_file, "\t> found label: '%.*s : %i'\n", str_fmt(result.as.label.name), 
                        ip - 1);
                da_append(&labels, result.as.label);
            } else {
                dta_trace(trace_file, 
                        "Failed at line when parsing label: #%i:'%.*s', error: %s\n", 
                        i, str_fmt(line), dtvm_get_error(stat));
                assert(0);
                break;
            }
        }

        else if (result.kind == ELINE_INSTRUCTION) {
            Instruction inst = result.as.instruction;

            if (stat == DTSTAT_IGNORE) 
                continue;
            else if(stat <= DTSTAT_NOT_ERROR) {
                da_append(code, inst);
                ip++;
            } else {
                fprintf(stderr, 
                        "Failed at line: #%i:'%.*s', error: %s\n", 
                        i, str_fmt(line), dtvm_get_error(stat));

                assert(0);
                break;
            }
        }
    }


    const String STRING_EMPTY = {0}; 
    for(size_t i = 0; i < code->count; i++) {
        Instruction* ins = code->items + i;
        if(ins->kind == DTI_IJIF || ins->kind == DTI_JMP)
        if(ins->as.ijif.jumpto == DT_SIZE_T_INVALID) {
            // TODO: 
            //  - use map
            Label instruction_label = {
                .name = ins->as.ijif.label
            };
            Label label             = labels.items[0];
            for(size_t j = 0; j < labels.count; j++, (label = labels.items[j]))  
                if(string_cmp(instruction_label.name, label.name)) {
                    ins->as.ijif.jumpto = label.address;
                    ins->as.ijif.label  = STRING_EMPTY;
                    // printf("matched label: %.*s, %.*s\n", 
                    //         str_fmt(instruction_label.name), 
                    //         str_fmt(label.name));
                }
        }
    }

                
    for(size_t i = 0; i < code->count; i++) {
        Instruction inst = code->items[i];
        dta_trace(trace_file,"%.6lu\t%s\n", i, dtvm_decompile_instruction(inst));
    }

    da_free(labels);
    return leftover;
}

Function dtvm_function_pack(String name,  Instructions* code, String* argv, size_t argc) {
    assert(code);
    assert(!str_is_empty(name));
    // count how many string symbols there are in bytecode
    size_t symbols_count = 0;
    size_t instruction_symbol_count = 0;
    for(size_t i = 0; i < code->count; i++) {
        Instruction ins = code->items[i];
        switch (ins.kind) {
            case DTI_LOAD: 
            case DTI_STORE:
            case DTI_CALL:
                symbols_count += ins.as.store.ident.len;
                break;
            case DTI_PUSH:
                {
                    Object it = ins.as.push.object;
                    if(it.type == OT_STRING) {
                        // fprintf(stderr, "\t\tFOR STRING '%.*s' length is %lu\n",(int) it.as.s.len, it.as.s.ptr, it.as.s.len);
                        symbols_count += it.as.s.len;
                    }
                } break;
        }
    }
    instruction_symbol_count = symbols_count;
    // count how much is the names of the arguments
    for(size_t i = 0; i < argc; i++) {
        // printf("argv[%i], '%.*s'\n", i, str_fmt(argv[i]));
        symbols_count += argv[i].len;
    }

    size_t code_in_bytes = code->count * sizeof(code->items[0]);
    size_t args_in_bytes = argc * sizeof(argv[0]);
    size_t allocation_size = 
        symbols_count + 
        args_in_bytes + 
        code_in_bytes ;

    // printf("non arg symbol_count:           %lu\n", instruction_symbol_count);
    // printf("symbol count in data:           %lu\n", symbols_count);
    // printf("args in bytes:                  %lu\n", args_in_bytes);
    // printf("code count in bytes:            %lu\n", code_in_bytes);
    // printf("total memory size:              %lu\n", allocation_size);
    // printf("code_section                    %lu\n", symbols_count + args_in_bytes);
    

    void* memory = malloc(allocation_size);
    void* symbols_section       = memory;
    void* arg_symbols_section   = memory + instruction_symbol_count;
    void* args_section          = memory + symbols_count;
    void* code_section          = memory + symbols_count + args_in_bytes;

    // pack objects as argument information
    String* args_packed = 0; 
    if(argc > 0) {
        memcpy(args_section, argv, args_in_bytes);
        args_packed = args_section;
    }

    // pack symbols, and update pointers for String inside instructions
    for(size_t i = 0, offset = 0; i < code->count; i++) {
        Instruction ins     = code->items[i];
        Instruction *insp   = code->items + i;
        switch (ins.kind) {
            case DTI_LOAD: 
            case DTI_STORE: 
            case DTI_CALL: 
                {
                    // move memory
                    String ident = ins.as.load.ident;
                    void* dest = (symbols_section + offset);
                    memcpy(dest, ident.ptr, ident.len);
                    // update offets
                    offset += ident.len;
                    // push new ident in place of the old one
                    String new_ident = {dest, ident.len};
                    insp->as.load.ident = new_ident;
                    break;
                }

            case DTI_PUSH:
                {   
                    Object it = ins.as.push.object;
                    if(it.type == OT_STRING) {
                        String str = it.as.s;
                        void* dest = (symbols_section + offset);
                        memcpy(dest, str.ptr, str.len);
                        // update offset
                        offset += str.len;
                        // push new string in place of the old one
                        String new_str              = {dest, str.len};
                        insp->as.push.object.as.s   = new_str;
                    }
                } break;
        }
    }

    // do the same for function arguments identifiers
    if(argc > 0) 
        for(size_t i = 0, offset = 0; i < argc; i++) {
            String* id = args_packed + i;
            void* dest = (arg_symbols_section + offset);
            memcpy(dest, id->ptr,id->len);
            id->ptr = dest;
            offset += id->len;
        }

    // pack new instructions
    void* old_code = code->items;
    memcpy(code_section, code->items, code_in_bytes);
    code->items = code_section;
    code->capacity = code->count;
    free(old_code);
 
    // printf("memory : {%.*s}\n", (int) allocation_size, (const char*)memory);

    Function fn = {
        .id             = name,
        .code           = *code,
        .memory         = memory,
        .symbol_count   = symbols_count,
        .argc           = argc,
        .argv           = args_packed,
    };
    return fn;
}

void dtvm_compile_from_asm(Dtvm* vm, String source) {
    assert(!str_is_empty(source));
    Arena allocator     = {0};
    //String fn_name         = {0};
    String leftovers    = source;
    String fn_name      = {0}; 
    int i = 0;

    FILE* trace_file = vm->asmtracef;

    dta_trace(trace_file, "\n### COMPILING ASSEMBLY ###\n");
    do {
        // TODO: figure out a way to reset code without having problems.
        Instructions code   = {0};
        // arguments data
        size_t  argc = 0;
        // TODO: make this flexible, maybe?
        String argv_data[64] = {0};
        Strings argv = {
            .items = argv_data,
            .capacity = 64
        };

        leftovers = dtvm_bytecode_compile(
                trace_file,
                &allocator, 
                &code, 
                leftovers, 
                &fn_name, 
                &argv);
        
        dta_trace(trace_file,"\tARGS: [ ");
        for(size_t i = 0; i < argv.count; i++) {
            dta_trace(trace_file,"%.*s ", str_fmt(argv.items[i]));
        }
        dta_trace(trace_file, "]\n");

        Function fn = dtvm_function_pack(fn_name, &code, argv.items, argv.count);
        da_append(&(vm->functions), fn);

        // memset(code.items, 0, code.count * sizeof(code.items[0]));
    } while(!str_is_empty(leftovers));
    arena_free(&allocator);
}

void dtvm_function_free(Function* fn) {
    free(fn->memory);
    memset(fn, 0, sizeof(*fn));
}


#endif //__DTVM_ASSEMBLER_H
