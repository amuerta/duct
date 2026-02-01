#include "declarations.h"
#include "assembler.c"

void dtvm_push(Dtvm* vm, Object value) {
    assert(vm->sp != DTVM_STACK_CAPACITY && "Stack overflow");
    vm->stack[vm->sp++] = value;
}

Object dtvm_pop(Dtvm* vm) {
    assert(vm->sp != 0 && "Stack underflow");
    return vm->stack[--vm->sp];
}

Object dtvm_peek(Dtvm* vm) {
    assert(vm->sp != 0 && "Stack underflow");
    return vm->stack[vm->sp-1];
}

Object* dtvm_refer(Dtvm* vm) {
    assert(vm->sp != 0 && "Stack underflow");
    return vm->stack + vm->sp-1;
}


const char* object_to_string(Object o) {
    static char temp[1024];
    memset(temp, 0, sizeof(temp));
    switch(o.type) {
        case OT_NULL:   snprintf(temp, 1024, "null");       break;
        case OT_BOOL:   snprintf(temp, 1024, "%s", o.as.B ? "true" : "false"); break;
        case OT_BYTE:   snprintf(temp, 1024, "%i", o.as.b); break;
        case OT_INT:    snprintf(temp, 1024, "%i", o.as.i); break;
        case OT_FLOAT:  snprintf(temp, 1024, "%f", o.as.f); break;
        default: 
                UNREACHABLE("Unhandled object_to_string type: %i", o.type); 
    }
    return (const char*)temp;
}

void objectmap_print_values(ObjectMap m) {
    // print all key+value pairs to show new map
    for(size_t i = 0; i < m.map_head.capacity; i++) {
        MapKeySlice key = m.map_head.keys[i];
        if(hc_map_key_is_empty(key)) continue;
        String str = {key.items, key.count};

        // printf("$%.*s = ", str_fmt(str));
        // printf("%s", object_to_string(m.items[i]));
        // printf("\n");
    }
}

void dtvm_print_stack(Dtvm *vm) {
    Object* stack = vm->stack;
    if(vm->tracef) 
        for(size_t i = 0;  i < vm->sp && i < DTVM_STACK_CAPACITY; i++)
        {
            Object* iter = stack + i;
            fprintf(vm->tracef,"%06lu\t", i);
            fprintf(vm->tracef,"$%.*s,\t", str_fmt(iter->id));
            fprintf(vm->tracef,"%s;", object_to_string(*iter));
            fprintf(vm->tracef,"\n");
        }
}

//
// IMPLEMENTATION
//

static inline bool object_is_null(Object o) {
    return (!o.type);
}

static inline Object object_bool(bool b) {
    Object o = {.type = OT_BOOL, .as.B = b};
    return o;
}

static inline Object object_byte(char c) {
    Object o = {.type = OT_BYTE, .as.b = c};
    return o;
}

static inline Object object_int(int i) {
    Object o = {.type = OT_INT, .as.i = i};
    return o;
}

static inline Object object_float(float f) {
    Object o = {.type = OT_FLOAT, .as.f = f};
    return o;
}


static inline Object object_type(EObjectType T) {
    Object e = {.type = OT_TYPE, .as.T = T};
    return e;
}

static inline Object object_arg(String ident, EObjectType T) {
    Object e = {
        .type = OT_TYPE, 
        .as.T = T,
        .id = ident
    };
    return e;
}

static inline Object object_result() {
    Object e = {.type = OT_RESULT, .as.r = 0};
    return e;
}

// TODO: use hash function to get the index for next two functions
// {
Object* object_get(ObjectMap* map, String id) {
    Object *v = 0;
    assert(hc_map_load(map->map_head) < 1.0); // handle this later
    assert(sizeof(*v) == map->map_head.typesize && "MISMATCH IN MAP ITEMS TYPE SIZE AND GIVEN ITEM");
    MapKeySlice key = {id.ptr, id.len};
    long int i = hc_map_query(map->map_head, key);
    // doesn't exist
    if(i == -1) return NULL;
    // exists
    return map->items + i;
}


Object* object_reserve_or_get(ObjectMap* map, String id) {
    assert(hc_map_load(map->map_head) < 1.0); // handle this later
    Object* obj = hc_map_get_or_reserve(
            map, 
            hc_map_slice(id.ptr, id.len));
    return  obj;
}


