#include <stddef.h>

int foo(int k, char * ptr) { 
 free(ptr);
 return k;
}
int bar(int k, char * ptr)
{
 int r = foo(k, ptr);
 return r;
}