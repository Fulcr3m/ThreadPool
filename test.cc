#include "threadpool.h"
 #include <unistd.h>
int sum(int a,int b){
    sleep(3);
    return a+b;
}

void print(const char* a){
    std::cout<<a<<std::endl;
}

int main(){
    ThreadPool pool(3);
    pool.start();
    pool.SubmitTask(print,"szyshs");
    auto res=pool.SubmitTask(sum,10,20);
    pool.SubmitTask(sum,10,20);
    pool.SubmitTask(sum,10,20);
    pool.SubmitTask(sum,10,20);
    pool.SubmitTask(sum,10,20);
    int num=res.get();
    std::cout<<num<<std::endl;
}