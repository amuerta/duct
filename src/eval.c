#include "common.c"
#include "parser.c"
#include "object.c"


#ifndef __DT_EVAL_H
#define __DT_EVAL_H


DtObject dte_eval_function(DtContext* ctx, DtNode* node);

DtIdentifer dte_ident_from_token(Token t) {
    DtIdentifer ident = {
        .name   = t.data.as_word.data,
        .length = t.data.as_word.length,
    };
    return ident;
}

bool dte_has_ident(DtObject o) {
    return o.identifier.name != 0 && o.identifier.length > 0;
}

bool dte_object_is_valid(DtObject o) {
    return o.value.type != 0;
}

DtObject dte_object_from_numeric_literall(DtNode* node) {
    switch(node->kind) {
        case NK_BOOLIT:
            return dto_object_from_numeric(
                    dto_ident(""), 
                    DT_TYPE_BOOL, 
                    node->identifier.data.as_int);
        case NK_INTLIT:
            return dto_object_from_numeric(
                    dto_ident(""), 
                    DT_TYPE_INT, 
                    node->identifier.data.as_int);
        case NK_FLTLIT:
            return dto_object_from_numeric(
                    dto_ident(""), 
                    DT_TYPE_FLOAT, 
                    dto_numeric_from_float(node->identifier.data.as_float));
        default: assert(0 && "Expected numeric AST node");
    }
}

DtIdentifer dte_ident_from_id(Arena* a, int n) {
    DtIdentifer ident;
    static char temp[1024];
    sprintf(temp, "$%i", n);
    const char* id = arena_put_string(a, temp); 
    ident.name   = id;
    ident.length = strlen(id);
    memset(temp, 0, sizeof(temp));
    return ident;
}

DtObject* dte_lookup_object(DtContext* ctx, DtIdentifer ident) {
    //DtScopeList top = ctx->sfs[ctx->call_depth-1].scopes;
    DtScope* current = ctx->current;
    DtObject* result = 0;
    DtObject* it = dto_scope_ref(current, ident);
    (void) result;
    return it;
}


//TODO: handle unary
DtObject dte_eval_expression(DtContext* ctx, DtNode* node) {
    Arena* allocator = &(ctx->main_allocator);
    //Arena* name_alloc = &(ctx->name_allocator);
    DtObject v1, v2;
    DtObject *r1 = 0;

    if(!node) return DT_OBJECT_NULL;
    switch(node->kind) {
        case NK_EXPRESSION:
            return dte_eval_expression(ctx, node->children);

        case NK_TERM:
            v1 = dte_eval_expression(ctx, node->children);
            v2 = dte_eval_expression(ctx, node->children->next);
            if (node->properties & NKP_IS_ADD) 
                return dto_object_binop(v1, v2, DT_BINOP_ADD);
            else 
                return dto_object_binop(v1, v2, DT_BINOP_SUB);

        case NK_FACTOR:
            v1 = dte_eval_expression(ctx, node->children);
            v2 = dte_eval_expression(ctx, node->children->next);
            if (node->properties & NKP_IS_MUL) 
                return dto_object_binop(v1, v2, DT_BINOP_MUL);
            else 
                return dto_object_binop(v1, v2, DT_BINOP_DIV);

        case NK_EQALITY:
            v1 = dte_eval_expression(ctx, node->children);
            v2 = dte_eval_expression(ctx, node->children->next);
            return dto_object_compare(v1, v2, DT_COMPARE_EQ);

            // LITTERALS
        case NK_BOOLIT:
        case NK_INTLIT:
        case NK_FLTLIT:
            return dte_object_from_numeric_literall(node);
        case NK_STRLIT:
            v1 = dto_string_new(allocator, dto_ident(""), 0, 
                    node->identifier.data.as_word.length);
            dt_error result = dto_string_set_sized(&v1, 
                    node->identifier.data.as_word.data,
                    node->identifier.data.as_word.length
            );
            // TODO check result
            (void) result;
            return v1;

        case NK_FUNCTION_CALL:
            {
                // find function, make sure it exists
                DtObject func = dto_scope_get(ctx->functions, dte_ident_from_token(node->identifier));
                // TODO: error checking
                assert(dte_object_is_valid(func)); 
                v1 = dte_eval_function(ctx, (DtNode*)func.value.as_function.entry);
                return v1;
            }
            break;

        case NK_IDENTIFIER: 
            r1 = dte_lookup_object(ctx, dte_ident_from_token(node->identifier));
            assert(r1);
            v1 = *r1;
            return v1;
            
        default:
            return DT_OBJECT_NULL;
    }
}

