int f(void* ptr, int size) {
    void* result;
    if (size==0)
        size=1;
    result = realloc(ptr, size);
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