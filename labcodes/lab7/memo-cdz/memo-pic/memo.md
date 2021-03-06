[toc]

# 练习0:填写已有实验
已完成,有相应修改/更新,见:  
trap.c => trap_dispatch()函数  

# 练习1:理解内核级信号量的实现和基于内核级信号量的哲学家就餐问题
## 内核级信号量的设计与实现
- **数据结构**  
信号量的定义,包含一个值(表示资源数/等待进程数)和一个等待/阻塞队列,见sem.h:
```
typedef struct {
    int value;                  // 信号量的当前值:可用资源数
    wait_queue_t wait_queue;    // 该信号量对应的等待队列
} semaphore_t;
```
等待/阻塞队列相关的定义(对应的操作见wait.c):
```
typedef struct {
    list_entry_t wait_head;            // wait_queue的队头
} wait_queue_t;

// 等待队列上的一个节点(通过wait_link与队列联系)
// 可见,ucore实现中不止一个等待/阻塞队列; 而就绪队列只有一个!
typedef struct {
    struct proc_struct *proc;           // 等待/阻塞进程
    uint32_t wakeup_flags;              // 进程被放入等待队列的原因标记
    wait_queue_t *wait_queue;           // 此wait_t结构所属的wait_queue
    list_entry_t wait_link;             // 通过这个结点链接到对应的wait_queue
} wait_t;
```
- **信号量的操作(sem.c)**  
初始化:
```
// 初始化信号量(内部会初始化该信号量对应的等待队列)
// 如果是互斥信号量则将val初始化为1; 同步信号量则初始化为0
void sem_init(semaphore_t *sem, int value) {
    sem->value = value; 
    wait_queue_init(&(sem->wait_queue));
}
```

P操作(理解_down函数非常重要!):
```
// P操作的包装函数
void down(semaphore_t *sem) {
    uint32_t flags = __down(sem, WT_KSEM); //阻塞原因:WT_KSEM(wait kernel semaphore)
    assert(flags == 0);
}

// P操作的具体实现(wait_state表示阻塞的原因)
static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);                 // 关中断
    if (sem->value > 0) {                       // 可获得资源
        sem->value --;
        local_intr_restore(intr_flag);
        return 0;
    }

    // 资源不足 => 阻塞当前进程,放入这个sem的阻塞队列; 设置被阻塞的原因为wait_state
    wait_t __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state); // 此函数:为当前进程初始化wait结点,并将其加入等待队列！
    local_intr_restore(intr_flag);              // 开中断

    // 运行调度器选择另一个进程执行
    schedule();     // => 进入schedule后,便已经是另一个进程的上下文了...

    // ....执行另一个进程
    // 当这个被阻塞的进程被唤醒后,回到它的上下文,从schedule()后接着执行

    //如果被V操作唤醒,则把自身关联的wait从等待队列中删除 
    // => 个人感觉这个操作放到_up函数(唤醒进程时)中逻辑更好一点?
    local_intr_save(intr_flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state) {         //按理说应该相等!
        return wait->wakeup_flags;  
    }
    return 0;
}

```

V操作:
```
// V操作
void up(semaphore_t *sem) {
    __up(sem, WT_KSEM);
}

// V操作的具体实现
static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);                     // 关中断
    {
        wait_t *wait;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {   //队列中没有等待进程
            sem->value ++;
        }
        else {              // 唤醒wait对应的进程,wakeup_wait会将其加入就绪队列
            assert(wait->proc->wait_state == wait_state);    // 有等待进程且等待的原因是这个信号量设置的,则唤醒进程
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);                 // 开中断
}
```
 
- **要点备注**  
`信号量P、V操作的原子性是由开关中断保证的`;  
`ucore的实现:P操作中,由于阻塞了当前进程,所以紧接着需要进行调度,详见_down()函数,很重要!!!`;  

