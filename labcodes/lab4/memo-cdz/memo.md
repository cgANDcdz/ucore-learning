[toc]

# 练习0:填写已有实验
已填写完整...

# 练习1:分配并初始化一个进程控制块
- **请说明proc_struct中context和tf成员变量含义和在本实验中的作用?**  
??

# 练习2:为新创建的内核线程分配资源
- **请说明ucore是否做到给每个新fork的线程一个唯一的id?**  
??


# 练习3:理解proc_run函数
- **proc_run调用的函数是如何完成进程切换的**  
??
- **在本实验的执行过程中，创建且运行了几个内核线程?**  
??
- **语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);在这里有何作用?**  
??

# 补充:进程/线程的属性与特征

## 内核线程与用户进程
- **内核线程**  
`内核线程是一种特殊的进程`  

- **内核线程与用户进程的区别**  
1.内核线程只运行在内核态 <=> 用户进程会在用户态和内核态交替运行(`在内核态执行可理解为:内核帮助用户进程执行`...)  
2.`所有内核线程共用ucore内核内存空间`,不需要为每个内核线程维度单独的内存空间 <=> 而用户进程需要维护各自的用户内存空间  


## 资源管理
`最基本的资源就是内存和CPU`;  
在有多个进程存在的情况下,对于CPU这种资源,需要通过进程调度来合理选择一个进程,并进一步通过进程分派和进程切换让不同的进程分时复用CPU,执行各自的工作.对于无法剥夺的共享资源,如果资源管理不当,多个进程会出现死锁饥饿现象  

## 进程状态管理  
进程不同的生命周期对应不同状态,通常使用进程的五态模型+进程挂起描述进程状态,如图所示:  
![进程状态模型](memo-pic/进程状态模型.png)  
这些进程状态的转换时机需要操作系统管理起来,而且进程的创建和清除等服务必须由操作系统提供,而且在"运行"、"就绪"、"阻塞"等状态之间的切换,涉及到保存和恢复进程的执行现场,即`进程上下文`.  

## 进程与线程  
一个具有状态(执行、就绪等)的进程是一个被操作系统`分配资源`(比如分配内存)和`调度`(比如分时使用CPU)的单位.资源分配和调度是进程的两个主要特征 => 但这两个特征相对独立,操作系统可见把这两个特征分别进行管理.  
引入线程后, 把拥有资源所有权的单位仍称作进程,对资源的管理称为进程管理;  
把指令执行流的单位称作线程,对线程的管理就是线程调度和线程分派;  
对于同属于一个进程的所有线程而言,他们共享进程的虚拟地址空间和其他资源,但是`每个线程都有一个独立的栈,还有独立的线程运行上下文`,用于包含表示线程执行现场的寄存器值等信息.  
多线程环境中,进程被定义成资源分配与保护的单位.一个进程可以由多个线程,每个线程有线程执行状态(运行、就绪、等待等),保存上次运行时的线程上下文、线程执行栈等.线程就是特殊的不拥有资源的轻量级进程.  
在ucore的调度和执行管理中,并没有区分线程和进程.且`ucore内核中所有内核线程共享一个内核地址空间和其他资源,所以这些内核线程从属于同一个唯一的内核进程,即ucore内核本身`.

# 补充:内核线程管理(重要)
## 实验执行流程概述
- **概述**  
lab4主要是对CPU进行虚拟化,让ucore实现分时共享CPU,实现多条控制流并发执行.本实验涉及内核线程管理,它是一种特殊的进程  

- **线程控制块**  
为了实现内核线程,需要设计管理线程的数据结构,即`进程控制块(在这里也可叫做线程控制块)`.如果要让内核线程运行,我们首先要创建内核线程对应的进程控制块,还需把这些进程控制块通过链表连在一起,便于随时进行插入,删除和查找操作等进程管理事务.这个链表就是进程控制块链表.然后在通过调度器(scheduler)来让不同的内核线程在不同的时间段占用CPU执行,实现对CPU的分时共享.  

