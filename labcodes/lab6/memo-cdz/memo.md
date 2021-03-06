[toc]

# 练习0:填写已有实验
已完成,需要对部分前面的代码进行修改(update),详见proc.c、trap.c

# 练习1:使用Round Robin(轮询)调度算法
## 分析Round Robin算法
要求概述:请理解并分析sched_class中各个函数指针的用法,并结合Round Robin 调度算法描ucore的调度执行过程  

- **sched_class如下**(具体的实例可参考default_sched.c):  
```
struct sched_class {
    // 调度器名字
    const char *name;                                             
    // 初始化,主要是就绪队列的初始化
    void (*init)(struct run_queue *rq);
    // 进程加入就绪队列
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // 进程从就绪队列出队
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // 从就绪队列选出下一个进程
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // 更新时间信息,比如时间到减少proc的时间片
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
};
```

- **调度执行过程分析**  
函数调用关系:  
```
初始化:kern_init() -> sched_init()(初始化调度类sched_class!)

调度:trap(...) -> schedule() -> sched_class的相关入队出队函数;proc_run()

时间片更新:trap_dispatch() -> sched_class_proc_tick(...)(每次tick就将当前执行进程的时间片减1)
```

- **分析:进程与就绪队列是如何联系起来的?**  
proc_struct结构如下:  
```
struct proc_struct {
    ......
    // 为进程调度补充的成员
    struct run_queue *rq;                       // running queue contains Process
    list_entry_t run_link;                      // the entry linked in run queue    => 将这个进程与就绪队列连接起来
    int time_slice;                             // time slice for occupying the CPU => 时间片
    skew_heap_entry_t lab6_run_pool;            // FOR LAB6 ONLY: the entry in the run pool
    uint32_t lab6_stride;                       // FOR LAB6 ONLY: the current stride of the process 
    uint32_t lab6_priority;                     // FOR LAB6 ONLY: the priority of process, set by lab6_set_priority(uint32_t)
};
```
run_queue结构如下(ucore中,所有就绪进程都在一个rq中):
```
struct run_queue {
    list_entry_t run_list;
    unsigned int proc_num;
    int max_time_slice;
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool;
};
```
进程的run_link成员就是进程与就绪队列的连接点,进程加入就绪队列其实就是将进程的run_link成员加入就绪队列的队列run_list,比如RR算法的入队:
```
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq->run_list), &(proc->run_link));
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;
    rq->proc_num ++;
}
```

## 多级反馈队列设计
暂略,待完成...

# 练习2:实现Stride Scheduling调度算法
已完成,详见default_sched_stride.c


# 补充:进程调度概述
## 实验执行流程
lab6专门需要针对处理器调度框架和各种算法进行设计与实现,为此对ucore的调度部分进行了适当的修改,使得kern/schedule/sched.c 只实现调度器框架,而不再涉及具体的调度算法实现.而调度算法在单独的文件(default_sched.[ch])中实现.  

除此之外,实验中还涉及了`idle进程`的概念.当cpu没有进程可以执行的时候,系统应该如何工作?在实验五的scheduler实现中,ucore内核不断的遍历进程池,直到找到第一个runnable状态的 process,调用并执行它.也就是说,当系统没有进程可以执行的时候,它会把所有cpu时间用在搜索进程池,以实现 idle的目的.但是这样的设计不被大多数操作系统所采用,原因在于它将进程调度和idle进程两种不同的概念混在了一起,而且,当调度器比较复杂时,schedule函数本身也会比较复杂,这样的设计结构很不清晰而且难免会出现错误.所以在此次实验中,ucore建立了一个单独的进程(kern/process/proc.c 中的 idleproc)作为cpu空闲时的 idle进程,这个程序是通常一个死循环.你需要了解这个程序的实现.  

执行过程:在init.c中的kern_init函数增加了对sched_init函数的调用.sched_init函数主要完成了对实现特定调度算法的调度类(sched_class)的绑定,使得ucore在后续的执行中,能够通过调度框架找到实现特定调度算法的调度类并完成进程调度相关工作  

