#include <stddef.h>

void f(){
    char* p = malloc(10);
    free(p);
    //p = NULL;
    p = malloc(10);
}