- **进程创建**  
在ucore的总控函数中,进程相关的初始化工作由proc_init()启动(见init.c).proc_init函数内部完成了idleproc内核线程和initproc线程的创建或复制工作(见proc.c);  
`idleproc`内核线程的工作就是不停地查询,看是否有其他内核线程可以执行.如果有,马上让调度器选择那个内核线程执行(参考proc.c/cpu_idle函数) => 所以idleproc内核线程是在ucore操作系统没有其他内核线程可执行的情况下才会被调用;  
`initproc`内核线程的工作就是显示"Hello World",表明自己存在且能正常工作了.  

- **进程调度与切换**  
调度器会在特定的调度点上执行调度,完成进程切换.在lab4中,这个调度点就一处,即在cpu_idle函数中,此函数如果发现当前进程(也就是idleproc)的need_resched置为1(在初始化idleproc的进程控制块时就置为1了),则调用schedule函数,完成进程调度和进程切换.进程调度的过程其实比较简单,就是在进程控制块链表中查找到一个"合适"的内核线程,所谓"合适"就是指内核线程处于"PROC_RUNNABLE"状态.在接下来的switch_to函数(在后续有详细分析,有一定难度,需深入了解一下)完成具体的进程切换过程.一旦切换成功,那么initproc内核线程就可以通过显示字符串来表明本次实验成功.
接下来将主要介绍了进程创建所需的重要数据结构--进程控制块 proc_struct,以及ucore创建并执行内核线程idleproc和initproc的两种不同方式,特别是`创建initproc的方式将被延续到实验五中,扩展为创建用户进程的主要方式`.另外,还初步涉及了进程调度(实验六涉及并会扩展)和进程切换内容.

## 关键数据结构-PCB  
进程控制块的定义如下(详见proc.h):  
```
struct proc_struct {
    enum proc_state state;                      // Process state
    int pid;                                    // Process ID
    int runs;                                   // the running times of Proces
    uintptr_t kstack;                           // Process kernel stack
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;                 // the parent process
    struct mm_struct *mm;                       // Process's memory management field
    struct context context;                     // Switch here to run process
    struct trapframe *tf;                       // Trap frame for current interrupt
    uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // Process name
    list_entry_t list_link;                     // Process link list 
    list_entry_t hash_link;                     // Process hash list
};
```
以下是对一些重要成员变量的解释  

- **mm**  
内存管理的信息,包括内存映射列表、页表指针等(详见vmm.h). mm成员变量在lab3中用于虚存管理.但`在实际OS中,内核线程常驻内存,不需要考虑swap page问题`.在lab5中涉及到了用户进程,才考虑进程用户内存空间的swap page问题,mm才会发挥作用.所以`在lab4中mm对于内核线程就没有用了,这样内核线程的proc_struct的成员变量mm=0是合理的`! mm里有个成员pgdir非常重要,它是该进程使用的一级页表的物理地址.由于*mm=NULL,所以在proc_struct数据结构中需要有一个代替pgdir项来记录页表起始地址,这就是proc_struct数据结构中的cr3成员变量.  

- **parent**  
用户进程的父进程.`在所有进程中,只有一个进程没有父进程,那就是内核创建的第一个内核线程idleproc`.内核根据这个父子关系建立了一个树形结构,用于维护一些特殊操作,比如确定某个进程是否可以对另一个进程进行某种操作等...  

- **context**  
进程的上下文,用于进程切换(见switch.S/switch_to).在ucore中,`所有的进程在内核中也是相对独立的,有独立的内核堆栈以及上下文等`.使用context保存寄存器的目的在于在内核态中能够进行上下文之间的切换.  

- **tf**  
中断帧(详见trap.h)的指针,它指向内核栈的某个位置:当进程从用户空间跳到内核空间时,中断帧记录了进程在被中断前的状态;当进程需要从内核跳回用户空间时,需要调整中断帧以恢复让进程继续执行的各寄存器值.(为了实现嵌套中断,ucore在内核栈上维护了tf链,详见trap.c/trap函数)  

