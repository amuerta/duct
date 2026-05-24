#include <stdio.h>
#include "../../src/dt.h"

int main(int argc, char** argv) {
    static char buffer[1<<10];
    const char* source = 0;    
    DtInterpreter interp = {0};

    if(argc>1 && argv[1]) {
        source = argv[1];
    } else {
        fprintf(stderr, "USAGE: %s <EXPR>", argv[1]);
    }
    
    snprintf(buffer, sizeof(buffer)-1, "main() {return (%s)}", source);
    dt_compile_run_reset(&interp, buffer);
    
    Object result = interp.vm.returned_object;
    printf("> %s", object_to_string(result));
}