## 基于内核级信号量的哲学家就餐问题
- **概述**  
lab7中的哲学家就餐问题并不是ucore中的一个基础功能,lab7只是用这个问题来检验进程同步,它相对于其他代码而言是比较独立的,可理解为仅用于测试.测试整个进程同步的入口在proc.c中的init_main()函数,调用关系如下:  
```
init_main() => check_sync() => philosopher_using_semaphore()
```
check_sync()创建了5个子进程模拟哲学家,每个哲学家都执行philosopher_using_semaphore(),函数细节如下:
```
int philosopher_using_semaphore(void * arg) /* i：哲学家号码，从0到N-1 */
{
    int i, iter=0;
    i=(int)arg;
    cprintf("I am No.%d philosopher_sema\n",i);
    while(iter++<TIMES)
    { /* 无限循环 */
        cprintf("Iter %d, No.%d philosopher_sema is thinking\n",iter,i); /* 哲学家正在思考 */
        do_sleep(SLEEP_TIME);
        phi_take_forks_sema(i); 
        /* 需要两只叉子，或者阻塞 */
        cprintf("Iter %d, No.%d philosopher_sema is eating\n",iter,i); /* 进餐 */
        do_sleep(SLEEP_TIME);
        phi_put_forks_sema(i); 
        /* 把两把叉子同时放回桌子 */
    }
    cprintf("No.%d philosopher_sema quit\n",i);
    return 0;    
}
```
更多细节见check_sync.c

- **哲学家do_sleep()函数解析=>定时器**  
1.do_sleep(int time)函数会让当前进程睡眠/阻塞指定时间,挑选另一个进程来执行;  
2.这个阻塞的进程被放入等待/阻塞队列`timer_list`;  
3.每次时钟中断时,trap_dispatch()函数会调用run_timer_list()函数,将时间到期的进程放入就绪队列;  
...
这些都是基于定时器,详见下文【补充】部分阐述,以及sched.c

- **哲学家就餐问题的实现**  
ucore中哲学家就餐问题的解法与原理课程中讲解的不同;  
ucore中实现的方法参考自《现代操作系统》,详见书籍2.5-经典IPC问题,ucore中具体实现如下(check_sync.c):  
```
int state_sema[N]; /* 记录每个人状态的数组,共5人 */
/* 信号量是一个特殊的整型变量 */
semaphore_t mutex; /* 临界区互斥:互斥信号量,初始化为1 */
semaphore_t s[N]; /* 每个哲学家一个信号量:同步信号量,初始化为0 */

struct proc_struct *philosopher_proc_sema[N];

// 尝试获得哲学家i的左右两把叉子
void phi_test_sema(int i) /* i：哲学家号码从0到N-1 */
{   
    // 只有在左右哲学家都空闲时,才能进入就餐状态
    if(state_sema[i]==HUNGRY && state_sema[LEFT]!=EATING && state_sema[RIGHT]!=EATING)
    {
        state_sema[i]=EATING;
        up(&s[i]);                  
    }
}

// 哲学家i拿起叉子吃饭
void phi_take_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
        down(&mutex); /* 进入临界区 */
        state_sema[i]=HUNGRY; /* 记录下哲学家i饥饿的事实 */
        phi_test_sema(i); /* 试图得到两只叉子 */
        up(&mutex); /* 离开临界区 */
        down(&s[i]); /* 如果得不到叉子就阻塞 */
}

// 哲学家i放下叉子
void phi_put_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
        down(&mutex); /* 进入临界区 */
        state_sema[i]=THINKING; /* 哲学家进餐结束 */
        phi_test_sema(LEFT); /* 看一下左邻居现在是否能进餐 */
        phi_test_sema(RIGHT); /* 看一下右邻居现在是否能进餐 */
        up(&mutex); /* 离开临界区 */
}


// 每个哲学家都执行此函数
int philosopher_using_semaphore(void * arg) /* i：哲学家号码，从0到N-1 */
{
    int i, iter=0;
    i=(int)arg;
    cprintf("I am No.%d philosopher_sema\n",i);
    while(iter++<TIMES)
    {   /* 无限循环 */
        cprintf("Iter %d, No.%d philosopher_sema is thinking\n",iter,i); /* 哲学家正在思考 */
        do_sleep(SLEEP_TIME);
        phi_take_forks_sema(i);           // 拿两把叉子
        /* 需要两只叉子，或者阻塞 */
        cprintf("Iter %d, No.%d philosopher_sema is eating\n",iter,i); /* 进餐 */
        do_sleep(SLEEP_TIME);
        phi_put_forks_sema(i);           // 放下叉子
        /* 把两把叉子同时放回桌子 */
    }
    cprintf("No.%d philosopher_sema quit\n",i);
    return 0;    
}
```
理解:某个哲学家要拿筷子吃东西,先把自己的状态成饥饿,然后看左右两边的人是不是在吃 =>   
1.如果他们没在吃,那哲学家就可以拿筷子,拿筷子的时候,信号量up,拿到筷子之后信号量down,不用等待;  
2.如果他们其中有人在吃,那哲学家不能拿筷子,这时直接信号量down,也就堵塞等待;  
3.那谁通知阻塞的哲学家可以解除阻塞呢,就是它旁边两个拿了筷子的人.他们其中一个放下这个哲学家要的筷子的时候,看到这个哲学家是饥饿的,而且它两边的人都没在吃了,就up了一下它的信号量,那刚才堵塞的down就会解除,它就可以重新尝试拿筷子了;  
4.可以看出,这里的信号量s[]起到了通知、等待的作用;  
`问题`:每个哲学家对应的一个信号量代表什么?为什么初始化为0?为什么拿筷子时是V操作,拿起后才是P操作?  
【答】待完成....


