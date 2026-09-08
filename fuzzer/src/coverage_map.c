#include "coverage_map.h"
#include "global.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>

#define MAP_BYTES (COVERAGE_MAP_SIZE / 8)
#define TABLE_SIZE 65537    //large prime
#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

typedef struct hash_node{    //need chained hash table
    char coverage_data[MAP_BYTES];
    struct hash_node* next;
} HashNode;

struct coverage_map{
    char bitmap[MAP_BYTES];
    HashNode* hashset[TABLE_SIZE];
};

static uint64_t helpr_hash(char* data, size_t len){ //fnv-1a hasher
    uint64_t res = FNV_OFFSET;
    for(int i = 0; i < len; i++){
        res ^= (unsigned char)data[i];
        res *= FNV_PRIME;
    }
    return res;
}

COVERAGE_MAP coverage_map_init() {
    COVERAGE_MAP map = calloc(1, sizeof(struct coverage_map));
    if(!map){
        return NULL;
    }
    return map;
}

void coverage_map_fini(COVERAGE_MAP map) {
    if(!map){
        return;
    }
    for(int i = 0; i < TABLE_SIZE; i++){
        HashNode* cur = (map->hashset)[i];
        while(cur){ //need to free entire chain
            HashNode* tmp = cur;
            cur = cur->next;
            SAFEFREE(tmp);
        }
    }
    SAFEFREE(map);
}

COVERAGE_PRIORITY coverage_map_add(COVERAGE_MAP map, char *cov_data) {
    if(!map || !cov_data){
        return COV_NO_PRIO;
    }
    int found_new_edge = 0;
    int found_new_path = 1;

    for(int i = 0; i < MAP_BYTES; i++){
        int diff = (map->bitmap[i] | cov_data[i]) != map->bitmap[i];
        if(diff){   //diff means new exec edge is found
            found_new_edge = 1;
        }
        map->bitmap[i] |= cov_data[i];
    }

    uint64_t hash = helpr_hash(cov_data, MAP_BYTES);
    size_t hash_ind = hash % TABLE_SIZE;

    HashNode* cur = map->hashset[hash_ind];
    while(cur){
        int alr_exists = memcmp(cur->coverage_data, cov_data, MAP_BYTES) == 0;
        if(alr_exists){
            found_new_path = 0;
            break;
        }
        cur = cur->next;
    }

    if(found_new_path){
        HashNode* new_node = malloc(sizeof(HashNode));
        if(!new_node){
            return COV_NO_PRIO;
        }
        memcpy(new_node->coverage_data, cov_data, MAP_BYTES);
        new_node->next = map->hashset[hash_ind];
        map->hashset[hash_ind] = new_node;
    }

    if(found_new_edge){
        return COV_HIGH_PRIO;
    }
    else if(found_new_path){
        return COV_LOW_PRIO;
    }
    else{
        return COV_NO_PRIO;
    }
}
