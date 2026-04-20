#include "fuzzer.h"
#include "global.h"
#include "input_queue.h"
#include "coverage_map.h"
#include "mutator.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <bits/sigaction.h>
#include <sys/select.h>

static volatile sig_atomic_t shutdown_req = 0;
static volatile sig_atomic_t sigchld_seen = 0;
static volatile sig_atomic_t sigusr1_seen = 0;

static void helpr_fuzzer_sighandler(int sig_num){
    if(sig_num == SIGINT || sig_num == SIGTERM || sig_num == SIGHUP){
        shutdown_req = 1;
    }
    else if(sig_num == SIGCHLD){
        sigchld_seen = 1;
    }
    else if(sig_num == SIGUSR1){
        sigusr1_seen = 1;
    }
}

int run_fuzzer(FILE *seed_file, int job_count, int input_count, int time_limit, char *target_program[]) {
    fzl_init("Entering fuzzer...");    //fzl event log

    cmd = *target_program;  //globals
    args = target_program + 1;
    timeout = time_limit;
    program_argc = 0;
    while(args[program_argc] != NULL){
        program_argc++;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = helpr_fuzzer_sighandler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    INPUT_QUEUE queue = input_queue_init();
    COVERAGE_MAP coverage_map = coverage_map_init();
    RUNNERS runners = runners_init(job_count);
    if(!queue || !coverage_map || !runners){
        fprintf(stderr, "Failure in fuzzer initialization steps\n");
        return -1;
    }

    int num_seeds = 0;
    char* cur_line = NULL;
    size_t cur_len = 0;
    ssize_t read_len;
    while((read_len = getline(&cur_line, &cur_len, seed_file)) != -1){  //read seed file
        if(cur_line[read_len - 1] == '\n'){
            cur_line[read_len - 1] = 0;
        }
        INPUT inp = make_input(cur_line);
        enqueue_high_prio_input(queue, inp);
        num_seeds++;
    }
    SAFEFREE(cur_line);

    sigset_t block_smask;
    sigset_t empty_smask;
    sigemptyset(&block_smask);
    sigemptyset(&empty_smask);
    sigaddset(&block_smask, SIGTERM);
    sigaddset(&block_smask, SIGINT);
    sigaddset(&block_smask, SIGHUP);
    sigaddset(&block_smask, SIGUSR1);
    sigaddset(&block_smask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_smask, NULL);

    int jobs_sent = 0;
    int jobs_received = 0;
    int target_total = input_count + num_seeds;

    while(!shutdown_req && jobs_received < target_total){
        if(sigchld_seen){   //pending sigs
            sigchld_seen = 0;
            runners_reap(runners);
            if(!runners_has_jobs(runners)){
                fprintf(stderr, "All runner processes terminated, shutting down...\n");
                shutdown_req = 1;
                continue;
            }
        }
        if(sigusr1_seen){
            sigusr1_seen = 0;
            runners_check_if_jobs_done(runners);
        }

        while(runners_has_done_jobs(runners)){  //handled finished jobs
            RUNNER_STATE state;
            int data = 0;

            RUNNER runner = runners_process_result(runners, &state, &data);
            INPUT tested_inp = runner_get_active_input(runner);
            char* coverage_data = runner_coverage_map(runner);

            if(state == VALID){
                COVERAGE_PRIORITY priority = coverage_map_add(coverage_map, coverage_data);
                if(priority == COV_HIGH_PRIO){
                    enqueue_high_prio_input(queue, tested_inp);
                }
                else if(priority == COV_LOW_PRIO){
                    enqueue_low_prio_input(queue, tested_inp);
                }
                else{
                    free_input(tested_inp);
                }
            }
            else if(state == CRASH){
                printf("***CRASH TRIGGERED*** Input: %s\n", input_str(tested_inp));
                free_input(tested_inp);
            }
            else if(state == TIMEOUT){
                free_input(tested_inp);
            }
            jobs_received++;
        }

        while(runners_has_ready_jobs(runners) && jobs_sent < target_total){
            INPUT inp = dequeue_input(queue);
            if(!inp){
                break;
            }

            INPUT to_send = NULL;
            if(input_mutator_state(inp) == 0){
                to_send = make_input(input_str(inp));
                input_state_step(inp);
            }
            else{
                to_send = mutate(inp);
            }

            if(runners_submit_input(runners, to_send) == 0){
                jobs_sent++;
            }
            else{
                free_input(to_send);
                break;
            }
        }

        if(jobs_received >= target_total){
            break;
        }
        if(!sigchld_seen && !sigusr1_seen && runners_has_active_jobs(runners) && !shutdown_req){
            sigsuspend(&empty_smask);
        }
    }

    runners_fini(runners);
    coverage_map_fini(coverage_map);
    input_queue_fini(queue);

    fzl_fini("Exiting fuzzer"); //fzl event log
    return 0;
}