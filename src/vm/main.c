#include "dtvm.h"

/* DTVM - High-Level Virtual Machine
 *
 * Very sloppy but easy to comprehend virtual machine made for Duct
 * Duct language is compiled with single-pass compiler that directly emits 
 * DTVM instruction code.
 *
 * Having to do both - compilation step and virtual machine interpreting is both: more fun and
 * also interesting then just evaluating AST. Also using virtual machine may result in better performance
 * in the future.
 */

/*  # How this works:
 *  
 *  program has two main workspaces:
 *  - compute stack
 *  - object map
 *
 *  1) All the arithmetic computations are done on the COMPUTE STACK
 *  compute stack is just stack of objects that require computation - hence the name.
 *  Every expression that requires evaluation is first computed with the stack
 *  for instance (10 + 34.0) / 2 is compiled to stack machine instructions and calculated.
 *  If stack has more than 1 item left on it, it's a bug
 *
 *  (10 + 34.0) / 2 => (10 + 34) / 2:
 *      PUSH Object(int,  2)
 *      PUSH Object(int, 10)
 *      PUSH Object(int, 34)
 *      ADD
 *      DIV
 *
 *  2) Object map is literally a fixed-size map of objects, every "variable" is stored in object map.
 *  Each "scope" or "call frame" or "function stack frame" has separate from global space map.
 *  Instructions that require read or write get the Object reference via hashing it's name (?consider other options).
 *  Idea is that variable or global symbols are obtained from maps instead of doing possibly complicated stack operations
 *  (at least at stage of development)
 */



// TODO: MAKE PROPER API AND DEFINE IMPORTANT PIECES
//       THAT PERFORM MEMORY MOVING BETWEEN SCOPES,
//       MEMORY MANAGMENT VIA ARENAS OR HEAP,
//       OBJECT BINDING, etc.
// TODO: - GLUE VM WITH COMPILER
// TODO: make functions stored in the map too
// TODO: constants
// TODO: test suit

#if 1
void test_assembler_0() {
    Hlvm vm = {
        .tracef = stdout,
        .writef = stdout,
    };

    String example_if = str_make(
            // $ - nameless variable
            "push 5     \n" 
            
            // if $ == 10
            "cmp 10     \n"
            "ijif 5     \n"
            "pop        \n"
            "pop        \n"
            "push 420   \n"
            "wrt        \n"
            "jmp 27     \n" // end
            
            // else if $ == 1
            "pop        \n"
            "cmp 1      \n"
            "ijif 5     \n"
            "pop        \n"
            "pop        \n"
            "push 1337  \n"
            "wrt        \n"
            "jmp 27     \n" // end

            // else if $ == 5
            "pop        \n"
            "cmp 5      \n"
            "ijif 5     \n"
            "pop        \n"
            "pop        \n"
            "push 69    \n"
            "wrt        \n"
            "jmp 27     \n" // end

            // clean up if none of the branches match
            "pop\n"
            "pop\n"
            "nop\n"

            // other code
            "nop\n"
    );

    String example_loop = str_make(
        // set iterator
        "push 0\n"
        // write n
        "@loop1\n"
        "wrt\n"
        // nested loop { 
            // set iterator
            "dup\n"
            "@loop2\n"
            // write n
            "wrt\n"
            // i < 9
            "lt 9\n"
            "ijif 5\n"
            // i += 1
            "pop\n"
            "push 1\n"
            "add\n"
            // start over
            "jmp loop2\n"
            
            //end
            "nop\n"
            "pop\n"

            // pop iterator
            "pop\n"
        // }
        // i < 9
        "lt 9\n"
        "ijif 5\n"
        // i += 1
        "pop\n"
        "push 1\n"
        "add\n"
        // start over
        "jmp loop1\n" // we jump over the 0th instruction, and execute 1st one
        
        //end
        "nop\n"
        "pop\n"

        // pop iterator
        "pop\n"
    );

    String symbols = str_make(
        "push 10\n"
        "push 20\n"
        "store n2\n"
        "store n1\n"
    );


    // String compiled_example = str_make(
    //         "fn main\n"
    //         "        push 1\n"
    //         "        push 2\n"
    //         "        sub\n"
    //         "        ret\n"
    //         "endfn\n"
    // );

    String compiled_example = str_make(
            "fn main\n"
            "        push 10\n"
            "        push 10\n"
            "        add\n"
            "        push 1\n"
            "        push 2\n"
            "        sub\n"
            "        add\n"
            "        ret\n"
            "endfn\n"
    );

    String example_add = str_make(
            "load n1\n"
            "load n2\n"
            "add\n"
            "ret\n"
    );

    String example_call = str_make(
        "push 10\n"
        "push 20\n"
        "call add\n"
        "push 30.0\n"
        "add\n"
    );

    String example_rotation = str_make(
        "fn main \n"
            "push 10\n"
            "push 20.0\n"
            "call add\n"
            // "add\n"
            "push 0\n"
            "not\n"
            "add\n"
        "endfn\n"
        "fn add l r\n"
            "load r\n"
            "load l\n"
            "add\n"
            "ret\n"
        "endfn\n"

    );

    String example_labeled_loop = str_make(
            // set iterator
            "push 0\n"
            "@loop\n"
            // write n
            "wrt\n"
            // i < 9
            "lt 9\n"
            "ijif 5\n"
            // i += 1
            "pop\n" // remove bool from 'lt'
            "push 1\n" 
            "add\n" 
            // start over after `push 0`
            "jmp loop\n"

            //end
            "nop\n"
            "pop\n"
    );


    Function fn1 = {0};
    Function fn2 = {0};
    Instructions code = {0};

    // fn1 = dtvm_function_from_asm("add", example_add, 
    //         va_array(Object, {
    //             object_arg(string("n1"), OT_INT), 
    //             object_arg(string("n2"), OT_INT)
    //         })
    // );

    dtvm_compile_from_asm(&vm, compiled_example);

    for(size_t i = 0; i<vm.functions.count; i++) {
        printf("vm.functions[%i] = '%.*s'\n", i, str_fmt(vm.functions.items[i].id));
        Function fn = vm.functions.items[i];
        for(size_t in = 0; in < fn.code.count; in++) {
            printf("\t> %lu kind: %i\n", in, fn.code.items[in].kind);
        }
    }

    // printf("\n\nCALLING 'main': \n");

    // dtvm_add_function(&vm, fn1);
    // dtvm_add_function(&vm, fn2);
    dtvm_call(&vm, string("main"), NULL, 0);

    printf("\n\nSTACK: \n");
    dtvm_print_stack(&vm);
    printf("-----: \n");
    // dtvm_function_free(&fn1);
    // dtvm_function_free(&fn2);
    dtvm_free(&vm);
}
#endif

int main(void) {
     test_assembler_0();
}