## 给用户态进程/线程提供信号量
??


# 练习2:完成内核级条件变量和基于内核级条件变量的哲学家就餐问题
详见源码...

# 补充:同步互斥机制的设计与实现
## 实验执行流程概述
- **同步与互斥的关系**  
`互斥`是指某一资源同时只允许一个进程对其进行访问,具有唯一性和排它性,但互斥不用限制进程对资源的访问顺序,即访问可以是无序的;  
`同步`是指在进程间的执行必须严格按照规定的某种先后次序来运行,即访问是有序的,这种先后次序取决于要系统完成的任务需求;  
`在进程写资源情况下,进程间要求满足互斥条件.在进程读资源情况下,可允许多个进程同时访问资源`.

- **实验概述**  
实验七设计实现了多种同步互斥手段,包括时钟中断管理、等待队列、信号量、管程机制(包含条件变量设计)等,并基于信号量实现了哲学家问题的执行过程.而本次实验的练习是要求用管程机制实现哲学家问题的执行过程.在实现信号量机制和管程机制时,需要让无法进入临界区的进程睡眠,为此在ucore中设计了等待队列wait_queue.当进程无法进入临界区(即无法获得信号量)时,可让进程进入等待队列,这时的进程处于等待状态(即阻塞),从而会让实验六中的调度器选择一个处于就绪状态(即RUNNABLE STATE)的进程,进行进程切换,让新进程有机会占用CPU执行,从而让整个系统的运行更加高效.  
在实验七中的ucore初始化过程,开始的执行流程都与实验六相同,直到执行到`创建第二个内核线程init_main时,修改了init_main的具体执行内容,即增加了check_sync函数的调用`,而位于lab7_figs/kern/sync/check_sync.c中的check_sync函数可以理解为是实验七的起始执行点,是实验七的总控函数.进一步分析此函数,可以看到这个函数主要分为了两个部分:第一部分是实现基于信号量的哲学家问题,第二部分是实现基于管程的哲学家问题.  
对于check_sync函数的第一部分:首先实现初始化了一个互斥信号量,然后创建了对应5个哲学家行为的5个信号量,并创建5个内核线程代表5个哲学家,每个内核线程完成了基于信号量的哲学家吃饭睡觉思考行为实现.这部分是给学生作为练习参考用的.学生可以看看信号量是如何实现的,以及如何利用信号量完成哲学家问题.  
对于check_sync函数的第二部分:首先初始化了管程,然后又创建了5个内核线程代表5个哲学家,每个内核线程要完成基于管程的哲学家吃饭、睡觉、思考的行为实现.这部分需要学生来具体完成.学生需要`掌握如何用信号量来实现条件变量`,以及包含条件变量的管程如何能够确保哲学家能够正常思考和吃饭.  

## 同步互斥机制的底层支撑
根据操作系统原理的知识,如果没有在硬件级保证读内存-修改值-写回内存的原子性,我们只能通过复杂的软件来实现同步互斥操作.但由于有定时器、屏蔽/使能中断、等待队列wait_queue、支持test_and_set_bit等原子操作机器指令(在本次实验中没有用到)的存在,使得我们在实现进程等待、同步互斥上得到了极大的简化.下面将对定时器、屏蔽/使能中断和等待队列进行进一步讲解

