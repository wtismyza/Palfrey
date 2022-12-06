
void delete_list(struct Node * list)
{
    char *buffer = new char[42];
    while(buffer) {
        char *next = new char[40];
        delete [] buffer;
        buffer = next;
    }
}
