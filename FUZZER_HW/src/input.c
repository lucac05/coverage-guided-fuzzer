#include "input.h"
#include "utils.h"
#include <string.h>

struct input{
    char* str;
    size_t len;
    MUTATOR_STATE state;
};

INPUT make_input(const char *input_str) {
    if(!input_str){
        return NULL;
    }

    INPUT new_inp = malloc(sizeof(struct input));
    if(!new_inp){
        return NULL;
    }
    new_inp->str = strdup(input_str);
    if(!new_inp->str){
        SAFEFREE(new_inp);
        return NULL;
    }

        new_inp->len = strlen(input_str);
    new_inp->state = 0;

    return new_inp;
}

void free_input(INPUT input) {
    if(!input){
        return;
    }
    if(input->str){
        SAFEFREE(input->str);
    }
    SAFEFREE(input);
}

size_t input_len(INPUT input) {
    if(!input){
        return 0;
    }
    return input->len;
}

const char *input_str(INPUT input) {
    if(!input){
        return NULL;
    }
    return input->str;
}

MUTATOR_STATE input_mutator_state(INPUT input) {
    if(!input){
        return 0;
    }
    return input->state;
}

MUTATOR_STATE input_set_state(INPUT input, MUTATOR_STATE state) {
    if(!input){
        return 0;
    }
    (void) state;
    return 0;
}

MUTATOR_STATE input_state_step(INPUT input) {
    if(!input){
        return 0;
    }
    MUTATOR_STATE old_state = input->state;
    input->state++;
    return old_state;
}