### 1.计时器
在传统的操作系统中,定时器是其中一个基础而重要的功能.它提供了基于时间事件的调度机制.在ucore中,`时钟(timer)中断给操作系统提供了有一定间隔的时间事件,操作系统将其作为基本的调度和计时单位`(我们记两次时间中断之间的时间间隔为一个时间片,timer slice).  

`基于此时间单位,操作系统得以向上提供基于时间点的事件,并实现基于时间长度的睡眠等待和唤醒机制`.在每个时钟中断发生时,操作系统产生对应的时间事件.应用程序或者操作系统的其他组件可以以此来构建更复杂和高级的进程管理和调度算法.  

sched.h, sched.c 定义了有关timer的各种相关接口来使用timer服务,其中主要包括:
```
typedef struct {...} timer_t: 定义了timer_t的基本结构,其可以用sched.h中的timer_init函数对其进行初始化;

void timer_init(...): 对某定时器进行初始化,让它在expires时间片之后唤醒proc进程;

void add_timer(...):添加定时器到链表,该定时器在指定时间后被激活,并将对应的进程唤醒至runnable;

void del_timer(...):删除某一个定时器,该定时器在取消后不会被系统激活并唤醒进程;

void run_timer_list():更新当前系统时间点,遍历当前所有处在系统管理内的定时器,找出所有应该激活的计数器,并激活它们.该过程在且只在每次定时器中断时被调用.在ucore中,其还会调用调度器事件处理程序.
```
一个timer_t在系统中的存活周期可以被描述如下:  
1.timer_t在某个位置被创建和初始化,并通过add_timer加入系统管理列表中;  
2.系统时间被不断累加,直到run_timer_list发现该timer_t到期;  
3.run_timer_list更改对应的进程状态,并从系统管理列表中移除该timer_t.  
注意:`ucore中定时器链表实际上是一个差分数组,从而在每个时钟中断时,不用遍历整个数组来修改每个定时器是的expire,只需修改链表的第一个元素即可`

### 2.屏蔽与使能中断
根据操作系统原理的知识,如果没有在硬件级保证读内存-修改值-写回内存的原子性,我们只能通过复杂的软件来实现同步互斥操作.但由于有`开关中断和test_and_set_bit等原子操作机器指令的存在,使得我们在实现同步互斥原语上可以大大简化`.  

在ucore中提供的底层机制包括中断屏蔽/使能控制等.kern/sync.c中实现的开关中断的控制函数local_intr_save(x)和local_intr_restore(x),它们是基于kern/driver文件下的intr_enable()、intr_disable()函数实现的.具体调用关系为:
```
关中断：local_intr_save --> __intr_save --> intr_disable --> cli
开中断：local_intr_restore--> __intr_restore --> intr_enable --> sti
```
最终的`cli和sti`是x86的机器指令,实现了关(屏蔽)中断和开(使能)中断,即设置了eflags寄存器中与中断相关的位.`通过关闭中断,可以防止当前执行的控制流被其他中断事件处理所打断`.既然不能中断,那也就意味着在内核运行的当前进程无法被打断或被重新调度,即`实现了对临界区的互斥操作`.所以在单处理器情况下,可以通过开关中断实现对临界区的互斥保护,需要互斥的临界区代码的一般写法为:  
```
local_intr_save(intr_flag);
{
  临界区代码
}
local_intr_restore(intr_flag);
......
```
由于`目前ucore只实现了对单处理器的支持`,所以通过这种方式,就可简单地支撑互斥操作了.`在多处理器情况下,这种方法是无法实现互斥的,因为屏蔽了一个CPU的中断,只能阻止本地CPU上的进程不会被中断或调度,并不意味着其他CPU上执行的进程不能执行临界区的代码`.所以,开关中断只对单处理器下的互斥操作起作用.在本实验中,开关中断机制是实现信号量等高层同步互斥原语的底层支撑基础之一.  