- **cr3**  
保存页表的物理地址,目的是进程切换的时候方便直接使用lcr3实现页表切换,避免每次都根据mm来计算cr3.`mm数据结构是用来实现用户空间的虚存管理的,但是内核线程没有用户空间,它执行的只是内核中的一小段代码(函数),所以它没有mm结构,即内核线程的PCB/TCB中,mm=NULL`;  
对于一个普通的用户态进程,PCB中的cr3就是mm中的pgdir;  
对于内核线程,cr3=boot_cr3=boot_pgdir.  

- **kstack**  
kstack记录了分配给该进程/线程的内核栈的位置!  
`每个线程都有一个内核栈,并且位于内核地址空间的不同位置`;  
对于内核线程,该栈就是运行时程序使用的栈;  
`对于普通线程,该栈是发生特权级改变时,用于保存被打断时的现场信息的内核栈`;  
uCore在创建进程时分配了2个连续的物理页(参见memlayout.h中KSTACKSIZE的定义)作为内核栈的空间.这个栈很小,所以内核中的代码应该尽可能的紧凑,并且避免在栈上分配大的数据结构,以免栈溢出,导致系统崩溃.  
`kstack主要作用`有以下几点:  
1.首先,当内核准备从一个进程切换到另一个的时候,需要根据kstack的值正确的设置好tss(可以回顾一下在实验一中讲述的tss在中断处理过程中的作用),以便在进程切换以后再发生中断时能够使用正确的栈.  
2.其次,内核栈位于内核地址空间,并且是不共享的(每个线程都拥有自己的内核栈),因此不受到mm的管理,当进程退出的时候,内核能够根据kstack的值快速定位栈的位置并进行回收.  
3.(重要学习材料)uCore的这种内核栈的设计借鉴的是linux的方法(但由于内存管理实现的差异,它实现的远不如linux的灵活),它使得每个线程的内核栈在不同的位置,这样从某种程度上方便调试,但同时也使得内核对栈溢出变得十分不敏感,因为一旦发生溢出,它极可能污染内核中其它的数据使得内核崩溃.如果能够通过页表,将所有进程的内核栈映射到固定的地址上去,能够避免这种问题,但又会使得进程切换过程中对栈的修改变得相当繁琐.感兴趣的同学可以参考 linux kernel 的代码对此进行尝试.  



# 补充:创建并执行内核线程
建立进程控制块(proc.c中的alloc_proc函数)后,就可以通过进程控制块来创建具体的进程/线程了.首先,考虑最简单的内核线程,它通常只是内核中的一小段代码或者函数,没有自己的"专属"空间.这是由于`在uCore OS启动后,已经对整个内核内存空间进行了管理,通过设置页表建立了内核虚拟空间(即boot_cr3指向的一级页表描述的空间).所以uCore OS内核中的所有线程都不需要再建立各自的页表,只需共享这个内核虚拟空间就可以访问整个物理内存了`.从这个角度看,内核线程被uCore OS内核这个大"内核进程"所管理  

