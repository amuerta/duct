#include <stdio.h>
#include <unistd.h>
#include "dt.h"

int main(int argc, char* argv[]) {
    bool no_run = false;
    bool buildin_exec = false;
    bool read_stdin = false;
    bool trace_vm = false;
    bool be_quiet = false;
    const char* source = NULL;
    
    for(int i = 1; i < argc; i++) {
        if(argv[i] && (
                    !strcmp(argv[i], "--quiet")     ||
                    !strcmp(argv[i], "--bequiet")   ||
                    !strcmp(argv[i], "bequiet")     ||
                    !strcmp(argv[i], "quiet") 
                    )) {
            be_quiet = true;
        } else if (argv[i] &&
                (
                    !strcmp(argv[i], "stdin")     ||
                    !strcmp(argv[i], "--stdin")   ||
                    !strcmp(argv[i], "read")     ||
                    !strcmp(argv[i], "--read") 
                   )) {
            read_stdin = true;
        } else if (argv[i] && (!strcmp(argv[i], "--tracevm") || !strcmp(argv[i], "tracevm"))) {
            trace_vm = true;
        } else if (argv[i] && (!strcmp(argv[i], "--buildin") || !strcmp(argv[i], "buildin"))) {
            buildin_exec = true;
        } else if (argv[i] && (!strcmp(argv[i], "--norun") || !strcmp(argv[i], "norun"))) {
            no_run = true;
        }


    }

    if (!isatty(STDIN_FILENO) && 1) {
        static char file[1024], line[128];
        
        while (fgets(line, sizeof(line), stdin) != NULL) {
            strncat(file, line, sizeof(file)-1);
        }
        source = file;
    } else if(argc > 1 && argv[1]) {
        source = argv[1];
    } else if (buildin_exec) {

    }
    else {
        fprintf(stderr, "USAGE: %s { [file <PATH>] | <SCRIPT_SOURCE> }", argv[0]);
        exit(1);
    }

    DtInterpreter interp = {

        .options = (be_quiet)? 0 : 
            0 
            // | DT_OPT_EXPECT_INITILIZED_STREAM_FILES
            | (DT_OPT_TRACE_VM_EXECUTION * trace_vm)
            | (DT_OPT_TRACE_VM_ASSEMBLY_COMPILATION * !no_run)   
            | (DT_OPT_TRACE_COMPILER_AST_WALKING)
        ,
        .tracef  = be_quiet ? NULL : stderr,
        .outputf = stdout,
    };

    // TODO: print("string"i) or print(i"string")
    // is not correctly errored and instead compiled into recursive mess.

    if(buildin_exec) 
        source = str_multiline(
            recursive_print(n) {
                if n > 0 {
                    print(n, "\n")
                    recursive_print(n - 1)
                }
                return 0
            }

            main() {
                n = 10
                recursive_print(n)
            }
        );

#if 0
    const char* source = str_multiline(
        add(a,b) {
            c = b + a
            return c
        }

        main() {
            i = 0
            while i < 5 {
                i = i + 1
            }
        }
    );
#endif
    // const char* source = str_multiline(
        // main() {
            // i = 0
            // while i < 10 {
                // print(i, "\n");
                // i=i+1
            // }
        // }
    // );

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

    //
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


    // dt_append_assembly(&interp, 
    //         "fn main\n"
    //             "push \"hello world!\"\n"
    //             "wrt\n"
    //             "pop\n"
    //         "endfn\n"
    // );

    dt_append_assembly(&interp, 
            "fn write v\n"
                "load v\n"
                "wrt\n"
                "pop\n"
            "endfn\n"
    );
    // dt_reset(&interp);
    if(no_run) {
        dt_compile(&interp, source);
    } else {
        dt_compile_run_reset(&interp, source);
    }
    // dt_run_reset(&interp, "main");
    // printf("\n");
    // TODO: check for empty file
    // free(txt);
}