### 3.等待队列
- **等待队列概述**  
到目前为止,用户进程或内核线程还没有`睡眠(阻塞)`的支持机制.在课程中提到用户进程或内核线程可以转入等待状态以等待某个特定事件(比如睡眠,等待子进程结束,等待信号量等),当该事件发生时这些进程能够被再次唤醒.内核实现这一功能的一个底层支撑机制就是等待队列wait_queue,等待队列和每一个事件(睡眠结束、时钟到达、任务完成、资源可用等)联系起来.需要等待事件的进程在转入休眠状态后插入到等待队列中.当事件发生之后,内核遍历相应等待队列,唤醒休眠的用户进程或内核线程,并设置其状态为就绪状态(PROC_RUNNABLE),并将该进程从等待队列中清除.ucore在kern/sync/{ wait.h, wait.c }中实现了等待项wait结构和等待队列wait queue结构以及相关函数,这是实现ucore中的信号量机制和条件变量机制的基础,进入wait queue的进程会被设为等待状态(PROC_SLEEPING),直到他们被唤醒.  

- **等待队列数据结构**  
详见wait.h
```
typedef struct {
    list_entry_t wait_head;            // wait_queue的队头
} wait_queue_t;

// 等待队列上的一个节点(通过wait_link与队列联系)
// 可见,ucore实现中不止一个等待/阻塞队列; 而就绪队列只有一个!
typedef struct {
    struct proc_struct *proc;           // 等待/阻塞进程
    uint32_t wakeup_flags;              // 进程被放入等待队列的原因标记
    wait_queue_t *wait_queue;           // 此wait_t结构所属的wait_queue
    list_entry_t wait_link;             // 通过这个结点链接到对应的wait_queue
} wait_t;

// 从等待队列的节点,转换到wait_t结构
#define le2wait(le, member)         \   
    to_struct((le), wait_t, member)
```

- **等待队列相关函数**  
底层函数:对wait queue的初始化、插入、删除和查找操作;  
高层函数:基于底层函数,让进程进入等待队列、从等待队列唤醒进程等...  
详见wait.c

- **调用关系举例**  
见[调用关系举例](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab7/lab7_3_2_3_waitqueue.html)




## 信号量
信号量是一种同步互斥机制的实现,普遍存在于现在的各种操作系统内核里.相对于spinlock(自旋锁)的应用对象,信号量的应用对象是在临界区中运行的时间较长的进程.等待信号量的进程需要睡眠来减少占用CPU的开销.信号量原理如下:
```
struct semaphore {
    int count;
    queueType queue;
};

void semWait(semaphore s)
{
    s.count--;
    if (s.count < 0) {
        /* place this process in s.queue */;
        /* block this process */;
    }
}

void semSignal(semaphore s)
{   
    s.count++;
    if (s.count<= 0) {
        /* remove a process P from s.queue */;
        /* place process P on ready list */;
    }
}
```
基于上述信号量实现可以认为:当多个(>1)进程可以进行互斥或同步合作时,一个进程会由于无法满足信号量设置的某条件而在某一位置停止,直到它接收到一个特定的信号(表明条件满足了).为了发信号,需要使用一个称作信号量的特殊变量.为通过信号量s传送信号,信号量的V操作采用进程可执行原语semSignal(s);为通过信号量s接收信号,信号量的P操作采用进程可执行原语semWait(s);如果相应的信号仍然没有发送,则进程被阻塞或睡眠,直到发送完为止.  

`ucore中信号量参照上述原理描述,建立在开关中断机制和wait_queue的基础上进行了具体实现`.信号量的数据结构定义如下(见sem.h):
```
typedef struct {
    int value;                  // 信号量的当前值:可用资源数
    wait_queue_t wait_queue;    // 该信号量对应的等待队列
} semaphore_t;
```
semaphore_t是最基本的记录型信号量(record semaphore)结构,包含了用于计数的整数值value,和一个进程等待队列wait_queue,一个等待的进程会挂在此等待队列上.信号量的计数器value具有有如下性质:  
1.value>0,表示共享资源的空闲数  
2.vlaue<0,表示该信号量的等待队列里的进程数  
3.value=0,表示等待队列为空  