## 创建第0个内核线程idleproc
在init.c::kern_init函数调用了proc.c::proc_init函数.proc_init函数启动了创建内核线程的步骤.首先`当前的执行上下文(从kern_init 启动至今)就可以看成是uCore内核(也可看做是内核进程)中的一个内核线程的上下文`.为此,uCore通过给当前执行的上下文分配一个进程控制块以及对它进行相应初始化,将其打造成第0个内核线程 -- idleproc.具体步骤如下:  
首先调用alloc_proc函数来通过kmalloc函数获得proc_struct结构的一块内存块,作为第0个进程控制块.并把proc进行初步初始化(即把proc_struct中的各个成员变量清零).但有些成员变量设置了特殊的值,比如:  
```
proc->state = PROC_UNINIT;  设置进程为“初始”态
proc->pid = -1;             设置进程pid的未初始化值
proc->cr3 = boot_cr3;       使用内核页目录表的基址
...
```
上述三条语句中,第一条设置了进程的状态为"初始"态,这表示进程已经 "出生"了,正在获取资源茁壮成长中;第二条语句设置了进程的pid为-1,这表示进程的"身份证号"还没有办好;第三条语句表明由于该内核线程在内核中运行,故采用为uCore内核已经建立的页表,即设置为在uCore内核页表的起始地址boot_cr3.后续实验中可进一步看出`所有内核线程的内核虚地址空间(也包括物理地址空间)是相同的`.既然内核线程共用一个映射内核空间的页表,这表示内核空间对所有内核线程都是"可见"的,所以更精确地说,这些内核线程都应该是从属于同一个唯一的"大内核进程"—uCore内核.  
接下来,proc_init函数对idleproc内核线程进行进一步初始化:  
```
idleproc->pid = 0;
idleproc->state = PROC_RUNNABLE;
idleproc->kstack = (uintptr_t)bootstack;
idleproc->need_resched = 1;
set_proc_name(idleproc, "idle");
```
需要注意前4条语句.第一条语句给了idleproc合法的身份证号--0,这名正言顺地表明了idleproc是第0个内核线程.通常可以通过pid的赋值来表示线程的创建和身份确定."0"是第一个的表示方法是计算机领域所特有的,比如C语言定义的第一个数组元素的小标也是"0".第二条语句改变了idleproc的状态,使得它从"出生"转到了"准备工作",就差uCore调度它执行了.第三条语句设置了idleproc所使用的内核栈的起始地址.`需要注意以后的其他线程的内核栈都需要通过分配获得,因为uCore启动时设置的内核栈直接分配给idleproc使用了`.第四条很重要,因为uCore希望当前CPU应该做更有用的工作,而不是运行idleproc这个"无所事事"的内核线程,所以把idleproc->need_resched设置为"1",结合idleproc的执行主体--cpu_idle函数的实现,可以清楚看出如果当前idleproc在执行,则只要此标志为1,马上就调用schedule函数要求调度器切换其他进程执行.


