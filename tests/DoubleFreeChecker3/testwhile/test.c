#include <stddef.h>

struct Node {
    int val;
    struct Node * next;
};
void delete_list(struct Node * list)
{
    struct Node * cur = list;
    while(cur != NULL) {
        struct Node * next = cur->next;
        free(cur);
        cur = next;
    }
}