在ucore中最重要的信号量操作是P操作函数`down(semaphore_t *sem)`和V操作函数`up(semaphore_t *sem)`.但这两个函数的具体实现是__down(semaphore_t *sem, uint32_t wait_state) 函数和__up(semaphore_t *sem, uint32_t wait_state)函数，二者的具体实现详见sem.c  
......

## 管程与条件变量(重要)
- **原理**  
引入管程是`为了将对共享资源的所有访问及其所需要的同步操作集中并封装起来`.Hansan为管程所下的定义:"一个管程定义了一个数据结构和能为并发进程所执行(在该数据结构上)的一组操作,这组操作能同步进程和改变管程中的数据".由上述定义可知,`管程由四部分组成`:
```
1.管程内部的共享变量;
2.管程内部的条件变量;
3.管程内部并发执行的进程;
4.对局部于管程内部的共享数据设置初始值的语句;
```
局限在管程中的数据结构,只能被局限在管程的操作过程所访问,任何管程之外的操作过程都不能访问它;另一方面,局限在管程中的操作过程也主要访问管程内的数据结构 => `管程相当于一个隔离区,它把共享变量和对它进行操作的若干个过程围了起来,所有进程要访问临界资源时,都必须经过管程才能进入,而管程每次只允许一个进程进入管程,从而需要确保进程之间互斥`.  

但在管程中仅仅有互斥操作是不够用的.进程可能需要等待某个条件Cond为真才能继续执行.如果采用忙等(busy waiting)方式:
```
while not( Cond ) do {}
```
在单处理器情况下,将会导致所有其它进程都无法进入临界区使得该条件Cond为真,该管程的执行将会发生死锁.为此,可引入条件变量(Condition Variables,简称CV).一个条件变量CV可理解为一个进程的等待队列,队列中的进程正等待某个条件Cond变为真.每个条件变量关联着一个条件,如果条件Cond不为真,则进程需要等待,如果条件Cond为真,则进程可以进一步在管程中执行.需要注意当一个进程等待一个条件变量CV(即等待Cond为真),该进程需要退出管程.这样才能让其它进程可以进入该管程执行,并进行相关操作.比如设置条件Cond为真,改变条件变量的状态,并唤醒等待在此条件变量CV上的进程.因此对条件变量CV有两种主要操作:  
```
wait_cv:被一个进程调用,以等待断言Pc被满足后该进程可恢复执行. 进程挂在该条件变量上等待时,不被认为是占用了管程;  
signal_cv:被一个进程调用,以指出断言Pc现在为真,从而可以唤醒等待断言Pc被满足的进程继续执行
```

- **哲学家就餐实例**  
有了互斥和信号量支持的管程就可用用了解决各种同步互斥问题:
```
monitor dp
{
    enum {THINKING, HUNGRY, EATING} state[5];
    condition self[5];

    void pickup(int i) {
        state[i] = HUNGRY;
        test(i);
        if (state[i] != EATING)
            self[i].wait_cv();
    }

    void putdown(int i) {
        state[i] = THINKING;
        test((i + 4) % 5);
        test((i + 1) % 5);
    }

    void test(int i) {
        if ((state[(i + 4) % 5] != EATING) &&
           (state[i] == HUNGRY) &&
           (state[(i + 1) % 5] != EATING)) {
              state[i] = EATING;
              self[i].signal_cv();
        }
    }

    initialization code() {
        for (int i = 0; i < 5; i++)
        state[i] = THINKING;
        }
}
```

- **关键数据结构**  
虽然大部分教科书上说明`管程适合在语言级实现比如java等高级语言`,没有提及在采用C语言的OS中如何实现.下面我们将要尝试在`ucore中用C语言实现基于互斥和条件变量机制的管程`.`ucore中的管程机制是基于信号量和条件变量来实现的`.ucore中的`管程`的数据结构monitor_t定义如下(详见monitor.h):  
```
typedef struct monitor{
    semaphore_t mutex;      // the mutex lock for going into the routines in monitor, should be initialized to 1
    semaphore_t next;       // the next semaphore is used to down the signaling proc itself, and the other 
                            // OR wakeuped waiting proc should wake up the sleeped signaling proc.
    int next_count;         // the number of of sleeped signaling proc
    condvar_t *cv;          // the condvars in monitor
} monitor_t;
```
管程中的成员变量`mutex是一个二值信号量,是实现每次只允许一个进程进入管程的关键元素,确保了互斥访问性质`;管程中的条件变量cv通过执行wait_cv,会使得等待某个条件Cond为真的进程能够离开管程并睡眠,且让其他进程进入管程继续执行;而进入管程的某进程设置条件Cond为真并执行signal_cv时,能够让等待某个条件Cond为真的睡眠进程被唤醒,从而继续进入管程中执行.  