// TODO: proper error stack?
Object object_store(Object* ref, Object value) {
    if (!ref->type) {
        if(string_cmp(ref->id,STRING_NULL))
            *ref = value;
        else {
            ref->type = value.type;
            ref->as   = value.as;
        }
    }
    else switch(ref->type) {
 
        // TODO: FIX BUG WHEN DOING LOOPING WITH THIS ONE
        case OT_BOOL: if (value.type == OT_BOOL)
                      ref->as.b = value.as.b;   else
                      return object_result();    break;

        case OT_BYTE: if (value.type == OT_BYTE)
                      ref->as.b = value.as.b;   else
                      return object_result();    break;

        case OT_INT: if (value.type == OT_INT)
                      ref->as.i = value.as.i;   else
                      return object_result();    break;

        case OT_FLOAT: if (value.type == OT_FLOAT)
                      ref->as.f = value.as.f;   else
                      return object_result();    break;

        case OT_STRING: if (value.type == OT_STRING)
                      ref->as.s = value.as.s;   else
                      return object_result();    break;

        default: UNREACHABLE("Unhandled object_write type");
    }
    return object_result();
}
// }

int object_resolve_types(Object* left, Object* right) {
    int b,i;
    float f;

    Object *l, *r;

    if(left->type == right->type) return left->type;
    if (left->type > right->type) {
        l = right;
        r = left;
    } else {
        l = left;
        r = right;
    }

    // TODO: make sane type resolution, improve it.
    switch(l->type) {

        case OT_BOOL:
            switch(r->type) {
                case OT_FLOAT: 
                    f = r->as.f;
                    r->as.B = f;
                    r->type = OT_BOOL;
                    break;
                case OT_INT:
                case OT_BYTE:
                case OT_BOOL:
                    i = r->as.i;
                    r->as.B = i;
                    r->type = OT_BOOL;
                    break;
            }

        // resolve int with other types
        // (float -> int), (bool -> int)
        case OT_INT: 
            switch(r->type) { 
                case OT_FLOAT: 
                    i = r->as.f; goto r_int;
                case OT_BOOL:
                    i = r->as.B; goto r_int;
                    
                    r_int:
                    //debug("%i\n", i);
                    r->as.i = i;
                    r->type = OT_INT;
                    return OT_INT;
                default: UNREACHABLE("Unresolvable downcast from int\n");

            } break;

        default: UNREACHABLE("Unavilable downcast\n");
    }
    return OT_NULL;
}

// CODEGEN
#define object_op(T, OP)  (l.as.T OP r.as.T)
// UN-Neccessary boiler plate shall be exterminated
#define object_binop_generate(name, op) \
    Object object_##name(Object l, Object r) {\
        Object o = {0};\
        switch(o.type = object_resolve_types(&l, &r)) {\
            case OT_BOOL:\
            case OT_BYTE:\
            case OT_INT: \
                         o.as.i = object_op(i, op); break;\
            case OT_FLOAT:\
                         o.as.f = object_op(f, op); break;\
            default: UNREACHABLE("Can't "#name" types");\
        }\
        return o;\
    }\

object_binop_generate(add, +  );
object_binop_generate(sub, -  );
object_binop_generate(mul, *  );
object_binop_generate(div, /  );
object_binop_generate(and, && );
object_binop_generate(or,  || );
object_binop_generate(gt,  >  );
object_binop_generate(lt,  <  );
object_binop_generate(gte, >= );
object_binop_generate(lte, <= );


// NOTE: comparison can be quirky
Object object_eq(Object l, Object r) {
    Object o = {0};
    switch(o.type = object_resolve_types(&l, &r)) {
            case OT_BOOL:
            case OT_BYTE:
            case OT_INT: 
                         return object_bool(object_op(i, == )); 
                         break;
            case OT_FLOAT:
                         return object_bool(object_op(f, == )); 
                         break;
                         
            default: UNREACHABLE("Can't add types");\
    }
    return o;
}

// Object dtvm_add_function(Dtvm* vm, Function fn) {
//     vm->functions.items[vm->functions.count++] = fn;
// }

void object_write(FILE* f, Object o) {
    switch(o.type) {
        case OT_NULL:
            fprintf(f, "NULL");
        case OT_BOOL: 
            fprintf(f, "%s", o.as.B ? "true" : "false");
            break;
        case OT_CHAR: 
            fprintf(f, "%c", o.as.b);
            break;
        case OT_BYTE: 
            fprintf(f, "%u", o.as.b);
        case OT_INT: 
            fprintf(f, "%i", o.as.i);
            break;
        case OT_FLOAT: 
            fprintf(f, "%f", o.as.f);
            break;

        default: UNREACHABLE("object_write unaccepted input"); break;
    }
}

