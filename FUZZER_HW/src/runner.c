#include "runner.h"
#include "global.h"
#include "utils.h"
#include "fuzzer.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <bits/sigaction.h>

#define MAP_BYTES (COVERAGE_MAP_SIZE / 8)

static int next_runner_id = 0;

struct runner{
    int id;
    pid_t pid;

    int pipe_f_to_r[2];
    int pipe_r_to_f[2];

    char shm_name[64];
    int shm_fdesc;
    char* shm_mmap;
    INPUT active_input;
};

RUNNER runner_init() {
    RUNNER runner = calloc(1, sizeof(struct runner));
    if(!runner){
        return NULL;
    }

    runner->id = next_runner_id++;
    runner->pid = -1;   //set actual pid upon launch
    runner->shm_fdesc = -1;    //init as -1 to track open/close
    runner->pipe_f_to_r[0] = -1;
    runner->pipe_f_to_r[1] = -1;
    runner->pipe_r_to_f[0] = -1;
    runner->pipe_r_to_f[1] = -1;

    int bad_pipe = pipe(runner->pipe_f_to_r) == -1;
    if(bad_pipe){
        perror("Failure in pipe()");
        goto init_error;
    }
    bad_pipe = pipe(runner->pipe_r_to_f) == -1;
    if(bad_pipe){
        perror("Failure in pipe()");
        goto init_error;
    }

    int flags = fcntl((runner->pipe_r_to_f)[0], F_GETFL, 0);
    if(flags == -1){
        perror("Failure in fcntl()");
        goto init_error;
    }
    fcntl((runner->pipe_r_to_f)[0], F_SETFL, flags | O_NONBLOCK);

    snprintf(runner->shm_name, sizeof(runner->shm_name), "/fuzzer_shm%d", runner->id);
    runner->shm_fdesc = shm_open(runner->shm_name, O_CREAT | O_RDWR, 0666);
    if(runner->shm_fdesc == -1){
        perror("Failure in shm_open()");
        goto init_error;
    }

    int bad_trunc = ftruncate(runner->shm_fdesc, MAP_BYTES) == -1;
    if(bad_trunc){
        perror("Failure in ftruncate()");
        goto init_error;
    }

    runner->shm_mmap = mmap(NULL, MAP_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, runner->shm_fdesc, 0);
    if(runner->shm_mmap == MAP_FAILED){
        perror("Failure in mmap()");
        runner->shm_mmap = NULL;
        goto init_error;
    }

    memset(runner->shm_mmap, 0, MAP_BYTES);
    return runner;

init_error:
    if(runner->pipe_f_to_r[0] != -1){
        close(runner->pipe_f_to_r[0]);
        close(runner->pipe_f_to_r[1]);
    }
    if(runner->pipe_r_to_f[0] != -1){
        close(runner->pipe_r_to_f[0]);
        close(runner->pipe_r_to_f[1]);
    }
    if(runner->shm_fdesc != -1){
        close(runner->shm_fdesc);
        shm_unlink(runner->shm_name);
    }
    SAFEFREE(runner);
    return NULL;
}

void runner_fini(RUNNER runner) {
    if(!runner){
        return;
    }

    if(runner->pid > 0){
        kill(runner->pid, SIGTERM);
    }

    close(runner->pipe_f_to_r[0]);    //pipe cleanup
    close(runner->pipe_f_to_r[1]);
    close(runner->pipe_r_to_f[0]);
    close(runner->pipe_r_to_f[1]);

    if(runner->shm_mmap && runner->shm_mmap != MAP_FAILED){
        munmap(runner->shm_mmap, MAP_BYTES);
    }
    if(runner->shm_fdesc != -1){
        close(runner->shm_fdesc);
    }

    shm_unlink(runner->shm_name);

    free(runner);
}

char *runner_coverage_map(RUNNER runner) {
    if(!runner){
        return NULL;
    }

    return runner->shm_mmap;
}

INPUT runner_get_active_input(RUNNER runner) {
    if(!runner){
        return NULL;
    }

    return runner->active_input;
}

typedef struct{
    RUNNER_STATE state;
    int xtra_data;
} StatusInfo;

static ssize_t helpr_write_wrap(int fdesc, const void* buf, size_t count){
    size_t written_len = 0;
    char* ptr = (char*)buf;

    while(written_len < count){
        ssize_t bytes_written = write(fdesc, ptr + written_len, count - written_len);
        if(bytes_written == -1){
            if(errno == EINTR){
                continue;   //if just interr then continue
            }
            return -1;  //real error
        }
        written_len += bytes_written;
    }

    return written_len;
}