## 创建第1个内核线程initproc
第0个内核线程主要工作是完成内核中各个子系统的初始化,然后就通过执行cpu_idle函数(见proc.c)开始过退休生活了.所以uCore接下来还需创建其他进程来完成各种工作,但idleproc内核子线程自己不想做,于是就通过调用kernel_thread函数创建了一个内核线程init_main.在实验四中,这个子内核线程的工作就是输出一些字符串,然后就返回了(参看init_main函数).但在`后续的实验中,init_main的工作就是创建特定的其他内核线程或用户进程`(实验五涉及).下面我们来分析一下创建内核线程的函数kernel_thread(proc.c):
```
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags){
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf_struct.tf_es = tf_struct.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}
```
注意,kernel_thread函数采用了局部变量tf来放置保存内核线程的临时中断帧,并把中断帧的指针传递给do_fork函数,而do_fork函数会调用copy_thread函数来在新创建的进程内核栈上专门给进程的中断帧分配一块空间.  
给中断帧分配完空间后,就需要构造新进程的中断帧,具体过程是:首先给tf进行清零初始化,并设置中断帧的代码段(tf.tf_cs)和数据段(tf.tf_ds/tf_es/tf_ss)为内核空间的段(KERNEL_CS/KERNEL_DS),这实际上也说明了initproc内核线程在内核空间中执行.而initproc内核线程从哪里开始执行呢?tf.tf_eip的指出了是kernel_thread_entry(位于kern/process/entry.S中),kernel_thread_entry是entry.S中实现的汇编函数,它做的事情很简单: 
```
kernel_thread_entry: # void kernel_thread(void)
pushl %edx           # push arg
call *%ebx           # call fn
pushl %eax           # save the return value of fn(arg)
call do_exit         # call do_exit to terminate current thread
```
从上可以看出,kernel_thread_entry函数主要为内核线程的主体fn函数做了一个准备开始和结束运行的"壳",并把函数fn的参数arg(保存在edx寄存器中)压栈,然后调用fn函数,把函数返回值eax寄存器内容压栈,调用do_exit函数退出线程执行.  
do_fork是创建线程的主要函数.kernel_thread函数通过调用do_fork函数最终完成了内核线程的创建工作.下面我们来分析一下do_fork函数的实现(练习2).do_fork函数主要做了以下几件事情:
```
1.分配并初始化进程控制块(alloc_proc函数);
2.分配并初始化内核栈(setup_stack函数);
3.根据clone_flag标志复制或共享进程内存管理结构(copy_mm函数);
4.设置进程在内核(将来也包括用户态)正常运行和调度所需的中断帧和执行上下文(copy_thread函数);
5.把设置好的进程控制块放入hash_list和proc_list两个全局进程链表中;
6.自此,进程已经准备好执行了,把进程状态设置为"就绪"态;
7.设置返回码为子进程的id号
```
这里需要注意的是,如果上述前3步执行没有成功,则需要做对应的出错处理,把相关已经占有的内存释放掉.copy_mm函数目前只是把current->mm设置为NULL,这是由于目前在实验四中只能创建内核线程,proc->mm描述的是进程用户态空间的情况,所以目前mm还用不上.copy_thread函数做的事情比较多,代码如下:
```
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    //在内核堆栈的顶部设置中断帧大小的一块栈空间
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf; //拷贝在kernel_thread函数建立的临时中断帧的初始值
    proc->tf->tf_regs.reg_eax = 0;
    //设置子进程/线程执行完do_fork后的返回值
    proc->tf->tf_esp = esp; //设置中断帧中的栈指针esp
    proc->tf->tf_eflags |= FL_IF; //使能中断
    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
}
```
此函数首先在内核堆栈的顶部设置中断帧大小的一块栈空间,并在此空间中拷贝在kernel_thread函数建立的临时中断帧的初始值,并进一步设置中断帧中的栈指针esp和标志寄存器eflags,特别是eflags设置了FL_IF标志,这表示此内核线程在执行过程中,能响应中断,打断当前的执行.执行到这步后,此进程的中断帧就建立好了,对于initproc而言,它的中断帧如下所示:
```
//所在地址位置
initproc->tf= (proc->kstack+KSTACKSIZE) – sizeof (struct trapframe);
//具体内容
initproc->tf.tf_cs = KERNEL_CS;
initproc->tf.tf_ds = initproc->tf.tf_es = initproc->tf.tf_ss = KERNEL_DS;
initproc->tf.tf_regs.reg_ebx = (uint32_t)init_main;
initproc->tf.tf_regs.reg_edx = (uint32_t) ADDRESS of "Helloworld!!";
initproc->tf.tf_eip = (uint32_t)kernel_thread_entry;
initproc->tf.tf_regs.reg_eax = 0;
initproc->tf.tf_esp = esp;
initproc->tf.tf_eflags |= FL_IF;
```
设置好中断帧后,最后就是设置initproc的进程上下文(process context,也称执行现场)了.只有设置好执行现场后,一旦uCore调度器选择了initproc执行,就需要根据initproc->context中保存的执行现场来恢复initproc的执行.这里设置了initproc的执行现场中主要的两个信息:上次停止执行时的下一条指令地址context.eip和上次停止执行时的堆栈地址context.esp.其实initproc还没有执行过,所以这其实就是initproc实际执行的第一条指令地址和堆栈指针.可以看出,由于initproc的中断帧占用了实际给initproc分配的栈空间的顶部,所以initproc就只能把栈顶指针context.esp设置在initproc的中断帧的起始位置.根据context.eip的赋值,可以知道initproc实际开始执行的地方在forkret函数(主要完成do_fork函数返回的处理工作)处.至此,initproc内核线程已经做好准备执行了.  

## 调度并执行内核线程initproc
在uCore执行完proc_init函数后,就创建好了两个内核线程:idleproc和initproc,这时uCore当前的执行现场就是idleproc,等到执行到init函数的最后一个函数cpu_idle之前,uCore的所有初始化工作就结束了,idleproc将通过执行cpu_idle函数让出CPU,给其它内核线程执行,具体过程如下:
    ```
    void cpu_idle(void) {
        while (1) {
            if (current->need_resched) {
                schedule();
            }
        }
    }
    ```
