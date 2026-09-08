#include "mutator.h"
#include "global.h"
#include "utils.h"
#include <string.h>

#define MAX_INP_LEN 1024
#define UNIQUE_MUTS 37

static uint64_t H(size_t n, int m, MUTATOR_STATE x){
    if(n == 0){
        return 0;
    }
    if(m == 0){
        return (uint64_t)x;
    }

    uint64_t res = (uint64_t)x;
    for(int i = 1; i <= m; i++){
        res = HASH(res) % n;
    }
    return res;
}

static void helpr_strat1(char* buf, size_t* N, int K){  //[1] Fill With Single Character To Length
    size_t target = 1;
    for(int i = 0; i < K; i++){
        target *= 2;
        if(target > MAX_INP_LEN){
            target = MAX_INP_LEN;
            break;
        }
    }

    if(target > *N){
        memset(buf + *N, 'a', target - *N);
    }
    buf[target] = 0;
    *N = target;
}

static void helpr_strat2(char* buf, size_t* N, int K){  //[2] Duplicate Input K+1 times
    size_t new_len = (*N) * (K + 1);
    new_len = MIN(new_len, MAX_INP_LEN);

    size_t written_len = *N;
    while(written_len < new_len){
        size_t copy_len = *N;
        if(written_len + copy_len > new_len){
            copy_len = new_len - written_len;
        }
        memcpy(buf + written_len, buf, copy_len);
        written_len += copy_len;
    }
    buf[new_len] = 0;
    *N = new_len;
}

static void helpr_strat3(char* buf, size_t* N, MUTATOR_STATE S, int L){ //[3] Lengthen String By L Characters
    size_t old_N = *N;
    size_t new_len = old_N + L;
    new_len = MIN(new_len, MAX_INP_LEN);

    for(size_t i = 0; i < new_len - old_N; i++){
        buf[old_N + i] = buf[H(old_N, i + 1, S)];
    }
    
    buf[new_len] = 0;
    *N = new_len;
}

static void helpr_strat4(char* buf, size_t* N, MUTATOR_STATE S){    //[4] Truncate Input
    size_t new_len = H(*N, 1, S);
    buf[new_len] = 0;
    *N = new_len;
}

static void helpr_strat5(char* buf, size_t N, MUTATOR_STATE S, int p, const char* A){   //[5] Inject String
    size_t A_len = strlen(A);
    for(int i = 1; i <= p; i++){
        size_t pos = H(N, i, S);
        size_t copy_len = A_len;

        if(pos + copy_len > N){
            copy_len = N - pos;
        }
        memcpy(buf + pos, A, copy_len);
    }
}

static void helpr_strat6(char* buf, size_t N, MUTATOR_STATE S, int p){  //[6] Inject Random Integer String
    char A[32];
    snprintf(A, sizeof(A), "%d", (int)HASH(S));
    helpr_strat5(buf, N, S, p, A);  //generate random string then inject
}

static void helpr_strat7(char* buf, size_t N, MUTATOR_STATE S, int p, int L){   //[7] Inject Random Substring of Length L
    size_t src_pos = H(N, 1, S);
    size_t actual_L = L;

    if(src_pos + actual_L > N){
        actual_L = N - src_pos;
    }

    char substr[MAX_INP_LEN];
    memcpy(substr, buf + src_pos, actual_L);

    for(int i = 1; i <= p; i++){
        size_t dest_pos = H(N, i + 1, S);
        size_t copy_len = actual_L;
        if(dest_pos + copy_len > N){
            copy_len = N - dest_pos;
        }
        memcpy(buf + dest_pos, substr, copy_len);
    }
}

static void helpr_strat8(char* buf, size_t N, MUTATOR_STATE S, int p, char C){  //[8] Inject Character
    for(int i = 1; i <= p; i++){
        buf[H(N, i, S)] = C;
    }
}

static void helpr_strat9(char* buf, size_t N, MUTATOR_STATE S, int p, int L){   //[9] Flip L Bits
    for(int i = 1; i <= p; i++){
        size_t bit_pos = H(8 * N, i, S);
        for(int j = 0; j < L; j++){
            if(bit_pos + j >= 8 * N){
                break;
            }
            
            size_t bit_toflip = bit_pos + j;
            size_t byte_ind = bit_toflip / 8;
            size_t bit_ind = bit_toflip % 8;
            buf[byte_ind] ^= (1 << bit_ind);
        }
    }
}

