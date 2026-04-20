#include "utils.h"
#include <ctype.h>

int is_num(char* str){
    if(!str || *str == 0){
        return 0;
    }
    int i = 0;
    while(str[i] != 0){
        if(!isdigit((unsigned char)str[i])){
            return 0;
        }
        i++;
    }
    return 1;
}