首先,判断当前内核线程idleproc的need_resched是否不为0,回顾前面"创建第一个内核线程idleproc"中的描述,proc_init函数在初始化idleproc中,就把idleproc->need_resched置为1了,所以会马上调用schedule函数找其他处于"就绪"态的进程执行.  
uCore在实验四中只实现了一个最简单的FIFO调度器,其核心就是schedule函数.它的执行逻辑很简单:
```
1.设置当前内核线程current->need_resched为0;  
2.在proc_list队列中查找下一个处于"就绪"态的线程或进程next;  
3.找到这样的进程后,就调用proc_run函数,保存当前进程current的执行现场(进程上下文),恢复新进程的执行现场,完成进程切换.
```
至此,新的进程next就开始执行了.由于在proc10中只有两个内核线程,且idleproc要让出CPU给initproc执行,我们可以看到schedule函数通过查找proc_list进程队列,只能找到一个处于"就绪"态的initproc内核线程.并通过proc_run和进一步的switch_to函数完成两个执行现场的切换,具体流程如下:
    ```
    1.让current指向next内核线程initproc;  
    2.设置任务状态段ts中特权态0下的栈顶指针esp0为next内核线程initproc的内核栈的栈顶,即next->kstack + KSTACKSIZE;  
    3.设置CR3寄存器的值为next内核线程initproc的页目录表起始地址next->cr3,这实际上是完成进程间的页表切换;  
    4.由switch_to函数完成具体的两个线程的执行现场切换,即切换各个寄存器,当switch_to函数执行完"ret"指令后,就切换到initproc执行了.
    ```
**注意**:  
在第二步设置任务状态段ts中特权态0下的栈顶指针esp0的目的是建立好内核线程或将来用户线程在执行特权态切换(从特权态0<-->特权态3,或从特权态3<-->特权态3)时能够正确定位处于特权态0时进程的内核栈的栈顶,而这个栈顶其实放了一个trapframe结构的内存空间.如果是在特权态3发生了中断/异常/系统调用,则CPU会从特权态3-->特权态0,且CPU从此栈顶(当前被打断进程的内核栈顶)开始压栈来保存被中断/异常/系统调用打断的用户态执行现场;如果是在特权态0发生了中断/异常/系统调用,则CPU会从从当前内核栈指针esp所指的位置开始压栈保存被中断/异常/系统调用打断的内核态执行现场.反之,当执行完对中断/异常/系统调用打断的处理后,最后会执行一个"iret"指令.在执行此指令之前,CPU的当前栈指针esp一定指向上次产生中断/异常/系统调用时CPU保存的被打断的指令地址CS和EIP,"iret"指令会根据ESP所指的保存的址CS和EIP恢复到上次被打断的地方继续执行.  
在页表设置方面,由于idleproc和initproc都是共用一个内核页表boot_cr3,所以此时第三步其实没用,但考虑到以后的进程有各自的页表,其起始地址各不相同,只有完成页表切换,才能确保新的进程能够正常执行.  
第四步proc_run函数调用switch_to函数,参数是前一个进程和后一个进程的执行现场:process context.
<br/>

我们再看看switch.S中的switch_to函数的执行流程:
    ```
    .globl switch_to
    switch_to: # switch_to(from, to)
    # save from's registers
    movl 4(%esp), %eax # eax points to from
    popl 0(%eax) # esp--> return address, so save return addr in FROM’s
    context
    movl %esp, 4(%eax)
    ……
    movl %ebp, 28(%eax)
    # restore to's registers
    movl 4(%esp), %eax # not 8(%esp): popped return address already
    # eax now points to to
    movl 28(%eax), %ebp
    ……
    movl 4(%eax), %esp
    pushl 0(%eax) # push TO’s context’s eip, so return addr = TO’s eip
    ret # after ret, eip= TO’s eip
    ```
首先,保存前一个进程的执行现场,前两条汇编指令(如下所示)保存了进程在返回switch_to函数后的指令地址到context.eip中:
    ```
    movl 4(%esp), %eax # eax points to from
    popl 0(%eax) # esp--> return address, so save return addr in FROM’s
    context
    ```