注意:管程中的成员变量信号量next和整型变量next_count是配合进程对条件变量cv的操作而设置的,这是由于发出signal_cv的进程A会唤醒由于wait_cv而睡眠的进程B,由于管程中只允许一个进程运行,所以进程B执行会导致唤醒进程B的进程A睡眠.直到进程B离开管程,进程A才能继续执行,这个同步过程是通过信号量next完成的;而next_count表示由于发singal_cv而睡眠的进程个数.  

管程中的`条件变量`的数据结构condvar_t定义如下:
```
typedef struct condvar{
    semaphore_t sem;        // the sem semaphore  is used to down the waiting proc, and the signaling proc should up the waiting proc
    int count;              // the number of waiters on condvar
    monitor_t * owner;      // the owner(monitor) of this condvar
} condvar_t;
```
信号量sem用于让发出wait_cv操作的等待某个条件Cond为真的进程睡眠,而让发出signal_cv操作的进程通过这个sem来唤醒睡眠的进程.count表示等在这个条件变量上的睡眠进程的个数.owner表示此条件变量的宿主是哪个管程.  

- **条件变量的signal和wait的设计**  
ucore设计实现了条件变量wait_cv操作和signal_cv操作对应的具体函数,即cond_wait函数和cond_signal函数.此外还有cond_init初始化函数.  
**wait_cv原理**  
```
cv.count++;
if(monitor.next_count > 0)
   sem_signal(monitor.next);
else
   sem_signal(monitor.mutex);
sem_wait(cv.sem);
cv.count -- ;
```
如果进程A执行了cond_wait函数,表示此进程等待某个条件Cond不为真,需要睡眠.因此表示等待此条件的睡眠进程个数cv.count要加一.接下来会出现两种情况:  
```
【情况一】如果monitor.next_count如果大于0,表示有大于等于1个进程执行cond_signal函数且睡了,就睡在了monitor.next信号量上(假定这些进程挂在monitor.next信号量相关的等待队列Ｓ上),因此需要唤醒等待队列Ｓ中的一个进程B;然后进程A睡在cv.sem上.如果进程A醒了,则让cv.count减一,表示等待此条件变量的睡眠进程个数少了一个,可继续执行了!
这里隐含这一个现象,即某进程A在时间顺序上先执行了cond_signal,而另一个进程B后执行了cond_wait,这会导致进程A没有起到唤醒进程B的作用.
问题: 在cond_wait有sem_signal(mutex),但没有看到哪里有sem_wait(mutex),这好像没有成对出现,是否是错误的? 答案:其实在管程中的每一个函数的入口处会有wait(mutex),这样二者就配好对了.  

【情况二】如果monitor.next_count如果小于等于0,表示目前没有进程执行cond_signal函数且睡着了,那需要唤醒的是由于互斥条件限制而无法进入管程的进程,所以要唤醒睡在monitor.mutex上的进程.然后进程A睡在cv.sem上,如果睡醒了,则让cv.count减一,表示等待此条件的睡眠进程个数少了一个,可继续执行了!  
```
**signal_cv原理**  
```
if( cv.count > 0) {
   monitor.next_count ++;
   sem_signal(cv.sem);
   sem_wait(monitor.next);
   monitor.next_count -- ;
}
```
首先进程B判断cv.count,如果不大于0,则表示当前没有执行cond_wait而睡眠的进程,因此就没有被唤醒的对象了,直接函数返回即可;如果大于0,这表示当前有执行cond_wait而睡眠的进程A,因此需要唤醒等待在cv.sem上睡眠的进程A.由于只允许一个进程在管程中执行,所以一旦进程B唤醒了别人(进程A),那么自己就需要睡眠.故让monitor.next_count加一,且让自己(进程B)睡在信号量monitor.next上.如果睡醒了,这让monitor.next_count减一.

