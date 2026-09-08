#include "input_queue.h"
#include <string.h>
#include "utils.h"
#define IS_EMPTY(_q_ptr) ((_q_ptr)->len == 0)

typedef struct node{    //singly-LL node
    INPUT input;
    struct node* next;
} Node;

typedef struct queue{   //queue of nodes
    Node* head;
    Node* tail;
    size_t len;
} Queue;

struct input_queue{ //opaque inp queue
    Queue high_p;
    Queue low_p;
    int deq_cycle_ind;
};

static void helpr_enqueue_node(Queue* queue, INPUT input){  //enq node to provided queue list
    Node* node = malloc(sizeof(Node));
    if(!node){
        return;
    }

    node->input = input;
    node->next = NULL;

    if(IS_EMPTY(queue)){
        queue->head = node;
        queue->tail = node;
    }
    else{
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->len++;
}

static int helpr_inp_is_duplicate(INPUT_QUEUE queue, INPUT input){ //check if this queue (h and l) has this input alr
    const char* target_inp = input_str(input);

    Node* cur = queue->high_p.head; //need to check both h and l
    while(cur){
        const char* cur_inp = input_str(cur->input);
        if(strcmp(target_inp, cur_inp) == 0){
            return 1;
        }
        cur = cur->next;
    }

    cur = queue->low_p.head; //need to check both h and l
    while(cur){
        const char* cur_inp = input_str(cur->input);
        if(strcmp(target_inp, cur_inp) == 0){
            return 1;
        }
        cur = cur->next;
    }

    return 0;
}

INPUT_QUEUE input_queue_init() {
    INPUT_QUEUE new_queue = malloc(sizeof(struct input_queue));
    if(!new_queue){
        return NULL;
    }

    new_queue->high_p.head = NULL;
    new_queue->high_p.tail = NULL;
    new_queue->high_p.len = 0;

    new_queue->low_p.head = NULL;
    new_queue->low_p.tail = NULL;
    new_queue->low_p.len = 0;

    new_queue->deq_cycle_ind = 0;
    return new_queue;
}

void input_queue_fini(INPUT_QUEUE queue) {
    if(!queue){
        return;
    }
    Node* cur = NULL;
    
    cur = queue->high_p.head;
    while(cur){
        Node* freeme = cur;
        cur = cur->next;

        free_input(freeme->input);
        SAFEFREE(freeme);
    }
    queue->high_p.head = NULL;
    queue->high_p.tail = NULL;
    queue->high_p.len = 0;

    cur = queue->low_p.head;
    while(cur){
        Node* freeme = cur;
        cur = cur->next;

        free_input(freeme->input);
        SAFEFREE(freeme);
    }
    queue->low_p.head = NULL;
    queue->low_p.tail = NULL;
    queue->low_p.len = 0;
    
    SAFEFREE(queue);
}

void enqueue_high_prio_input(INPUT_QUEUE queue, INPUT input) {
    if(!queue || !input){
        return;
    }

    if(helpr_inp_is_duplicate(queue, input)){   //cant have duplicate inputs
        free_input(input);  //free bad input here
        return;
    }
    helpr_enqueue_node(&(queue->high_p), input);
}

void enqueue_low_prio_input(INPUT_QUEUE queue, INPUT input) {
    if(!queue || !input){
        return;
    }

    if(helpr_inp_is_duplicate(queue, input)){   //cant have duplicate inputs
        free_input(input);  //free bad input here
        return;
    }
    helpr_enqueue_node(&(queue->low_p), input);
}

INPUT dequeue_input(INPUT_QUEUE queue) {
    if(!queue || (IS_EMPTY(&(queue->high_p)) && IS_EMPTY(&(queue->low_p)))){
        return NULL;
    }

    Queue* target_q = NULL;
    if(queue->deq_cycle_ind < 9){ //pull from high q
        target_q = &(queue->high_p);
        if(IS_EMPTY(&(queue->high_p))){
            target_q = &(queue->low_p);
        }
    }
    else{
        target_q = &(queue->low_p);
        if(IS_EMPTY(&(queue->low_p))){
            target_q = &(queue->high_p);
        }
    }
    queue->deq_cycle_ind = (queue->deq_cycle_ind + 1) % 10; //1 low per 9 high

    Node* res = target_q->head;
    target_q->head = target_q->head->next;
    target_q->len--;
    if(!target_q->head){
        target_q->tail = NULL;
    }
    res->next = NULL;

    if(IS_EMPTY(target_q)){
        target_q->head = res;
        target_q->tail = res;
    }
    else{
        target_q->tail->next = res;
        target_q->tail = res;
    }
    target_q->len++;

    return res->input;
}