static void helpr_strat10(char* buf, size_t N, MUTATOR_STATE S, int p){ //[10] Increment Bytes
    for(int i = 1; i <= p; i++){
        buf[H(N, i, S)]++;
    }
}

static void helpr_strat11(char* buf, size_t N, MUTATOR_STATE S, int p){ //[11] Decrement Bytes
    for(int i = 1; i <= p; i++){
        buf[H(N, i, S)]--;
    }
}

INPUT mutate(INPUT input) {
    size_t N = input_len(input);
    MUTATOR_STATE S = input_mutator_state(input);
    input_state_step(input);

    if(N == 0){
        return make_input("");
    }
    int K = (S / UNIQUE_MUTS) + 1;
    int p = (K + 1) / 2;

    char buf[MAX_INP_LEN + 1];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, input_str(input), MAX_INP_LEN);

    int cycle_step = S % UNIQUE_MUTS;

    switch(cycle_step){
        case 0:
            helpr_strat7(buf, N, S, p, 1);
            break;
        case 1:
            helpr_strat5(buf, N, S, p, "0");
            break;
        case 2:
            helpr_strat3(buf, &N, S, 4);
            break;
        case 3:
            helpr_strat5(buf, N, S, p, "1");
            break;
        case 4:
            helpr_strat8(buf, N, S, p, '/');
            break;
        case 5:
            helpr_strat3(buf, &N, S, 7);
            break;
        case 6:
            helpr_strat9(buf, N, S, p, 1);
            break;
        case 7:
            helpr_strat11(buf, N, S, p);
            break;
        case 8:
            helpr_strat2(buf, &N, K);
            break;
        case 9:
            helpr_strat3(buf, &N, S, 1);
            break;
        case 10:
            helpr_strat5(buf, N, S, p, "-128");
            break;
        case 11:
            helpr_strat3(buf, &N, S, 8);
            break;
        case 12:
            helpr_strat5(buf, N, S, p, "2147483647");
            break;
        case 13:
            helpr_strat9(buf, N, S, p, 4);
            break;
        case 14:
            helpr_strat5(buf, N, S, p, "-1");
            break;
        case 15:
            helpr_strat3(buf, &N, S, 2);
            break;
        case 16:
            helpr_strat5(buf, N, S, p, "32767");
            break;
        case 17:
            helpr_strat1(buf, &N, K);
            break;
        case 18:
            helpr_strat7(buf, N, S, p, 2);
            break;
        case 19:
            helpr_strat8(buf, N, S, p, '.');
            break;
        case 20:
            helpr_strat3(buf, &N, S, 3);
            break;
        case 21:
            helpr_strat5(buf, N, S, p, "%p");
            break;
        case 22:
            helpr_strat4(buf, &N, S);
            break;
        case 23:
            helpr_strat6(buf, N, S, p);
            break;
        case 24:
            helpr_strat5(buf, N, S, p, "127");
            break;
        case 25:
            helpr_strat8(buf, N, S, p, ';');
            break;
        case 26:
            helpr_strat3(buf, &N, S, 5);
            break;
        case 27:
            helpr_strat5(buf, N, S, p, "-32768");
            break;
        case 28:
            helpr_strat10(buf, N, S, p);
            break;
        case 29:
            helpr_strat5(buf, N, S, p, "%s");
            break;
        case 30:
            helpr_strat7(buf, N, S, p, 8);
            break;
        case 31:
            helpr_strat9(buf, N, S, p, 8);
            break;
        case 32:
            helpr_strat5(buf, N, S, p, "-2147483648");
            break;
        case 33:
            helpr_strat8(buf, N, S, p, ',');
            break;
        case 34:
            helpr_strat3(buf, &N, S, 6);
            break;
        case 35:
            helpr_strat7(buf, N, S, p, 4);
            break;
        case 36:
            helpr_strat9(buf, N, S, p, 2);
            break;
    }

    return make_input(buf);
}