- **管程中函数的入口出口设计**
为了让整个管程正常运行,还需在管程中的每个函数的入口和出口增加相关操作,即:
```
function_in_monitor(...)
{
  sem.wait(monitor.mutex);
  //-----------------------------
  the real body of function;
  //-----------------------------
  if(monitor.next_count > 0)
     sem_signal(monitor.next);
  else
     sem_signal(monitor.mutex);
}
```
这样带来的作用有两个:(1)只有一个进程在执行管程中的函数;  (2)避免由于执行了cond_signal函数而睡眠的进程无法被唤醒.对于第二点,如果进程A由于执行了cond_signal函数而睡眠(这会让monitor.next_count大于0,且执行sem_wait(monitor.next)),则其他进程在执行管程中的函数的出口,会判断monitor.next_count是否大于0,如果大于0,则执行sem_signal(monitor.next),从而执行了cond_signal函数而睡眠的进程被唤醒.上诉措施将使得管程正常执行.  

注:上述只是原理描述,与具体实现相比,还有一定的差距

# 总结:锁(临界区)、信号量(临界区)、条件变量(管程)
待完成


# 相关要点/问题速览
- **同步机制的入口?**  
proc.c: init_main() => check_sync()  
详见check_sync.c

- **如何理解时钟中断?多久一次时钟中断?**  
时钟中断是中断的一种,在trap_dispatch()函数中会处理时钟中断;  
发生时钟中断后,调用run_timer_list()函数,从而唤醒阻塞的进程.ucore的进程调度也是发生时钟中断发生时;  
多久一次? => 与操作系统的时钟管理有关,可以在内核代码中设置中断频率....


- **有多少等待队列?有多少就绪队列?**  
就绪队列只有一个;  
等待队列有很多个=> 有多少信号量就有多少等待(阻塞队列)  

- **如何保证P/V操作的原子性?**  
信号量P、V操作的原子性是由开关中断保证的;  
需要注意的是,`P/V操作并不是整个函数都需要保证原子性`,我们只需关注其中不能被中断的代码(修改value、入队出队),在这部分代码前后开关中断即可 => 详见`__down()`函数  
由__down(sync/sem.c)源码可知,线程在信号量上阻塞时,正是在__down的内部完成线程切换的=> 当它再次被唤醒时,才在_down的内部继续往下执行  


- **同步与互斥的含义及其关系?**  
`互斥`是指某一资源同时只允许一个进程对其进行访问,具有唯一性和排它性,但互斥不用限制进程对资源的访问顺序,即访问可以是无序的;  
`同步`是指在进程间的执行必须严格按照规定的某种先后次序来运行,即访问是有序的,这种先后次序取决于要系统完成的任务需求;  
`在进程写资源情况下,进程间要求满足互斥条件.在进程读资源情况下,可允许多个进程同时访问资源`.

- **理解定时器中结点组织的方式(很有技巧性)**  
ucore中定时器链表实际上是一个`差分数组`(链表) =>   
1.向链表timer_list中添加定时器时,遍历链表,查找合适的位置时会不断减小timer的expire字段,目的就是计算差值(详见sched.c/add_timer()函数);  
2.从链表timer_list中删除某个定时器时,需要修改timer后面的结点的expire字段,因为要删除timer,必须更新后面结点的差分值(详见sched.c/add_timer()函数);  
3.每次时钟中断时,会调用run_timer_list()函数,这个函数修改所有阻塞线程的时间片时,并未遍历整个链表,只需将第一个结点的expire减1即可,`这就是使用差分的好处`!  


- **使用关中断保证原子操作的缺点?**  
关中断只适用于一个cpu的情况,因为只能关闭当前程序占用的cpu的中断;  


- **如何理解管程?**  
详见monitor.h中的英文注释部分(必看)!  

- **使用管程时,线程到底会阻塞在哪些地方?**  
1.阻塞在管程的mutex信号量上:这是由于暂时无法进入管程;  
2.阻塞在管程的next信号量上:虽然进入了管程,但是因为发出signal唤醒了其他线程,自己必须被阻塞,此时放在管程的next信号量的阻塞队列上;  
3.阻塞在条件变量的成员sem信号量上:


