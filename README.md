# 概述

用c++实现的线程池

用户可以submit任意类型的函数，支持任意类型的可变参数

可以设置cache和fixed两种模式

>经过大量测试，没有发现明显的死锁和内存泄露问题，表现良好

## 涉及的c++技术点

智能指针

右值引用

引用折叠，完美转发

可变参数模板

condition_variable

future

pacakege_task

mutex

unique_lock

thread

stl ..........