DtObject dte_object_from_ast(DtContext* ctx, DtNode* node, int parent_id) {
    DtObject o  = {0}, 
             v1 = {0}
             //v2 = {0}
    ;

    Arena* allocator = &(ctx->main_allocator);
    Arena* name_alloc = &(ctx->name_allocator);
    int field_id = 0;
    DtNode* child = node ? node->children : 0;

    if (!node) return o;

    //fprintf(stderr, "%s\n", DT_NODE_KIND_STR[node->kind]);

    // prase name
    switch(node->kind) {
        case NK_VARIABLE:
            o = dto_object_new(dte_ident_from_token(node->identifier), 0);
        break;

        case NK_RVALUE: 
            o = dte_object_from_ast(ctx, child, 0);
            o.identifier = dte_ident_from_id(name_alloc, parent_id);
            return o;
        break;

        case NK_OBJECT: 
            {
                o.value.type = DT_TYPE_OBJECT;
                DtNode* fields = node->children;
                while(fields) {
                    v1 = dte_object_from_ast(ctx, fields, field_id);
                    if (!dte_has_ident(v1))
                        v1.identifier = dte_ident_from_token(fields->identifier);
                    dto_object_append(allocator, &o, v1);
                    
                    fields = fields->next;
                    field_id++;
                }
                return o;
            } break;
        default: return DT_OBJECT_NULL;
    }

    // parse data
    if (child) switch(child->kind) {

        // TODO:
        case NK_VARIABLE:
        case NK_RVALUE:
            return dte_object_from_ast(ctx, child, parent_id);

        case NK_OBJECT: 
            {
                DtNode* fields = child->children;
                while(fields) {
                    v1 = dte_object_from_ast(ctx, fields, field_id);
                    if (!dte_has_ident(v1))
                        v1.identifier = dte_ident_from_token(fields->identifier);
                    dto_object_append(allocator, &o, v1);
                    
                    fields = fields->next;
                    field_id++;
                }
                return o;
            } break;

        case NK_EXPRESSION:
            return dte_eval_expression(ctx, node->children);
        default: return DT_OBJECT_NULL;
    }
    return o;
}


DtObject dte_eval_scope(DtContext* ctx, DtNode* node) {
    DtObject var;
    DtObject* ref;
    DtScope* s = ctx->current;
    assert(node->kind == NK_BLOCK);
    
    DtNode* next = node->children;
    while(next) {
        switch(next->kind) {
            
            case NK_VARIABLE:
                ref = dte_lookup_object(ctx, dte_ident_from_token(next->identifier));
                if (!ref) {
                    var = dte_object_from_ast(ctx,next, 0);
                    var.identifier = dte_ident_from_token(next->identifier);
                    dto_scope_push(s, var);
                } else {
                    *ref = dte_eval_expression(ctx, next->children);
                }
                break;

            case NK_RETURN:
                var = dte_eval_expression(ctx, next->children);
                ctx->ret = var;
                goto end;
                break;

            case NK_IF_STATEMENT:
                {
                    DtNode* ifb     = dtp_node_get(next, NK_IF);
                    DtNode* elseb   = dtp_node_get(next, NK_ELSE);
                    DtNode* elseifb = dtp_node_get(next, NK_ELSEIFS);

                    if(!ctx->eval_mode) {
                        // if branch is always present
                        var = dte_eval_expression(ctx, dtp_node_index(ifb->children, 0));
                        if (var.value.as_byte) {
                            dte_eval_scope(ctx, dtp_node_index(ifb->children, 1));
                            goto end_statement;
                        }
                        
                        // else if branches
                        if(elseifb) {
                            DtNode* elseifnext = elseifb->children;
                            while(elseifnext) {
                                var = dte_eval_expression(ctx, dtp_node_index(elseifnext->children, 0));
                                if (var.value.as_byte) {
                                    dte_eval_scope(ctx, dtp_node_index(elseifnext->children, 1));
                                    goto end_statement;
                                }
                                elseifnext = elseifnext->next;
                            }
                        }

                        // else branch
                        if(elseb) {
                            dte_eval_scope(ctx, dtp_node_index(elseb->children, 0));
                        }
                        
                    }
                }
                break;

                // function call not in expression, return ignored
            case NK_FUNCTION_CALL: 
                {
                    // find function, make sure it exists
                    DtObject func = dto_scope_get(ctx->functions, dte_ident_from_token(next->identifier));
                    // TODO:
                    printf("called function\n");
                    assert(dte_object_is_valid(func)); 
                    dte_eval_function(ctx, (DtNode*)func.value.as_function.entry);
                }
                break;

            default: assert(0 && "TODO:");
        }
end_statement:
        next = next->next; 
    }

end:
    return var;
}