#define dtvm_opt(vm, opt) (vm.options_mirror & (opt))

Object dtvm_exec_step(Dtvm* vm, Function* fn, Instruction inst, size_t* ip) {
    // Get current object pool
    ObjectMap*  scope = dtvm_get_scope(vm);
    Object      result = {0};

    switch (inst.kind) {
        case DTI_NOP: break;

        case DTI_WRT: 
            object_write(vm->writef, dtvm_peek(vm));
            break;

        case DTI_STORE: 
            {
                String  id  = inst.as.store.ident;
                Object obj  = dtvm_pop(vm);
                Object* tp  = object_reserve_or_get(scope, id);
                object_store(tp, obj);
            } break;

        case DTI_RET:
            {
                result = dtvm_pop(vm);
                // entry point
                if(vm->scopes.top == 0) {
                    // TODO mirgrate memory if needed.
                    vm->returned_object = result;
                }
                return result;
            } break;

        case DTI_CALL:
            {
                String id = inst.as.call.ident;
                Function* fn = dtvm_get_function(vm, id);
                assert(fn && "call to underclared function");
                

                // TODO: move this some better place
                Object fn_args[64] = {0};
                for(int i = (int)fn->argc - 1; i >= 0; i--) 
                    fn_args[i] = dtvm_pop(vm);

                size_t prev_sp = vm->sp;
                vm->scopes.top++;
                
                Object ret = dtvm_call(vm, id, fn_args, fn->argc);
                
                vm->sp = prev_sp;
                dtvm_push(vm,ret);

                // printf("call to %.*s\n", str_fmt(id));
                vm->scopes.top--;
            } break;

        case DTI_LOAD:
            {
                Object* obj = object_get(scope, inst.as.load.ident);
                assert(obj);
                dtvm_push(vm, *obj);
            } break;


        case DTI_IJIF:
            {
                // __asm__("int3");
                assert(inst.as.ijif.jumpto != -1 && "Uninitilized jump address from a tag.");
                Object obj = dtvm_peek(vm);
                if(!obj.as.B)  // jump over to next branch check
                    *ip += inst.as.ijif.jumpto;
            } break;

        case DTI_JMP:
            {
                assert(inst.as.ijif.jumpto != -1 && "Uninitilized jump address from a tag.");
                *ip = inst.as.ijif.jumpto;
            } break;

        case DTI_POP:
            dtvm_pop(vm);
            break;

        case DTI_PUSH:
            dtvm_push(vm, inst.as.push.object);
            break;

        case DTI_DUP:
            {
                Object obj = dtvm_pop(vm);
                dtvm_push(vm, obj);
                dtvm_push(vm, obj);
            } break;

        case DTI_ROT:   
                      {
                          Object t1 = dtvm_pop(vm);
                          Object t2 = dtvm_pop(vm);
                          dtvm_push(vm,t1);
                          dtvm_push(vm,t2);
                      } break;


        // lazyy
        #define DTI_LITERAL_COMPARE(proc, instruction_name) { \
                Object l = dtvm_peek(vm);\
                Object r = inst.as.instruction_name.object; \
                dtvm_push(vm, proc(l,r)); \
        }
        case DTI_LEQ:  DTI_LITERAL_COMPARE(object_eq,  leq ) break;
        
        case DTI_LLT:  DTI_LITERAL_COMPARE(object_lt,  llt ) break;
        case DTI_LGT:  DTI_LITERAL_COMPARE(object_gt,  lgt ) break;
        case DTI_LLTE: DTI_LITERAL_COMPARE(object_lte, llte) break;
        case DTI_LGTE: DTI_LITERAL_COMPARE(object_gte, lgte) break;


        // lazyy
        #define DTI_BINOP(proc) { \
                Object l = dtvm_pop(vm); Object r = dtvm_pop(vm); \
                dtvm_push(vm, proc(r,l)); \
        }

        #define DTI_BINCMP(proc) { \
                Object l = dtvm_pop(vm); Object r = dtvm_pop(vm); \
                dtvm_push(vm, object_bool(proc(r,l).as.i)); \
        }

        case DTI_EQ:  DTI_BINCMP(object_eq ) break;
        case DTI_LT:  DTI_BINCMP(object_lt ) break;
        case DTI_GT:  DTI_BINCMP(object_gt ) break;
        case DTI_LTE: DTI_BINCMP(object_lte) break;
        case DTI_GTE: DTI_BINCMP(object_gte) break;


        case DTI_ADD: DTI_BINOP(object_add) break;
        case DTI_SUB: DTI_BINOP(object_sub) break;
        case DTI_MUL: DTI_BINOP(object_mul) break;
        case DTI_DIV: DTI_BINOP(object_div) break;
        case DTI_AND: DTI_BINOP(object_and) break;
        case DTI_OR:  DTI_BINOP(object_or)  break;
        case DTI_NOT:   
                      {
                          Object* b = dtvm_refer(vm);
                          b->as.B   = !b->as.B;
                      } break;

        default: UNREACHABLE("UNSUPPORTED INSTRUCTION");
    }

    return result;
}

