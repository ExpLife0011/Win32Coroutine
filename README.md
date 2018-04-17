# Win32Coroutine  
Win32下的协程实现，基于Windows的Fiber实现，额外修改了几个IO函数以实现非侵入的同步代码异步实现。    
**实现原理：**  
参考了libco，修改IAT挂钩了包括文件IO，网络IO和管道IO在内的一系列支持阻塞IO的API，在内部将其转换为完成端口调用，并通过调度协程实现非阻塞IO。 协程本身的上下文切换是借助于Windows的Fiber实现的。  
**优势**
 - 完成端口实现的异步IO。
 - 非侵入式，对以前的业务逻辑复制粘贴就可以使用。
 - 对协程以外的IO调用没有影响。
 - C语言实现，较少的Overhead。
 
**待改进**
 - 调度算法较为简单。
 - 没有支持延时调用。（事件等待和延时函数也会阻塞线程）
 - 取消IO没有处理。

**已经实现的IO函数：**  
 - ReadFile
 - WriteFile
 - DeviceIoControl
 - socket
 - accept
 - recv
 - send

 **性能比较**
 顺序阻塞读取文件和协程异步读取文件的性能对比：
 ![性能比较](performance.png)