static ssize_t helpr_read_wrap(int fdesc, const void* buf, size_t count){ //use void* for generics
    size_t read_len = 0;
    char* ptr = (char*)buf;

    while(read_len < count){
        ssize_t bytes_read = read(fdesc, ptr + read_len, count - read_len);
        if(bytes_read == -1){
            if(errno == EINTR){
                continue;   //if just interr then continue
            }
            return -1;  //real error
        }
        if(bytes_read == 0){
            break;  //hit EOF
        }
        read_len += bytes_read;
    }

    return read_len;
}

int fuzzer_send_runner_input(RUNNER runner, INPUT input) {
    if(!runner || !input){
        return -1;
    }

    size_t len = input_len(input);
    const char* str = input_str(input);

    fzl_sending_input(runner->id, str, "Sending mutated input to runner..."); //fzl event log
    if(helpr_write_wrap(runner->pipe_f_to_r[1], &len, sizeof(size_t)) == -1){
        return -1;
    }

    if(len > 0 && (helpr_write_wrap(runner->pipe_f_to_r[1], str, len) == -1)){    //writing sucess
        return -1;
    }

    runner->active_input = input;
    return 0;
}

char * runner_receive_fuzzer_input(RUNNER runner) {
    if(!runner){
        return NULL;
    }

    fd_set read_fdescs;
    FD_ZERO(&read_fdescs);
    FD_SET(runner->pipe_f_to_r[0], &read_fdescs);

    sigset_t empty_smask;
    sigemptyset(&empty_smask);

    int done = pselect(runner->pipe_f_to_r[0] + 1, &read_fdescs, NULL, NULL, NULL, &empty_smask);
    if(done == -1){
        if(errno == EINTR){
            return NULL;
        }
        perror("Failure in pselect()");
        return NULL;
    }

    size_t len = 0;
    if(helpr_read_wrap(runner->pipe_f_to_r[0], &len, sizeof(size_t)) <= 0){
        return NULL;    //bad read
    }

    char* input_str = calloc(1, len + 1);   //+1 for null term
    if(!input_str){
        return NULL;
    }

    if(len > 0 && (helpr_read_wrap(runner->pipe_f_to_r[0], input_str, len) <= 0)){
        SAFEFREE(input_str);
        return NULL;
    }

    fzl_runner_received_input(runner->id, input_str, "Runner received input");  //fzl event log
    return input_str;
}

int runner_alert_fuzzer(RUNNER runner, RUNNER_STATE state, int data) {
    if(!runner){
        return -1;
    }

    StatusInfo info;
    info.state = state;
    info.xtra_data = data;

    fzl_runner_sending_status(runner->id, state, data, "Runner sending status packet to fuzzer"); //fzl event log
    if(helpr_write_wrap(runner->pipe_r_to_f[1], &info, sizeof(StatusInfo)) == -1){
        return -1;
    }
    if(kill(getppid(), SIGUSR1) == -1){
        perror("Failure in signaling SIGUSR1 to parent");
        return -1;
    }
    return 0;
}

RUNNER_STATE fuzzer_attempt_receive_status(RUNNER runner, int *data) {
    if(!runner){
        return NO_STATE;
    }

    StatusInfo info;
    ssize_t bytes_read = read(runner->pipe_r_to_f[0], &info, sizeof(StatusInfo));

    if(bytes_read != sizeof(StatusInfo)){
        return NO_STATE;
    }

    if(data && info.state != TIMEOUT){
        *data = info.xtra_data;
    }
    return info.state;
}

static volatile sig_atomic_t sigterm_seen = 0;
static volatile sig_atomic_t sigalrm_seen = 0;
static volatile sig_atomic_t sigchld_seen = 0;
static volatile pid_t current_target_pid = -1;

static void helpr_runner_sighandler(int sig_num){
    if(sig_num == SIGTERM || sig_num == SIGINT || sig_num == SIGHUP){
        sigterm_seen = 1;
        if(current_target_pid > 0){
            kill(current_target_pid, SIGKILL);
        }
    }
    else if(sig_num == SIGALRM){
        sigalrm_seen = 1;
        if(current_target_pid > 0){
            kill(current_target_pid, SIGALRM);
        }
    }
    else if(sig_num == SIGCHLD){
        sigchld_seen = 1;
    }
}