## 进程状态
在ucore中,runnable(就绪)的进程会被放在运行队列中.在具体实现中,ucore定义的进程控制块struct proc_struct包含了成员变量state,用于描述进程的运行状态,而running和runnable共享同一个状态(state)值(PROC_RUNNABLE,不同之处在于处于running态的进程不会放在运行队列中.进程的正常生命周期如下:  
- 进程首先在cpu初始化或者sys_fork的时候被创建,当为该进程分配了一个进程控制块之后,该进程进入 uninit态(在proc.c 中alloc_proc)  
- 当进程完全完成初始化之后,该进程转为runnable态.  
- 当到达调度点时,由调度器sched_class根据运行队列rq的内容来判断一个进程是否应该被运行,即把处于runnable态的进程转换成running状态.从而占用CPU执行.  
- running态的进程通过wait等系统调用被阻塞,进入sleeping态.  
- sleeping态的进程被wakeup变成runnable态的进程.  
- running态的进程主动exit变成zombie态,然后由其父进程完成对其资源的最后释放,子进程的进程控制块成为unused.

## 进程调度实现
### 内核抢占点
调度本质上体现了对CPU资源的抢占.对于用户进程而言,`由于有中断的产生,可以随时打断用户进程的执行,转到操作系统内部,从而给了操作系统以调度控制权`,让操作系统可以根据具体情况(比如用户进程时间片已经用完了)选择其他用户进程执行.这体现了用户进程的可抢占性(preemptive).但如果把ucore操作系统也看成是一个特殊的内核进程或多个内核线程的集合,那ucore是否也是可抢占的呢?其实`ucore内核执行是不可抢占的`(non-preemptive),即在执行"任意"内核代码时,CPU控制权不可被强制剥夺.这里需要注意,`不是在所有情况下ucore内核执行都是不可抢占的`,有以下几种情况是例外:  
```
【内核执行可被剥夺的情况】
1.进行同步互斥操作,比如争抢一个信号量、锁(lab7中会详细分析);  
2.进行磁盘读写等耗时的异步操作,由于等待完成的耗时太长,ucore会调用shcedule让其他就绪进程执行.
```
上述两种情况其实都是由于当前内核进程所需的某个`资源(也可称为事件)`无法得到满足,无法继续执行下去,从而不得不主动放弃对CPU的控制权.如果参照用户进程任何位置都可被内核打断并放弃CPU控制权的情况,`这些在内核中放弃CPU控制权的执行地点是"固定"而不是"任意"的,不能体现内核任意位置都可抢占性的特点`.  
我们搜寻一下实验五的代码,可发现在如下几处地方调用了shedule函数:  
|  编号   | 位置  | 原因 |
|  ----  | ----  | ---- |
|  1     | proc.c::do_exit  | 用户线程执行结束,主动放弃CPU控制权 |
|  2     | proc.c::do_wait  | 用户线程等待子进程结束,主动放弃CPU控制权.|
|  3     | proc.c::init_main| a. initproc内核线程等待所有用户进程结束,如果没有结束,就主动放弃CPU控制权; b. initproc内核线程在所有用户进程结束后,让kswapd内核线程执行10次,用于回收空闲内存资源 |
|  4     | proc.c::cpu_idle | idleproc内核线程的工作就是等待有处于就绪态的进程或线程,如果有就调用schedule函数|
|  5     | sync.h::lock	    | 在获取锁的过程中,如果无法得到锁,则主动放弃CPU控制权 |
|  6     | trap.c::trap     | 如果当前线程在用户态被打断,且当前线程控制块的成员变量need_resched设置为1,则当前线程会放弃CPU控制权|  
第1、2、5处的执行位置体现了由于获取某种资源一时等不到满足、进程要退出、进程要睡眠等原因而不得不主动放弃CPU.  
第3、4处的执行位置比较特殊,initproc内核线程等待用户进程结束而执行schedule函数;idle内核线程在没有进程处于就绪态时才执行,一旦有了就绪态的进程,它将执行schedule函数完成进程调度.  
第6处的位置比较特殊:
```
if (!in_kernel) {
    ……

    if (current->need_resched) {
        schedule();
    }
}
```
这里表明了只有当进程在用户态执行到"任意"某处用户代码位置时发生了中断,且当前进程控制块成员变量need_resched为1(表示需要调度了)时,才会执行shedule函数.这实际上体现了对用户进程的可抢占性.`如果没有第一行的if语句,那么就可以体现对内核代码的可抢占性.但如果要把这一行if语句去掉,我们就不得不实现对ucore中的所有全局变量的互斥访问操作,以防止所谓的racecondition现象,这样ucore的实现复杂度会增加不少`.  

### 进程切换过程
假定有两个用户进程,在二者进行进程切换的过程中,具体的步骤如下:  
首先在执行某进程A的用户代码时,出现了一个 trap (例如是一个外设产生的中断),这个时候就会从进程A的用户态切换到内核态(`过程(1)`),并且保存好进程A的trapframe;当内核态处理中断时发现需要进行进程切换时,ucore要通过schedule函数选择下一个将占用CPU执行的进程(即进程B),然后会调用proc_run函数,proc_run函数进一步调用switch_to函数,切换到进程B的内核态(`过程(2)`),继续进程B上一次在内核态的操作,并通过iret指令,最终将执行权转交给进程B的用户空间(`过程(3)`).  

当进程B由于某种原因发生中断之后(`过程(4)`),会从进程B的用户态切换到内核态,并且保存好进程B的trapframe;当内核态处理中断时发现需要进行进程切换时,即需要切换到进程A,ucore再次切换到进程A(`过程(5)`),会执行进程A上一次在内核调用schedule(具体还要跟踪到 switch_to 函数)函数返回后的下一行代码,这行代码当然还是在进程A的上一次中断处理流程中.最后当进程A的中断处理完毕的时候,执行权又会反交给进程A的用户代码(`过程(6)`).  
注意事项:
```
a) 需要透彻理解在进程切换以后,程序是从哪里开始执行的?需要注意到虽然指令还是同一个cpu上执行,但是此时已经是另外一个进程在执行了,且使用的资源已经完全不同了.
b) 内核在第一个程序运行的时候,需要进行哪些操作?有了实验四和实验五的经验,可以确定,内核启动第一个用户进程的过程,实际上是从进程启动时的内核状态切换到该用户进程的内核状态的过程,而且该用户进程在用户态的起始入口应该是forkret.
c) 进程切换始终是在内核态进行的(从A的内核态切换到B的内核态!!!)
```


# 补充:lab6进程调度实现细节
## 设计思路与数据结构
- **设计思路**  
实行一个进程调度策略,到底需要实现哪些基本功能对应的数据结构?首先考虑到一个无论哪种调度算法都需要选择一个就绪进程来占用CPU运行.为此我们可把就绪进程组织起来,可用队列(双向链表)、二叉树、红黑树、数组…等不同的组织方式.  

在操作方面,如果需要选择一个就绪进程,就可以从基于某种组织方式的就绪进程集合中选择出一个进程执行.需要注意,这里"选择"和"出"是两个操作,`选择是在集合中挑选一个"合适"的进程,"出"意味着离开就绪进程集合`.另外考虑到一个处于运行态的进程还会由于某种原因(比如时间片用完了)回到就绪态而不能继续占用CPU执行,这就会重新进入到就绪进程集合中.这两种情况就形成了调度器相关的三个基本操作:在就绪进程集合中选择、进入就绪进程集合和离开就绪进程集合.这三个操作属于调度器的基本操作.  

在进程的执行过程中,就绪进程的等待时间和执行进程的执行时间是影响调度选择的重要因素,这两个因素随着时间的流逝和各种事件的发生在不停地变化,比如处于就绪态的进程等待调度的时间在增长,处于运行态的进程所消耗的时间片在减少等.这些进程状态变化的情况需要及时让进程调度器知道,便于选择更合适的进程执行.所以这种进程变化的情况就形成了调度器相关的一个变化感知操作:`timer时间事件感知操作`.这样在进程运行或等待的过程中,调度器可以调整进程控制块中与进程调度相关的属性值(比如消耗的时间片、进程优先级等),并可能导致对进程组织形式的调整(比如以时间片大小的顺序来重排双向链表等),并最终可能导致调选择新的进程占用CPU运行.这个操作属于调度器的进程调度属性调整操作.  


- **数据结构**  
1.通常的操作系统中,进程池是很大的(虽然在ucore中,MAX_PROCESS很小).在ucore中,调度器引入run-queue(简称rq,即运行队列 => 实际上是就绪队列)的概念,通过链表结构管理进程.  
2.`由于目前ucore设计运行在单CPU上,其内部只有一个全局的就绪队列,用来管理系统内全部的进程`.  
3.就绪队列通过链表的形式进行组织.链表的每一个节点是一个list_entry_t,每个list_entry_t又对应到了struct proc_struct *,这其间的转换是通过宏le2proc来完成的.  
4.为了保证调度器接口的通用性,ucore调度框架定义了如下接口(见sched.h),该接口中,几乎全部成员变量均为函数指针.  
```
struct sched_class {
    // the name of sched_class
    const char *name;
    // Init the run queue
    void (*init)(struct run_queue *rq);
    // put the proc into runqueue, and this function must be called with rq_lock
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // get the proc out runqueue, and this function must be called with rq_lock
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // choose the next runnable task
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // dealer of the time-tick => 实际上就是减少时间片
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
};
```
proc.h中的struct proc_struct 中也记录了一些调度相关的信息:
```
struct proc_struct {
    // . . .
    // 该进程是否需要调度(从而释放该进程占用的cpu),只对当前进程有效
    volatile bool need_resched;
    // 该进程的调度链表结构,该结构内部的连接组成了 就绪队列 列表
    list_entry_t run_link;
    // 该进程剩余的时间片,只对当前进程有效
    int time_slice;

    // round-robin 调度器并不会用到以下成员
    // 该进程在优先队列中的节点,仅在 LAB6 使用
    skew_heap_entry_t  lab6_run_pool;
    // 该进程的调度优先级,仅在LAB6使用
    uint32_t lab6_priority;
    // 该进程的调度步进值,仅在LAB6使用
    uint32_t lab6_stride;
};
```
通过数据结构struct run_queue来描述完整的run_queue(就绪队列).在ucore框架中,运行队列存储的是当前可以调度的进程,所以,只有状态为runnable的进程才能够进入运行队列.当前正在运行的进程并不会在运行队列中(这里所说的运行队列就是就绪队列!).它的主要结构如下:
```
struct run_queue {
    //其运行队列的哨兵结构,可以看作是队列头和尾
    list_entry_t run_list;
    //优先队列形式的进程容器，只在 LAB6 中使用
    skew_heap_entry_t  *lab6_run_pool;
    //表示其内部的进程总数
    unsigned int proc_num;
    //每个进程一轮占用的最多时间片
    int max_time_slice;
};
```

- **调度点的相关关键函数**  
虽然进程各种状态变化的原因和导致的调度处理各异,但其实仔细观察各个流程的共性部分,会发现其中`只涉及了三个关键调度相关函数:wakup_proc、shedule、run_timer_list`.  
`wakeup_proc`函数其实完成了把一个就绪进程放入到就绪进程队列中的工作,为此还调用了一个调度类接口函数sched_class_enqueue,这使得wakeup_proc的实现与具体调度算法无关; `schedule`函数完成了与调度框架和调度算法相关三件事情:把当前继续占用CPU执行的运行进程放放入到就绪进程队列中,从就绪进程队列中选择一个"合适"就绪进程,把这个"合适"的就绪进程从就绪进程队列中摘除.通过调用三个调度类接口函数sched_class_enqueue、sched_class_pick_next、sched_class_enqueue来使得完成这三件事情与具体的调度算法无关; `run_timer_list`函数在每次timer中断处理过程中被调用,从而可用来调用调度算法所需的timer时间事件感知操作,调整相关进程的进程调度相关的属性值.通过调用调度类接口函数sched_class_proc_tick使得此操作与具体调度算法无关.  

## RR调度算法实现
RR调度算法的调度思想:让所有runnable态的进程分时轮流使用CPU时间.RR调度器维护当前runnable进程的有序运行队列.当前进程的时间片用完之后,调度器将当前进程放置到运行队列的尾部,再从其头部取出进程进行调度.RR调度算法的就绪队列在组织结构上也是一个双向链表,只是增加了一个成员变量,表明在此就绪进程队列中的最大执行时间片.而且在进程控制块proc_struct中增加了一个成员变量time_slice,用来记录进程当前的可运行时间片段.在每个timer到时的时候,操作系统会递减当前执行进程的time_slice,当time_slice为0时,就意味着这个进程运行了一段时间(这个时间片段称为进程的时间片),需要把CPU让给其他进程执行,于是操作系统就需要让此进程重新回到rq的队列尾,且重置此进程的时间片为就绪队列的成员变量最大时间片max_time_slice值,然后再从rq的队列头取出一个新的进程执行.  
RR_enqueue的函数实现详见default_sched.c....


## Stride Scheduling 调度算法实现
### 基本思路
考察round-robin调度器,在假设所有进程都充分使用了其拥有的CPU时间资源的情况下,所有进程得到的CPU时间应该是相等的.但是有时候我们希望调度器能够更智能地为每个进程分配合理的CPU资源.假设我们为不同的进程分配不同的优先级,则我们有可能希望`每个进程得到的时间资源与他们的优先级成正比关系`(通过增加调度次数实现).Stride调度是基于这种想法的一个较为典型和简单的算法.  
- **可控制性**  
可以证明 Stride Scheduling对进程的调度次数正比于其优先级  

- **调度过程及其确定性**  
在不考虑计时器事件的情况下,整个调度机制都是可预知和重现的.该算法的基本思想可以考虑如下:  
```
1.为每个runnable的进程设置一个当前状态`stride,表示该进程当前的调度权`.另外定义其对应的pass值,表示对应进程在调度后,stride 需要进行的累加值.  
2.每次需要调度时,从当前runnable态的进程中选择stride最小的进程调度.  
3.对于获得调度的进程P,将对应的stride加上其对应的步长pass(只与进程的优先权有关系).  
4.在一段固定的时间之后,回到2.步骤,重新调度当前stride最小的进程.  
```
可以证明,如果令 P.pass =BigStride / P.priority 其中P.priority表示进程的优先权(大于 1),而BigStride表示一个预先定义的大常数,则该调度方案为每个进程分配的时间将与其优先级成正比.  

- **stride溢出问题**  
在之前的实现里面我们并没有考虑stride的数值范围,而这个值在理论上是不断增加的,在stride溢出以后,基于stride的比较可能会出现错误.比如假设当前存在两个进程A和B,stride属性采用16位无符号整数进行存储.当前队列中元素如下(假设当前运行的进程已经被重新放置进运行队列中):  
![...](./memo-pic/overflow1.png)
此时应该选择 A 作为调度的进程,而在一轮调度后，队列将如下:
![...](./memo-pic/overflow2.png)
可以看到由于溢出的出现,进程间stride的理论比较和实际比较结果出现了偏差.我们首先在理论上分析这个问题:令PASS_MAX为当前所有进程里最大的步进值.则我们可以证明如下结论:对每次Stride调度器的调度步骤中,有其最大的步进值STRIDE_MAX和最小的步进值STRIDE_MIN之差:STRIDE_MAX – STRIDE_MIN <= PASS_MAX  
提问 1：如何证明该结论?  
有了该结论,在加上之前对优先级有Priority > 1限制,我们有STRIDE_MAX – STRIDE_MIN <= BIG_STRIDE,`于是我们只要将BigStride取在某个范围之内,即可保证对于任意两个Stride之差都会在机器整数表示的范围之内`.而我们可以通过其与0的比较结果,来得到两个Stride的大小关系.在上例中,虽然在直接的数值表示上 98 < 65535,但是 98 - 65535 的结果用带符号的 16位整数表示的结果为99,与理论值之差相等.所以在这个意义下 98 > 65535.基于这种特殊考虑的比较方法,即便Stride有可能溢出,我们仍能够得到理论上的当前最小Stride,并做出正确的调度决定.  
提问 2:在 ucore 中,目前Stride是采用无符号的32位整数表示.则BigStride应该取多少,才能保证比较的正确性?


### 使用优先队列实现Stride Scheduling
在上述的实现描述中,对于每一次pick_next函数,我们都需要完整地扫描来获得当前`最小的stride及其进程`.这在进程非常多的时候是非常耗时和低效的.考虑到其调度选择于优先队列的抽象逻辑一致,考虑使用优化的优先队列数据结构实现该调度.  
优先队列是这样一种数据结构:使用者可以快速的插入和删除队列中的元素,并且在预先指定的顺序下快速取得当前在队列中的最小(或者最大)值及其对应元素.  
本次实验提供了libs/skew_heap.h 作为优先队列的一个实现,该实现定义相关的结构和接口,其中主要包括:  
```
// 优先队列节点的结构
typedef struct skew_heap_entry  skew_heap_entry_t;
// 初始化一个队列节点
void skew_heap_init(skew_heap_entry_t *a);
// 将节点 b 插入至以节点 a 为队列头的队列中去，返回插入后的队列
skew_heap_entry_t  *skew_heap_insert(skew_heap_entry_t  *a,
                                    skew_heap_entry_t  *b,
                                    compare_f comp);
// 将节点 b 插入从以节点 a 为队列头的队列中删除，返回删除后的队列
    skew_heap_entry_t  *skew_heap_remove(skew_heap_entry_t  *a,
                                        skew_heap_entry_t  *b,
                                        compare_f comp);
```
其中优先队列的顺序是由比较函数comp决定的,sched_stride.c中提供了proc_stride_comp_f比较器用来比较两个stride的大小,你可以直接使用它.当使用优先队列作为Stride调度器的实现方式之后,运行队列结构也需要作相关改变,其中包括:  
1.struct run_queue中的lab6_run_pool指针,在使用优先队列的实现中表示当前优先队列的头元素,如果优先队列为空,则其指向空指针(NULL).  
2.struct proc_struct中的lab6_run_pool结构,表示当前进程对应的优先队列节点.本次实验已经修改了系统相关部分的代码,使得其能够很好地适应LAB6新加入的数据结构和接口.而在实验中我们需要做的是用优先队列实现一个正确和高效的Stride调度器,如果用较简略的伪代码描述,则有:  
```
init(rq):
    Initialize rq->run_list
    Set rq->lab6_run_pool to NULL
    Set rq->proc_num to 0

enqueue(rq, proc)
    Initialize proc->time_slice
    Insert proc->lab6_run_pool into rq->lab6_run_pool
    rq->proc_num ++

dequeue(rq, proc)
    Remove proc->lab6_run_pool from rq->lab6_run_pool
    rq->proc_num --

pick_next(rq)
    If rq->lab6_run_pool == NULL, return NULL
    Find the proc corresponding to the pointer rq->lab6_run_pool
    proc->lab6_stride += BIG_STRIDE / proc->lab6_priority
    Return proc

proc_tick(rq, proc):
    If proc->time_slice > 0, proc->time_slice --
    If proc->time_slice == 0, set the flag proc->need_resched
```

# 补充:斜堆实现优先队列(stride算法)
??待完成


# 相关要点/问题速览
- **斜堆实现优先队列**  
libs/skew_heap.h,待学习!!


- **如何理解stride溢出的解决方法?**  
先参考上文:"stride溢出问题"  
根据proc.h,lab6_stride使用无符号数表示.即使无符号数溢出(以16位为例,65534+100 => 溢出,结果为无符号数98),也没有关系,因为比较大小时,使用符号数表示差(比如溢出后的和98与65535比较大小,98-65535 => 符号数99>0 => 仍然认为前者大于后者);所以`关键是要让这个差值(符号数)不溢出`,这就是BigStride的取值依据!  
数的二进制表示知识可参考csapp第二章!

- **如何理解local_intr_save(schedule.c中schedule()函数内使用)?**  
目的是关中断,从而实现同步!


- **ucore内核线程能否被抢占?**  
通常`ucore内核执行是不可抢占的`(non-preemptive),即在执行"任意"内核代码时,CPU控制权不可被强制剥夺;  
这里需要注意,`不是在所有情况下ucore内核执行都是不可抢占的`,有以下几种情况是例外:  
```
【内核执行可被剥夺的情况】
1.进行同步互斥操作,比如争抢一个信号量、锁(lab7中会详细分析);  
2.进行磁盘读写等耗时的异步操作,由于等待完成的耗时太长,ucore会调用shcedule让其他就绪进程执行. => 注意内核进行读写时是异步操作,因为他使用缓冲区,详见第8章
```  
上述两种情况其实都是由于当前内核进程所需的某个`资源(也可称为事件)`无法得到满足,无法继续执行下去,从而不得不主动放弃对CPU的控制权.如果参照用户进程任何位置都可被内核打断并放弃CPU控制权的情况,`这些在内核中放弃CPU控制权的执行地点是"固定"而不是"任意"的,不能体现内核任意位置都可抢占性的特点`.  

- **进程切换的过程是怎样的?**  
假定有两个用户进程,在二者进行进程切换的过程中,具体的步骤如下:  
首先在执行某进程A的用户代码时,出现了一个 trap (例如是一个外设产生的中断),这个时候就会从进程A的用户态切换到内核态(`过程(1)`),并且保存好进程A的trapframe;当内核态处理中断时发现需要进行进程切换时,ucore要通过schedule函数选择下一个将占用CPU执行的进程(即进程B),然后会调用proc_run函数,proc_run函数进一步调用switch_to函数,切换到进程B的内核态(`过程(2)`),继续进程B上一次在内核态的操作,并通过iret指令,最终将执行权转交给进程B的用户空间(`过程(3)`).  
当进程B由于某种原因发生中断之后(`过程(4)`),会从进程B的用户态切换到内核态,并且保存好进程B的trapframe;当内核态处理中断时发现需要进行进程切换时,即需要切换到进程A,ucore再次切换到进程A(`过程(5)`),会执行进程A上一次在内核调用schedule(具体还要跟踪到 switch_to 函数)函数返回后的下一行代码,这行代码当然还是在进程A的上一次中断处理流程中.最后当进程A的中断处理完毕的时候,执行权又会反交给进程A的用户代码(`过程(6)`).  
注意事项:  
```
a) 需要透彻理解在进程切换以后,程序是从哪里开始执行的?需要注意到虽然指令还是同一个cpu上执行,但是此时已经是另外一个进程在执行了,且使用的资源已经完全不同了.
b) 内核在第一个程序运行的时候,需要进行哪些操作?有了实验四和实验五的经验,可以确定,内核启动第一个用户进程的过程,实际上是从进程启动时的内核状态切换到该用户进程的内核状态的过程,而且该用户进程在用户态的起始入口应该是forkret.
c) 进程切换始终是在内核态进行的(从A的内核态切换到B的内核态!!!)
```

- **就绪队列只能有一个吗?**  
由于ucore只能运行在单cpu上,因此只有一个就绪队列;  
如果是多cpu,则应该每个cpu对应一个就绪队列!  

