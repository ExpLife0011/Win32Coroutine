# Win32Coroutine  
Win32下的协程实现，基于Windows的Fiber实现，额外修改了几个IO函数以实现非侵入的同步转异步。  
**实现原理：**  
参考了libco，HOOK了几个可以用IO完成端口实现异步IO的函数，将其同步调用转换为异步调用，并通过协程调度实现并发。  
**已经实现的IO函数：**  
 - ReadFile