在接下来的7条汇编指令完成了保存前一个进程的其他7个寄存器到context中的相应成员变量中.至此前一个进程的执行现场保存完毕.再往后是恢复上一个进程的执行现场,这其实就是上述保存过程的逆执行过程,即从context的高地址的成员变量ebp开始,逐一把相关成员变量的值赋值给对应的寄存器,倒数第二条汇编指令"pushl 0(%eax)"其实把context中保存的下一个进程要执行的指令地址context.eip放到了堆栈顶,这样接下来执行最后一条指令"ret"时,会把栈顶的内容赋值给EIP寄存器,这样就切换到下一个进程执行了,即当前进程已经是下一个进程了.uCore会执行进程切换,让initproc执行.在对initproc进行初始化时,设置了initproc->context.eip = (uintptr_t)forkret,这样,当执行switch_to函数并返回后,initproc将执行其实际上的执行入口地址forkret.而forkret会调用位于kern/trap/trapentry.S中的forkrets函数执行,具体代码如下:
    ```
    .globl __trapret
    __trapret:
    # restore registers from stack
    popal
    # restore %ds and %es
    popl %es
    popl %ds
    # get rid of the trap number and error code
    addl $0x8, %esp
    iret
    .globl forkrets
    forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp //把esp指向当前进程的中断帧
    jmp __trapret
    ```
可以看出,forkrets函数首先把esp指向当前进程的中断帧,从_trapret开始执行到iret前,esp指向了current->tf.tf_eip,而如果此时执行的是initproc,则current->tf.tf_eip=kernel_thread_entry,initproc->tf.tf_cs = KERNEL_CS,所以当执行完iret后,就开始在内核中执行kernel_thread_entry函数了,而initproc->tf.tf_regs.reg_ebx = init_main,所以在kernl_thread_entry中执行"call %ebx"后,就开始执行initproc的主体了.Initprocde的主体函数很简单就是输出一段字符串,然后就返回到kernel_tread_entry函数,并进一步调用do_exit执行退出操作了.本来do_exit应该完成一些资源回收工作等,但这些不是实验四涉及的,而是由后续的实验来完成.至此,实验四中的主要工作描述完毕.




# 相关要点/问题速览

- **进程上下文和中断上下文?**  
https://www.cnblogs.com/hustcat/articles/1505618.html

- **ucore内核进程**  
ucore内核中所有内核线程共享一个内核地址空间和其他资源,所以这些内核线程从属于同一个唯一的内核进程,即ucore内核本身.  
在ucore的调度和执行管理中,并没有区分线程和进程 => (不管内核还是用户)ucore线程间唯一的关系就是共享同一进程的地址空间,除此之外,每个线程都是独立的个体!  

- **区分内核线程和在内核态运行的线程**  
内核线程肯定运行在内核态; 普通用户线程通过中断陷入内核后,也是运行在内核态,不过它不是内核线程!!!  

- **内核线程(进程)与用户线程(进程)的区别**  
1.内核线程只运行在内核态;  
2.`用户进程会在在用户态和内核态交替运行`,所以用户进程陷入内核时,它依然是用户线程,只是拥有较高的权限而已;  
3.`所有内核线程共用ucore内核内存空间`,不需为每个内核线程维护单独的内存空间;  
4.而用户进程需要维护各自的用户内存空间  

- **idleproc内核线程的作用?**  
在操作系统没有其他内核线程可执行的情况下,才会执行idleproc.他会不停地查询,直到有一个内核线程可以执行了,那么就让调度器执行其他内核线程.  
补充:ucore最开始执行的时候并无线程,当执行到proc_init()的时候,创建了idleproc线程,通过对PCB的初始化,将ucore最开始至今的代码都囊括到了idleproc线程中  


- **内核栈是线程共享的吗?**  
1.所有线程都有自己的内核栈,它不是进程内多个线程共享的!  
2.`内核栈位于内核地址空间中(虚拟以及物理)`,它很小,只有8KB;  