Function* dtvm_get_function(Dtvm* vm, String fnid) {
    Function* fn = 0;
    Functions fns = vm->functions;
    for(size_t i = 0; i < fns.count; i++) {
        Function* it = fns.items + i;
        if (string_cmp(it->id, fnid)) { return it; }
    }
    return fn;
}

// TODO: make this flexible or something.
ObjectMap dtvm_init_objectmap(void) {
    const ObjectMap reference = {0};
    ObjectMap map = {
        .items      = calloc(sizeof(*reference.items), DTVM_OBJECT_MAP_CAPACITY),
        .map_head   = hc_map_heap(DTVM_OBJECT_MAP_CAPACITY, sizeof(*reference.items))
    };
    hc_map_set_default_hashes(&map.map_head);
    return map;
}

void dtvm_free_scope(ObjectMap* s) {
    assert(s->items);
    free(s->items);
    hc_map_heap_free(&(s->map_head));
    memset(s, 0, sizeof(*s));
}

ObjectMap* dtvm_get_scope(Dtvm* vm) {
    ObjectMap* current = vm->scopes.buf + vm->scopes.top;
    if(vm->scopes.top >= vm->scopes.count) {
        vm->scopes.count++;
        *current = dtvm_init_objectmap();
    } 
    return current;
}

void dtvm_trace_execution(Dtvm* vm, Instruction inst, size_t ip) {
    if(vm->tracef) {
        fprintf(vm->tracef, "F:%04lu\t %06lu > %s\n", 
                vm->scopes.top,
                ip,
                dtvm_decompile_instruction(inst)
               );
        dtvm_print_stack(vm);
        fprintf(vm->tracef, "----------------------------\n");
    }
}

Object dtvm_call(Dtvm* vm, String fnid, Object* argv, int argc) {
    // find function or return result 
    Function *fn = dtvm_get_function(vm, fnid);
    assert(fn && "Called function is undefined");
    // Get current object pool
    ObjectMap* scope = dtvm_get_scope(vm);

    assert(vm->scopes.top <= 100 && "EXCEEDED CALL DEPTH");

    // Push all arguments 
    assert(fn->argc == argc);
    for(size_t i = 0; i < fn->argc; i++) {
        Object obj  = argv[i];
        String obj_name = fn->argv[i];
        // fprintf(stderr,"obj.id[%i] = %.*s\n", i, slice_fmt(obj.id));
        Object* tp  = object_reserve_or_get(scope, obj_name);
        object_store(tp, obj);
    }
    // Execute instructions TODO(until ret, nop is encountered) or 
    // if all commands are executed
    for(size_t ip = 0; ip < fn->code.count; ip++) {
        Instruction inst = fn->code.items[ip];

        dtvm_trace_execution(vm,inst,ip);

        Object result = dtvm_exec_step(vm, fn, inst, &ip);
        if (!object_is_null(result)) 
            return result;
    }
    objectmap_print_values(*scope);
    //__asm__("int3");


    // return catchable error if failed
    const Object OBJECT_NULL ={0};
    return OBJECT_NULL;
}



void dtvm_free(Dtvm* vm) {
    for(size_t i = 0; i < vm->functions.count; i++) 
        free(vm->functions.items[i].memory);
    for(size_t i = 0; i < vm->scopes.count; i++) {
        // TODO: !IMPORTANT free all the arena data.
        dtvm_free_scope(vm->scopes.buf + i);
    }
    
    free(vm->functions.items);
}
