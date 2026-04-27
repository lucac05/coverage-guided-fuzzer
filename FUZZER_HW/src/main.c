#include "global.h"
#include "fuzzer.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"

#define ERR_EXIT() do{PRINT_USAGE(stderr, argv[0]); return EXIT_FAILURE;}while(0)

int main(int argc, char *argv[]) {  //TODO investigate
    int num_jobs = DEFAULT_RUNNER_COUNT;
    int num_inputs = DEFAULT_INPUT_TOTAL;
    int timelimit = DEFAULT_TIMEOUT_SEC;
    char* seed_fname = NULL;

    int arg_ind = 1;
    while(arg_ind < argc){
        if(strcmp(argv[arg_ind], "-h") == 0){
            PRINT_USAGE(stdout, argv[0]);
            return EXIT_SUCCESS;
        }
        else if(strcmp(argv[arg_ind], "-j") == 0){
            arg_ind++;
            if(arg_ind >= argc || !is_num(argv[arg_ind])){
                fprintf(stderr, "Error: -j requires an argument (number)\n");
                ERR_EXIT();
            }
            num_jobs = atoi(argv[arg_ind]);
        }
        else if(strcmp(argv[arg_ind], "-n") == 0){
            arg_ind++;
            if(arg_ind >= argc || !is_num(argv[arg_ind])){
                fprintf(stderr, "Error: -n requires an argument (number)\n");
                ERR_EXIT();
            }
            num_inputs = atoi(argv[arg_ind]);
        }
        else if(strcmp(argv[arg_ind], "-s") == 0){
            arg_ind++;
            if(arg_ind >= argc){
                ERR_EXIT();
            }
            seed_fname = argv[arg_ind];
        }
        else if(strcmp(argv[arg_ind], "-t") == 0){
            arg_ind++;
            if(arg_ind >= argc || !is_num(argv[arg_ind])){
                fprintf(stderr, "Error: -t requires an argument (number)\n");
                ERR_EXIT();
            }
            timelimit = atoi(argv[arg_ind]);
        }
        else{   //this is the target program, evertyhign after is its args
            break;
        }
        arg_ind++;
    }

    if(!seed_fname){    //-s required
        fprintf(stderr, "Error: Seed file required with -s\n");
        ERR_EXIT();
    }
    if(arg_ind >= argc){    //missing target
        fprintf(stderr, "Error: Target program is required\n");
        ERR_EXIT();
    }

    int found_placeholder = 0;
    char** target_program = argv + arg_ind;
    for(int j = 0; target_program[j] != NULL; j++){
        if(strcmp(target_program[j], PROGRAM_ARGUMENT_PLACEHOLDER) == 0){
            found_placeholder = 1;
            break;
        }
    }

    if(!found_placeholder){
        fprintf(stderr, "Error: Target program args must contain '%s' placeholder\n", PROGRAM_ARGUMENT_PLACEHOLDER);
        ERR_EXIT();
    }

    FILE *seed_fp = fopen(seed_fname, "r");
    if(!seed_fp){
        perror("Error opening seed file");
        return EXIT_FAILURE;
    }

    int res = run_fuzzer(seed_fp, num_jobs, num_inputs, timelimit, target_program);
    fclose(seed_fp);
    if(res == 0){
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}