DtObject dte_eval_function(DtContext* ctx, DtNode* node) {
    DtNode* body = dtp_node_get(node, NK_BLOCK);
    assert(node->kind == NK_FUNCTION_DECL);
    assert(body->kind == NK_BLOCK);
    
    // setup
    DtObject ret = DT_OBJECT_NULL;
    DtScope 
        s = dto_scope_init(),
        *before = ctx->current;
    ctx->call_depth++;
    ctx->current = &s;

    // body
    ret = dte_eval_scope(ctx, body);
    ret = ctx->ret;

    // restore
    ctx->current = before;
    dto_scope_clear(&s);
    ctx->call_depth--;
    return ret;
}

dt_enum8 dte_basic_type_from_ast(DtNode* n) {
    if (!n) return DT_TYPE_VOID;
    Token ident = n->identifier;
    if      (Token_compare_cstr(ident, "byte"))     return DT_TYPE_BYTE;
    else if (Token_compare_cstr(ident, "bool"))     return DT_TYPE_BOOL;
    else if (Token_compare_cstr(ident, "int"))      return DT_TYPE_INT;
    else if (Token_compare_cstr(ident, "long"))     return DT_TYPE_LONG;
    else if (Token_compare_cstr(ident, "float"))    return DT_TYPE_FLOAT;
    else if (Token_compare_cstr(ident, "double"))   return DT_TYPE_DOUBLE;
    else if (Token_compare_cstr(ident, "string"))   return DT_TYPE_STRING;
    else return DT_TYPE_VOID;
}

void dte_eval_prepass(DtContext* ctx, DtNode* tree) {
    DtScope* funcs = &ctx->functions;
    DtNode* next = tree->children;
    while(next) {
        switch(next->kind) {
            case NK_FUNCTION_DECL:
                {
                    DtNode* arguments = dtp_node_get(next, NK_FUNCTION_ARGS);
                    DtNode* ret_type  = dtp_node_get(next, NK_TYPE);

                    DtObject obj_func = {
                        .identifier = dte_ident_from_token(next->identifier),
                        .value.type = DT_TYPE_FUNCTION,
                        .value.as_function = {
                            .name = dte_ident_from_token(next->identifier),
                            .return_type = dte_basic_type_from_ast(ret_type),
                            .entry = next,
                        }
                    };

                    DtObject obj_args = {0};
                    DtNode* argnext = arguments->children;
                    while(argnext) {
                        DtObject arg = {
                            .value.type = DT_TYPE_TYPEDEF,
                            .value.as_type.typeid = dte_basic_type_from_ast(argnext->children)
                        };
                        dto_object_append(&funcs->temporary_memory, &obj_args, arg);
                        argnext = argnext->next;
                    }

                    dto_scope_push(funcs, obj_func);
                }
                break;

            default: assert(0);
        }
        next = next->next;
    }
}

void dte_eval_root(DtContext* ctx, DtNode* tree) {

    // TODO: user can define entry point
    assert(ctx);

    static char buffer[1024];

    DtSerializeOpt opt = {
        .spacing = "  ",
        .flags = 0 |
            DT_SERIALIZE_PRETTIFY               | 
            DT_SERIALIZE_WITH_NAMES             |
            DT_SERIALIZE_PUT_STRING_QUOTATIONS
        ,
    };

    // TODO: proper tree treversal
    // temporary get first item in block of first (main) function
    DtNode* main = tree->children;
    //fprintf(stderr, "%s\n", DT_NODE_KIND_STR[ast_obj->kind]);
    assert(main->kind == NK_FUNCTION_DECL);
    DtObject o = dte_eval_function(ctx, main);

    dto_serialize(buffer, 1024, opt, o);
    printf("%s\n", buffer);
    
    arena_reset(&ctx->name_allocator);
    arena_reset(&ctx->main_allocator);
}

#endif
