
int f(void* ptr, int size) {
    void* result;
    if (size==2)
        size=1;
    result = malloc(size);
    int* p_integer=calloc(size, sizeof(int));
    if (result) {
        HunspellReportMemoryAllocation(result);
    }
    /*else if (size==0){
        //xxx
    }*/
    else {
        HunspellReportMemoryAllocation(ptr);
    }
}
