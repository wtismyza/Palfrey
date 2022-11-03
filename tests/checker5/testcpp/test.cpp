#include <vector>
int f(void* ptr, int size) {
    std::vector<int> myvec;
    if (size==0)
        size=1;
    myvec.resize(size);
}