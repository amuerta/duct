// COMPILER ANALYSIS 
#include "declarations.h"

DtParserFunctions dtp_init_function_symbols(void) {
    const  size_t cap = DTP_FUNCTION_SYMBOLS_STARTING_CAPACITY;
    const DtParserFunctions reference = {0};
    DtParserFunctions map = {
        .items      = calloc(sizeof(*reference.items), cap),
        .map_head   = hc_map_heap(cap, sizeof(*reference.items))
    };
    hc_map_set_default_hashes(&map.map_head);
    return map;
}


void dtp_free_function_symbols(DtParserFunctions* s) {
    assert(s->items);
    free(s->items);
    hc_map_heap_free(&(s->map_head));
    memset(s, 0, sizeof(*s));
}



bool dtp_validate_function(DtParserFunctions* map, DtParserFunctionSymbol s) {
    assert(hc_map_load(map->map_head) < 1.0); // handle this later
    String id = s.name;
    DtParserFunctionSymbol* fn =
        hc_map_get_or_reserve(
            map, 
            hc_map_slice(id.ptr, id.len));
    // fn is always not null, but to be sure assert exists.
    assert(fn);

    if(fn->already_seen) {
        if (fn->argc != s.argc)                            
            return false;
        if ((!fn->has_return_value) && s.used_in_expression) 
            return false;
    } else {
        fn->name = s.name;
        fn->argc = s.argc;
        fn->already_seen = true;
    }
    return true;
}
