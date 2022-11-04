# pragma onece
#include<unordered_map>
#include<memory>
#include<atomic>
#include<vector>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<thread>
#include<iostream>
#include<future>

#define MAX_TASK_SIZE 1024
#define MAX_THREADHOLD 1024
#define MAX_IDLE_TIME 60

enum class PoolMode{
    Fixed,
    Cached
};

class Thread{
public:
    using Func=std::function<void(int)>;
    Thread(Func func,int id): m_func(func),m_id(id) {}
    void start() {
        std::thread t(m_func,m_id);
        t.detach();
    }
private:
    Func m_func;
    int m_id;
};

class ThreadPool{
public:
    using Task=std::function<void()>;

    ThreadPool(int initnum=4,PoolMode mode=PoolMode::Cached):m_inithreadnum(initnum),m_maxtasksize(MAX_TASK_SIZE),m_maxthreadnum(MAX_THREADHOLD),
        m_curtasksize(0),
        m_idlenums(0),
        m_curthreadnum(0),
        m_stopping(false),
        m_mode(mode) {}
    

    ~ThreadPool(){
        m_stopping=true;
        std::unique_lock<std::mutex> lock(m_taskmtx);
        m_notempty.notify_all();
        m_isexit.wait(lock,[&]() ->bool {return m_threads.size()==0;});
    }

    void start(){
        m_curthreadnum=m_inithreadnum;
        for(int i=0;i<m_inithreadnum;i++)
            {
                int id=GenerateId();
                auto t_ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1),id);
                m_threads.emplace(id,std::move(t_ptr));
            }           //构造线程对象
        for(int i=0;i<m_inithreadnum;i++)
            {
                m_threads[i]->start();
                m_idlenums++;
            }
    }

    template<typename Func,typename... Args>
    auto SubmitTask(Func&& func,Args&&... args) -> std::future<decltype(func(args...))>
    {
        using RTtype=decltype(func(args...));
        auto task_ptr=std::make_shared<std::packaged_task<RTtype()> >(std::bind(
            std::forward<Func>(func),std::forward<Args>(args)...
        ));
        std::future<RTtype> result=task_ptr->get_future();
        std::unique_lock<std::mutex> lock(m_taskmtx);
        if(!m_notfull.wait_for(lock,std::chrono::seconds(1),[&]()-> bool {return m_taskque.size()<m_maxtasksize;}) )
        {
            std::cerr<<"task queue is full,submit task fail"<<std::endl;
            task_ptr=std::make_shared<std::packaged_task<RTtype()> >([]() {return RTtype();});
            result=task_ptr->get_future();
            (*task_ptr)();
            return result;
        }
        
        m_taskque.emplace([task_ptr]()-> void {(*task_ptr)();});
        m_curtasksize++;
        m_notempty.notify_all();
        if(m_mode==PoolMode::Cached && m_curtasksize>m_idlenums && m_curthreadnum<m_maxthreadnum){
            int id=GenerateId();
            std::cout<<"create new thread because task is too many"<<std::endl;
            auto thread_ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1),id);
            m_threads.emplace(id,std::move(thread_ptr));
            m_threads[id]->start();
            m_idlenums++;
            m_curthreadnum++;
        }
        return result;
    }
    
    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&) =delete;

private:
    void threadFunc(int threadid){       //需要一个实参作为key来删除容器
        while(1){
            auto lastTime=std::chrono::high_resolution_clock().now();
            Task task;
            {
                std::unique_lock<std::mutex> lock(m_taskmtx);
                std::cout<<"threadid:"<<threadid<< " 尝试获取任务..."<<std::endl;
                while(m_taskque.size()==0){
                    if(m_stopping==true){
                        m_threads.erase(threadid);
                        std::cout<<"threadid:"<<threadid<<" exit"<<std::endl;
                        m_curthreadnum--;
                        m_idlenums--;
                        m_isexit.notify_all();
                        return;
                    }
                    if(PoolMode::Cached==m_mode){
                        if(std::cv_status::timeout==m_notempty.wait_for(lock,std::chrono::seconds(1))){
                            auto now=std::chrono::high_resolution_clock().now();
                            auto dur=std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                            if(dur.count()>=MAX_IDLE_TIME && m_curthreadnum>m_inithreadnum)
                            {
                                m_threads.erase(threadid);
                                std::cout<<"threadid:"<<threadid<<" idletime is too long will exit"<<std::endl;
                                m_curthreadnum--;
                                m_idlenums--;
                                return;
                            }
                        }
                    }
                    else{
                        m_notempty.wait(lock);
                    }
                }
                task=m_taskque.front();
                std::cout<<"threadid:"<<threadid<<"获取任务成功.."<<std::endl;
                m_taskque.pop();
                m_curtasksize--;
                m_notfull.notify_all();
            }
            m_idlenums--;
            task();
            m_idlenums++;
        }
    }
    static int GenerateId(){
        static int id=0;
        return id++;
    }
private:
    std::unordered_map<int,std::unique_ptr<Thread> > m_threads;   //线程对象容器 key是线程id
    int m_inithreadnum;                //fixed的线程个数
    int m_maxtasksize;                    //任务个数的最大值
    int m_maxthreadnum;                   //线程池中线程的最大值
    std::atomic_int m_curtasksize;              //当前的任务个数
    std::atomic_int m_idlenums;                //空闲线程数量
    std::atomic_int m_curthreadnum;          //当前线程的数量
    std::queue<Task> m_taskque;     //任务队列
    std::mutex m_taskmtx;                       //保证任务队列线程安全的互斥锁
    std::condition_variable m_notfull;       //等待直到不为满的条件变量
    std::condition_variable m_notempty;      //等待直到不为空的条件变量
    std::condition_variable m_isexit;       //等待直到所有子线程都退出的条件变量
    std::atomic_bool m_stopping;           //线程池是否结束
    PoolMode m_mode;                //线程池的模式
};