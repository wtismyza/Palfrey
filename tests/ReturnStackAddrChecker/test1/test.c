int *func_return_stack_addr() {
    char str[32];
    int value = 1024;
    //int* p = &value;
    return &value;
}