int runner_launch(RUNNER runner) {
    if(!runner){
        return -1;
    }

    pid_t pid = fork();
    if(pid == -1){
        perror("Failure in fork()");
        return -1;
    }
    if(pid > 0){    //PARENT
        runner->pid = pid;
        close(runner->pipe_f_to_r[0]);  //close pipes belonging to runner
        close(runner->pipe_r_to_f[1]);
        return 0;
    }
    //CHILD (runner process)
    fzl_runner_init(runner->id, "Runner child process forked"); //fzl event log
    close(runner->pipe_f_to_r[1]);  //close pipes belonging to fuzzer
    close(runner->pipe_r_to_f[0]);

    if(dup2(runner->shm_fdesc, COVERAGE_MAP_FD) == -1){
        perror("Failure in dup2()");
        exit(EXIT_FAILURE);
    }
    close(runner->shm_fdesc);   //close og after dup

    sigset_t block_smask;
    sigset_t empty_smask;
    sigemptyset(&block_smask);
    sigemptyset(&empty_smask);
    sigaddset(&block_smask, SIGTERM);
    sigaddset(&block_smask, SIGINT);
    sigaddset(&block_smask, SIGHUP);
    sigaddset(&block_smask, SIGALRM);
    sigaddset(&block_smask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_smask, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = helpr_runner_sighandler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int target_argc = 0;
    while(args[target_argc] != NULL){
        target_argc++;
    }
    char** target_args = malloc((target_argc + 2) * sizeof(char*));

    while(!sigterm_seen){
        char* inp_str = runner_receive_fuzzer_input(runner);
        if(!inp_str || sigterm_seen){
            SAFEFREE(inp_str);
            break;
        }

        target_args[0] = cmd;

        for(int i = 0; i < target_argc; i++){
            if(strcmp(args[i], PROGRAM_ARGUMENT_PLACEHOLDER) == 0){
                target_args[i + 1] = inp_str;
            }
            else{
                target_args[i + 1] = args[i];
            }
        }
        target_args[target_argc + 1] = NULL;

        pid_t target_pid = fork();
        if(target_pid == 0){    //CHILD CHILD
            sigprocmask(SIG_SETMASK, &empty_smask, NULL);

            int null_fdesc = open("/dev/null", O_RDWR);
            if(null_fdesc != -1){
                dup2(null_fdesc, STDIN_FILENO);
                dup2(null_fdesc, STDOUT_FILENO);
                close(null_fdesc);
            }

            fzl_runner_launch(runner->id, target_args, "Executing target"); //fzl event log
            execvp(cmd, target_args);
            exit(EXIT_FAILURE); //only happens if execvp goes bad
        }
        //PARENT (runner)
        current_target_pid = target_pid;
        sigchld_seen = 0;
        sigalrm_seen = 0;

        alarm(timeout);
        while(!sigchld_seen && !sigterm_seen){
            sigsuspend(&empty_smask);
        }
        alarm(0);

        if(sigchld_seen){
            int wait_status;
            waitpid(target_pid, &wait_status, 0);
            current_target_pid = -1;    //target reaped

            RUNNER_STATE final_state = NO_STATE;
            int final_data = 0;
            if(WIFEXITED(wait_status)){
                final_state = VALID;
                final_data = WEXITSTATUS(wait_status);
            }
            else if(WIFSIGNALED(wait_status)){
                int term_sig = WTERMSIG(wait_status);
                if(sigalrm_seen && term_sig == SIGALRM){
                    final_state = TIMEOUT;
                }
                else{
                    final_state = CRASH;
                    final_data = term_sig;
                }
            }
            runner_alert_fuzzer(runner, final_state, final_data);
        }
        SAFEFREE(inp_str);
    }
    SAFEFREE(target_args);

    fzl_runner_fini(runner->id, "Runner process exiting");  //fzl event log
    exit(EXIT_SUCCESS);
}

struct runners{
    int total_jobs;

    RUNNER* pool;
    int pool_count;

    RUNNER* ready;
    RUNNER* active;
    RUNNER* done;
    int ready_count;
    int active_count;
    int done_count;
};

static void helpr_remove_runner(RUNNER* arr, int* count, RUNNER victim){
    for(int i = 0; i < *count; i++){
        if(arr[i] == victim){
            for(int j = i + 1; j < *count; j++){
                arr[j - 1] = arr[j];
            }
            (*count)--;
            return;
        }
    }
}

RUNNERS runners_init(int job_count) {
    RUNNERS runners = calloc(1, sizeof(struct runners));
    if(!runners){
        return NULL;
    }

    runners->total_jobs = job_count;
    runners->pool = calloc(job_count, sizeof(RUNNER));
    runners->ready = calloc(job_count, sizeof(RUNNER));
    runners->active= calloc(job_count, sizeof(RUNNER));
    runners->done = calloc(job_count, sizeof(RUNNER));

    for(int i = 0; i < job_count; i++){
        RUNNER runner = runner_init();
        if(runner){
            runner_launch(runner);
            runners->pool[runners->pool_count++] = runner;
            runners->ready[runners->ready_count++] = runner;
        }
    }
    return runners;
}

void runners_fini(RUNNERS runners) {
    if(!runners){
        return;
    }

    for(int i = 0; i < runners->pool_count; i++){
        runner_fini((runners->pool)[i]);
    }

    SAFEFREE(runners->pool);
    SAFEFREE(runners->ready);
    SAFEFREE(runners->active);
    SAFEFREE(runners->done);
    SAFEFREE(runners);
}

int runners_submit_input(RUNNERS runners, INPUT input) {
    if(!runners || runners->ready_count == 0){
        return -1;
    }

    RUNNER runner = (runners->ready)[0];
    helpr_remove_runner(runners->ready, &(runners->ready_count), runner);

    if(fuzzer_send_runner_input(runner, input) == -1){
        return -1;
    }

    runners->active[runners->active_count++] = runner;
    return 0;
}

int runners_has_jobs(RUNNERS runners) {
    if(!runners){
        return 0;
    }
    return runners->pool_count > 0;
}

int runners_has_active_jobs(RUNNERS runners) {
    if(!runners){
        return 0;
    }
    return runners->active_count > 0;
}

int runners_has_done_jobs(RUNNERS runners) {
    if(!runners){
        return 0;
    }
    return runners->done_count > 0;
}

int runners_has_ready_jobs(RUNNERS runners) {
    if(!runners){
        return 0;
    }
    return runners->ready_count > 0;
}

void runners_check_if_jobs_done(RUNNERS runners) {
    if(!runners || runners->active_count == 0){
        return;
    }

    fd_set read_fdescs;
    FD_ZERO(&read_fdescs);

    int max_fdesc = -1;
    for(int i = 0; i < runners->active_count; i++){
        int fd = (runners->active)[i]->pipe_r_to_f[0];
        FD_SET(fd, &read_fdescs);
        max_fdesc = MAX(max_fdesc, fd);
    }

    struct timeval tv = {0, 0};
    int ready_count = select(max_fdesc + 1, &read_fdescs, NULL, NULL, &tv);
    if(ready_count > 0){
        int act_ind = 0;
        while(act_ind < runners->active_count){
            RUNNER runner = (runners->active)[act_ind];

            if(FD_ISSET(runner->pipe_r_to_f[0], &read_fdescs)){
                helpr_remove_runner(runners->active, &(runners->active_count), runner);
                runners->done[runners->done_count++] = runner;
            }
            else{
                act_ind++;
            }
        }
    }
}

RUNNER runners_process_result(RUNNERS runners, RUNNER_STATE *state, int *data) {
    if(!runners || runners->done_count == 0){
        return NULL;
    }

    RUNNER runner = (runners->done)[0];
    helpr_remove_runner(runners->done, &(runners->done_count), runner);

    *state = fuzzer_attempt_receive_status(runner, data);

    runners->ready[runners->ready_count++] = runner;

    fzl_received_status(runner->id, *state, *data, "Fuzzer received status packet");    //fzl event log
    return runner;
}

int runners_reap(RUNNERS runners) {
    if(!runners){
        return -1;
    }
    
    int ret = 0;
    int i = 0;
    while(i < runners->pool_count){
        RUNNER r = (runners->pool)[i];
        int wait_status;

        pid_t res = waitpid(r->pid, &wait_status, WNOHANG);
        if(res > 0){
            helpr_remove_runner(runners->ready, &(runners->ready_count), r);
            helpr_remove_runner(runners->active, &(runners->active_count), r);
            helpr_remove_runner(runners->done, &(runners->done_count), r);
            helpr_remove_runner(runners->pool, &(runners->pool_count), r);

            r->pid = -1;
            runner_fini(r);
        }
        else if(res == -1){   //unsuccessful reap
            perror("Failure in waitpid()");
            ret = -1;
            i++;
        }
        else{
            i++;
        }
    }
    return ret;
}
