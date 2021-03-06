[toc]

# 练习0:填写已有实验
已完成...

# 练习1:完成读文件操作的实现
- **读写文件的处理流程**  
见user/sh.c(这是测试文件系统的代码) => 从其中的reopen()函数开始递归找下去即可  
(有空再自行整理)  

- **UNIX的PIPE机制设计方案**  
参考[PIPE设计方案](https://github.com/ZebornDuan/UCore/tree/master/lab8)


# 练习2:完成基于文件系统的执行程序机制的实现
- **UNIX的硬链接和软链接机制**  
待补充

# challenge
## 实现PIPE
待完成...
## 实现软硬链接
...


# 补充:文件系统设计与实现
## 0.ucore文件系统总体介绍
- **UNIX文件系统设计**  
ucore的文件系统模型源于Havard的OS161的文件系统和Linux文件系统.但其实这二者都是源于传统的UNIX文件系统设计.UNIX提出了四个文件系统抽象概念:文件(file)、目录项(dentry)、索引节点(inode)和安装点(mount point).  
`文件`:UNIX文件中的内容可理解为是一有序字节buffer,文件都有一个方便应用程序识别的文件名称(也称文件路径名).典型的文件操作有读、写、创建和删除等.  
`目录项`:目录项不是目录(又称文件路径),而是目录的组成部分.在UNIX中目录被看作一种特定的文件,而目录项是文件路径中的一部分.如一个文件路径名是"/test/testfile",则包含的目录项为:根目录"/".目录"test"和文件"testfile",这三个都是目录项.一般而言,目录项包含目录项的名字(文件名或目录名)和目录项的索引节点(见下面的描述)位置.  
`索引节点`:UNIX将文件的相关元数据信息(如访问控制权限、大小、拥有者、创建时间、数据内容等等信息)存储在一个单独的数据结构中,该结构被称为索引节点.  
`安装点`:在UNIX中,文件系统被安装在一个特定的文件路径位置,这个位置就是安装点.所有的已安装文件系统都作为根文件系统树中的叶子出现在系统中.  
上述抽象概念形成了UNIX文件系统的逻辑数据结构,并需要通过一个具体文件系统的架构设计与实现把上述信息映射并储存到磁盘介质上,从而在具体文件系统的磁盘布局(即数据在磁盘上的物理组织)上具体体现出上述抽象概念.比如文件元数据信息存储在磁盘块中的索引节点上.`当文件被载入内存时,内核需要使用磁盘块中的索引点来构造内存中的索引节点`.  

- **ucore文件系统架构**  
ucore模仿了UNIX的文件系统设计,ucore的文件系统架构主要由四部分组成:  
`通用文件系统访问接口层`:该层提供了一个从用户空间到文件系统的标准访问接口.这一层访问接口让应用程序能够通过一个简单的接口获得ucore内核的文件系统服务(见kern/fs/sysfile.c)  
`文件系统抽象层`:向上提供一个一致的接口给内核其他部分(文件系统相关的系统调用实现模块和其他内核功能模块)访问.向下提供一个同样的抽象函数指针列表和数据结构屏蔽不同文件系统的实现细节.  
`Simple FS文件系统层`:一个基于索引方式的简单文件系统实例.向上通过各种具体函数实现以对应文件系统抽象层提出的抽象函数.向下访问外设接口.  
`外设接口层`:向上提供device访问接口屏蔽不同硬件细节.向下实现访问各种具体设备驱动的接口,比如disk设备接口/串口设备接口/键盘设备接口等.  

假如应用程序操作文件(打开/创建/删除/读写):首先需要通过文件系统的通用文件系统访问接口层给用户空间提供的访问接口进入文件系统内部 => 接着由文件系统抽象层把访问请求转发给某一具体文件系统(比如SFS文件系统) => 具体文件系统(Simple FS文件系统层)把应用程序的访问请求转化为对磁盘上的block的处理请求 => 外设接口层交给磁盘驱动例程来完成具体的磁盘操作.以用户态写文件函数write的整个执行过程为例,ucore文件系统架构的层次和依赖关系如图:
![ucore文件系统架构](./memo-pic/ucore文件系统架构.png)


- **ucore文件系统总体结构**  
从ucore操作系统不同的角度来看,ucore中的文件系统架构包含四类主要的数据结构,它们分别是:  
`超级块(SuperBlock)`,它主要从文件系统的`全局角度`描述特定文件系统的全局信息.它的作用范围是整个OS空间.  
`索引节点(inode)`:它主要从文件系统的单个`文件的角度`它描述了文件的各种属性和数据所在位置,它的作用范围是整个OS空间.  
`目录项(dentory)`:它主要从文件系统的文件`路径的角度`描述了文件路径中的一个特定的目录项(注:一系列目录项形成目录/文件路径).它的作用范围是整个OS空间.对于SFS而言,inode(具体为struct sfs_disk_inode)对应于物理磁盘上的具体对象,dentry(具体为struct sfs_disk_entry)是一个内存实体,其中的ino成员指向对应的inode number,另外一个成员是file name(文件名).  
`文件(file)`:它主要从`进程的角度`描述了一个进程在访问文件时需要了解的文件标识,文件读写的位置,文件引用情况等信息.它的作用范围是某一具体进程.  
如果一个用户进程打开了一个文件,那么在ucore中涉及的相关数据结构(其中相关数据结构将在下面各个小节中展开叙述)和关系如下图所示:
![ucore关键数据结构](./memo-pic/ucore关键数据结构.png)


## 1.通用文件系统访问接口(layer4)
- **文件和目录相关用户库函数**  
lab8中部分用户库函数与文件系统有关,我们先讨论对单个文件进行操作的系统调用,然后讨论对目录和文件系统进行操作的系统调用.  
`文件操作方面`(见user/libs/file.c),最基本的相关函数是open、close、read、write. 
1.在读写一个文件之前,首先要用open系统调用将其打开.open的第一个参数指定文件的路径名,可使用绝对路径名;第二个参数指定打开的方式,可设置为O_RDONLY、O_WRONLY、O_RDWR,分别表示只读、只写、可读可写.在打开一个文件后,就可以使用它返回的文件描述符fd对文件进行相关操作; 在使用完一个文件后,还要用close系统调用把它关闭,其参数就是文件描述符fd.这样它的文件描述符就可以空出来,给别的文件使用.  
2.读写文件内容的系统调用是read和write.read系统调用有三个参数:一个指定所操作的文件描述符,一个指定读取数据的存放地址,最后一个指定读多少个字节.在C程序中调用该系统调用的方法如下:
```
count = read(filehandle, buffer, nbytes);
```
该系统调用会把实际读到的字节数返回给count变量.在正常情形下这个值与nbytes相等,但有时可能会小一些.例如,在读文件时碰上了文件结束符,从而提前结束此次读操作.如果由于参数无效或磁盘访问错误等原因,使得此次系统调用无法完成,则count被置为-1.而write函数的参数与之完全相同.  

`对于目录而言`(见user/libs/dir.c),最常用的操作是跳转到某个目录,这里对应的用户库函数是chdir.然后就需要读目录的内容了,即列出目录中的文件或目录名,这在处理上与读文件类似,即需要通过opendir函数打开目录,通过readdir来获取目录中的文件信息,读完后还需通过closedir函数来关闭目录.由于在ucore中把目录看成是一个特殊的文件,所以opendir和closedir实际上就是调用与文件相关的open和close函数.只有readdir需要调用获取目录内容的特殊系统调用sys_getdirentry.而且这里没有写目录这一操作,在目录中增加内容其实就是在此目录中创建文件,需要用到创建文件的函数.  


- **文件和目录访问相关系统调用**  
与文件相关的open、close、read、write用户库函数对应的是sys_open、sys_close、sys_read、sys_write四个系统调用接口;与目录相关的readdir用户库函数对应的是sys_getdirentry系统调用.这些系统调用函数接口将通过syscall函数来获得ucore的内核服务.当到了ucore内核后,再调用文件系统抽象层的file接口和dir接口  
`这部分接口见user/libs/syscall.c`

## 2.文件系统抽象层VFS(layer3)
文件系统抽象层是把不同文件系统的对外共性接口提取出来,形成一个函数指针数组.这样,通用文件系统访问接口层只需访问文件系统抽象层,而不需关心具体文件系统的实现细节和接口.
### file & dir 接口  
进程访问文件是通过进程控制块中的files_struct结构获取文件相关信息,此结构定义如下(见kern/fs/fs.h):
```
struct files_struct {
    struct inode *pwd;      // 进程当前执行目录的内存指针
    struct file *fd_array;  // 进程打开文件的数组
    int files_count;        // 访问此文件的线程数
    semaphore_t files_sem;  // 确保对进程控制块中files_struct的互斥访问
};
```
当创建一个进程后,该进程的files_struct将会被初始化或复制父进程的files_struct.当`用户进程打开一个文件时,将从fd_array数组中取得一个空闲的file项,然后会把此file的成员变量node指针(见下面的file结构体)指向一个代表此文件的inode的起始地址`  
  

file&dir接口层定义了进程在内核中直接访问的文件相关信息,文件信息定义在file数据结构中,具体描述如下(见kern/fs/file.h):
```
struct file {
    enum {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    } status;                   // 文件的状态
    bool readable;              // 文件是否可读
    bool writable;              // 文件是否可写
    int fd;                     // 文件在filemap中的索引值
    off_t pos;                  // 访问当前文件的位置
    struct inode *node;         // 该文件对应的inode指针
    int open_count;             // 文件被打开次数
};
```

### inode接口
index node是位于内存的索引节点，它是VFS结构中的重要数据结构，因为它实际负责把不同文件系统的特定索引节点信息(甚至不能算是一个索引节点)统一封装起来,避免了进程直接访问具体文件系统.其定义如下(见kern/fs/vfs/inode.h):
```
struct inode {
    union {                                   // 包含不同文件系统特定inode信息的union成员变量
        struct device __device_info;          // 设备文件系统内存inode信息
        struct sfs_inode __sfs_inode_info;    // SFS文件系统内存inode信息
    } in_info;
    enum {
        inode_type_device_info = 0x1234,
        inode_type_sfs_inode_info,
    } in_type;                                // 此inode所属文件系统类型
    int ref_count;                            // 此inode的引用计数
    int open_count;                           // inode对应的文件被打开次数
    struct fs *in_fs;                         // 抽象的文件系统,包含访问文件系统的函数指针
    const struct inode_ops *in_ops;           // 抽象的inode操作,包含访问inode的函数指针  
};
```
在inode中,有一成员变量为in_ops,这是对此inode的操作函数指针列表,其数据结构定义如下(见kern/fs/vfs/inode.h:
```
struct inode_ops {
    unsigned long vop_magic;
    int (*vop_open)(struct inode *node, uint32_t open_flags);
    int (*vop_close)(struct inode *node);
    int (*vop_read)(struct inode *node, struct iobuf *iob);
    int (*vop_write)(struct inode *node, struct iobuf *iob);
    int (*vop_fstat)(struct inode *node, struct stat *stat);
    int (*vop_fsync)(struct inode *node);
    int (*vop_namefile)(struct inode *node, struct iobuf *iob);
    int (*vop_getdirentry)(struct inode *node, struct iobuf *iob);
    int (*vop_reclaim)(struct inode *node);
    int (*vop_gettype)(struct inode *node, uint32_t *type_store);
    int (*vop_tryseek)(struct inode *node, off_t pos);
    int (*vop_truncate)(struct inode *node, off_t len);
    int (*vop_create)(struct inode *node, const char *name, bool excl, struct inode **node_store);
    int (*vop_lookup)(struct inode *node, char *path, struct inode **node_store);
    int (*vop_ioctl)(struct inode *node, int op, void *data);
};
```
参照上面对SFS中的索引节点操作函数的说明，可以看出`inode_ops是对常规文件、目录、设备文件所有操作的一个抽象函数表示`.对于某一具体的文件系统中的文件或目录,只需实现相关的函数,就可以被用户进程访问具体的文件了,且用户进程无需了解具体文件系统的实现细节.  

### VFS与通用文件系统访问接口的衔接
文件操作在用户层提供了通用的接口,常用的是open、close、read、write.下面以read为例,讲述用户态的通用访问接口如何与调用到VFS层接口的:  
```
read(user/libs/file.c)  => sys_read(user/libs/syscall.c)  => syscall(user/libs/syscall.c, 然后执行中断)  
=> sys_read(kern/syscall/syscall.c,这是已经进入内核)  => sysfile_read(kern/fs/sysfile.c) => file_read(kern/fs/file.c)  => vop_read(kern/fs/vfs/inode.h)
```
其实,`进入了file_read(kern/fs/file.c)开始,基本就可看做是进入了VFS层`.在file_read中会获取file结构,它为vop_read提供必要信息,file结构虽然不是在vfs目录下,但是file&dir 以及inode都应该看做是VFS层提供的接口.  
至于inode中的inode_ops,包含一系列文件操作,这些操作是在SFS层实现,它可以看做是VFS和SFS的接口......



## 3.Simple FS文件系统(layer2)
### 概述  
ucore内核把所有文件都看作是字节流,任何内部逻辑结构都是专用的,由应用程序负责解释.但是ucore区分文件的物理结构.ucore目前支持如下几种类型的文件:  
`常规文件`:文件中包括的内容信息是由应用程序输入.SFS文件系统在普通文件上不强加任何内部结构,把其文件内容信息看作为字节.  
`目录`:包含一系列的entry,每个entry包含文件名和指向与之相关联的索引节点(index node)的指针.目录是按层次结构组织的.  
`链接文件`:实际上一个链接文件是一个已经存在的文件的另一个可选择的文件名(=> 软连接、硬连接).  
`设备文件`:不包含数据,但是提供了一个映射物理设备(如串口、键盘等)到一个文件名的机制.可通过设备文件访问外围设备.  
`管道`:管道是进程间通讯的一个基础设施.管道缓存了其输入端所接收的数据,以便在管道输出端读的进程能一个先进先出的方式来接受数据.  
在lab8中关注的主要是SFS支持的常规文件、目录和链接中的hardlink(硬链接)的设计实现.SFS文件系统中目录和常规文件具有共同的属性,而这些属性保存在索引节点中.SFS通过索引节点来管理目录和常规文件,索引节点包含操作系统所需要的关于某个文件的关键信息,比如文件的属性、访问许可权以及其它控制信息都保存在索引节点中.可以有多个文件名可指向一个索引节点.

### 文件系统的布局  
文件系统通常保存在磁盘上.`在本实验中,第三个磁盘(即disk0,前两个磁盘分别是ucore.img和swap.img)用于存放一个SFS文件系统(Simple Filesystem)`.通常文件系统中,磁盘的使用是以扇区(Sector)为单位的,但是为了实现简便,SFS中以block(4K,与内存page大小相等)为基本单位.SFS文件系统的布局如下图所示:  
![ucore文件系统布局](./memo-pic/ucore文件系统布局.png)
`第0个块(4K)是超级块(superblock),它包含了关于文件系统的所有关键参数`,当计算机被启动或文件系统被首次接触时,超级块的内容就会被装入内存.其定义如下(见kern/fs/sfs/sfs.h):
```
struct sfs_super {
    uint32_t magic;                                 /* magic number, should be SFS_MAGIC */
    uint32_t blocks;                                /* # of blocks in fs */
    uint32_t unused_blocks;                         /* # of unused blocks in fs */
    char info[SFS_MAX_INFO_LEN + 1];                /* infomation for sfs  */
};
```
sfs_super包含一个成员变量魔数magic,其值为0x2f8dbe2a,内核通过它来检查磁盘镜像是否是合法的SFS img;成员变量blocks记录了SFS中所有block的数量,即img的大小;成员变量unused_block记录了SFS中还没有被使用的block的数量;成员变量info包含了字符串"simple file system".  

第1个块放了一个root-dir的inode,用来记录根目录的相关信息.有关inode还将在后续部分介绍.这里只要理解`root-dir是SFS文件系统的根结点`,通过这个root-dir的inode信息就可以定位并查找到根目录下的所有文件信息.  

从第2个块开始,根据SFS中所有块的数量,用1个bit来表示一个块的占用和未被占用的情况(即位图).这个区域称为SFS的freemap区域，这将占用若干个块空间.为了更好地记录和管理freemap区域,专门提供了两个文件kern/fs/sfs/bitmap.[ch]来完成根据一个块号查找或设置对应的bit位的值.  

最后在剩余的磁盘空间中,存放了所有其他目录和文件的inode信息和内容数据信息.需要注意的是虽然inode的大小小于一个块的大小(4096B),但为了实现简单,每个inode都占用一个完整的block.  

在sfs_fs.c(kern/fs/)文件中的sfs_do_mount函数中,完成了加载位于硬盘上的SFS文件系统的超级块superblock和freemap的工作.这样,在内存中就有了SFS文件系统的全局信息.

### 索引节点
- **概述**  
在SFS文件系统中,需要记录文件内容的存储位置以及文件名与文件内容的对应关系.sfs_disk_inode记录了文件或目录的内容存储的索引信息,该数据结构在硬盘里储存,需要时读入内存.sfs_disk_entry表示一个目录中的一个文件或目录,包含该项所对应inode的位置和文件名,同样也在硬盘里储存,需要时读入内存.  

- **磁盘索引节点**  
SFS中的磁盘索引节点代表了一个实际位于磁盘上的文件.在硬盘上的索引节点的内容如下(见kern/fs/sfs/sfs.h):  
```
/* inode (on disk) */
struct sfs_disk_inode {
    uint32_t size;                   // 如果inode表示常规文件,则size是文件大小
    uint16_t type;                   // inode的文件类型
    uint16_t nlinks;                 // 此inode的硬连接数
    uint32_t blocks;                 // 此inode对应数据块的个数
    uint32_t direct[SFS_NDIRECT];    // 此inode的直接数据块索引值(有SFS_NDIRECT个)
    uint32_t indirect;               // 此inode的一级间接数据块索引值
//    uint32_t db_indirect;          /* double indirect blocks */
//   unused
};
```
通过上表可以看出:如果inode表示的是文件,则成员变量direct[]直接指向了保存文件内容数据的数据块索引值.indirect间接指向了保存文件内容数据的数据块,此数据块实际存放的全部是数据块索引,这些数据块索引指向的数据块才被用来存放文件内容数据.  

默认的,ucore里SFS_NDIRECT是12,即直接索引的数据页大小为12 * 4k=48k;当使用一级间接数据块索引时,ucore支持最大的文件大小为12*4k+1024*4k=48k+4m.数据索引表内,0表示一个无效的索引,inode里blocks表示该文件或者目录占用的磁盘的block的个数.indiret为0时,表示不使用一级索引块.(因为 block0用来保存super block,它不可能被其他任何文件或目录使用,所以这么设计也是合理的).  

对于普通文件,索引值指向的block中保存的是文件中的数据.而对于目录,索引值指向的数据保存的是目录下所有的文件名以及对应的索引节点所在的索引块(磁盘块)所形成的数组.数据结构如下(见kern/fs/sfs/sfs.h):  
```
struct sfs_disk_entry {
    uint32_t ino;               // 目录项对应的文件/子目录索引节点所占数据块索引值(ucore直接取磁盘块号)
    char name[SFS_MAX_FNAME_LEN + 1];    // 文件名
};
```
操作系统中,每个文件系统下的inode都应该分配唯一的inode编号.SFS下,为了实现的简便(偷懒),每个inode直接用他所在的磁盘 block的编号作为inode编号.比如:root block的inode编号为1;  每个sfs_disk_entry数据结构中,name表示目录下文件或文件夹的名称,ino表示磁盘block编号,通过读取该block的数据,能够得到相应的文件或文件夹的inode.ino为0时,表示一个无效的entry.和inode相似,每个sfs_disk_entry也占用一个block.

- **内存中的索引节点**  
```
struct sfs_inode {
    struct sfs_disk_inode *din;                     /* on-disk inode */
    uint32_t ino;                                   /* inode number */
    bool dirty;                                     /* true if inode modified */
    int reclaim_count;                              /* kill inode if it hits zero */
    semaphore_t sem;                                /* semaphore for din */
    list_entry_t inode_link;                        /* entry for linked-list in sfs_fs */
    list_entry_t hash_link;                         /* entry for hash linked-list in sfs_fs */
};
```
可以看到SFS中的内存inode包含了SFS的硬盘inode信息,而且还增加了其他一些信息,这便于进行是判断否改写、互斥操作、回收和快速地定位等.需要注意,`一个内存inode是在打开一个文件后才创建的`,如果关机则相关信息都会消失.`而硬盘inode的内容是保存在硬盘中的`,只是在进程需要时才被读入到内存中,用于访问文件或目录的具体内容数据.  

为了方便实现上面提到的多级数据的访问以及目录中entry的操作,对inode SFS实现了一些辅助的函数(见面kern/fs/sfs/sfs_inode.c):
```
sfs_bmap_load_nolock()
sfs_bmap_truncate_nolock()
sfs_dirent_read_nolock()
sfs_dirent_search_nolock
```
注:这些后缀为nolock的函数,只能在已经获得相应inode的semaphore才能调用.

- **inode的文件操作函数**  
详见kern/fs/sfs/sfs_inode.h
```
static const struct inode_ops sfs_node_fileops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_openfile,
    .vop_close                      = sfs_close,
    .vop_read                       = sfs_read,
    .vop_write                      = sfs_write,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_tryseek                    = sfs_tryseek,
    .vop_truncate                   = sfs_truncfile,
};

```
上述sfs_openfile、sfs_close、sfs_read和sfs_write分别对应用户进程发出的open、close、read、write操作.其中sfs_openfile不用做什么事;sfs_close需要把对文件的修改内容写回到硬盘上,这样确保硬盘上的文件内容数据是最新的;sfs_read和sfs_write函数都调用了一个函数sfs_io,并最终通过访问硬盘驱动来完成对文件内容数据的读写. 

- **inode的目录操作函数**  
```
static const struct inode_ops sfs_node_dirops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_opendir,
    .vop_close                      = sfs_close,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_namefile                   = sfs_namefile,
    .vop_getdirentry                = sfs_getdirentry,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_lookup                     = sfs_lookup,
};
```
对于目录操作而言,由于目录也是一种文件,所以sfs_opendir、sys_close对应户进程发出的open、close函数.相对于sfs_open,sfs_opendir只是完成一些open函数传递的参数判断,没做其他更多的事情.目录的close操作与文件的close操作完全一致.由于目录的内容数据与文件的内容数据不同,所以读出目录的内容数据的函数是sfs_getdirentry,其主要工作是获取目录下的文件inode信息.

### SFS与VFS的衔接
详见kern/fs/sfs/sfs_inode.c
```
static const struct inode_ops sfs_node_dirops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_opendir,
    .vop_close                      = sfs_close,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_namefile                   = sfs_namefile,
    .vop_getdirentry                = sfs_getdirentry,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_lookup                     = sfs_lookup,
};
/// The sfs specific FILE operations correspond to the abstract operations on a inode.
static const struct inode_ops sfs_node_fileops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_openfile,
    .vop_close                      = sfs_close,
    .vop_read                       = sfs_read,
    .vop_write                      = sfs_write,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_tryseek                    = sfs_tryseek,
    .vop_truncate                   = sfs_truncfile,
};
```
inode_ops是VFS层定义的操作接口,通过将其函数指针设置为SFS层的函数,从而在VFS层对SFS层的调用时,屏蔽了下层...

## 4.设备层文件IO(layer1)
为了统一地访问设备,我们可以把一个设备看成一个文件,通过访问文件的接口来访问设备.目前实现了stdin设备文件文件、stdout设备文件、disk0设备.stdin设备就是键盘,stdout设备就是CONSOLE(串口、并口和文本显示器),而disk0设备是承载SFS文件系统的磁盘设备.下面我们逐一分析ucore是如何让用户把设备看成文件来访问.
### 关键数据结构
为了表示一个设备,需要有对应的数据结构,ucore为此定义了struct device,其描述如下(见kern/fs/dev/dev.h):
```
struct device {
    size_t d_blocks;                                                // 设备占用的数据块的个数
    size_t d_blocksize;                                             // 数据块大小
    int (*d_open)(struct device *dev, uint32_t open_flags);         // 打开设备的函数指针...
    int (*d_close)(struct device *dev);                             
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_ioctl)(struct device *dev, int op, void *data);
};
```
这个数据结构能够支持对块设备(比如磁盘)、字符设备(比如键盘、串口)的表示,完成对设备的基本操作.ucore虚拟文件系统为了把这些设备链接在一起,还定义了一个设备链表,即双向链表`vdev_list`(kern/fs/vsf/vfsdev.c),这样通过访问此链表,可以找到ucore能够访问的所有设备文件.  

device设备描述没有与文件系统以及表示一个文件的inode数据结构建立关系,还需要另外一个数据结构把device和inode联通起来,这就是vfs_dev_t数据结构(kern/fs/vsf/vfsdev.c):  
```
typedef struct {
    const char *devname;
    struct inode *devnode;
    struct fs *fs;
    bool mountable;
    list_entry_t vdev_link;
} vfs_dev_t;
```
利用vfs_dev_t数据结构,就可以让文件系统通过一个链接vfs_dev_t结构的双向链表找到device对应的inode数据结构,一个inode节点的成员变量in_type的值是0x1234,则此inode的成员变量in_info将成为一个device结构.这样inode就和一个设备建立了联系,这个inode就是一个设备文件.  
```
总结:所有设备通过vfs_dev_t结点链接起来(构成双向链表vdev_list),且通过这个结点,可找到此设备对应的设备文件inode(设备模型化为文件)
```


### stdout设备文件
- **调用链**  
stdout设备是设备文件系统的文件(设备模型化为文件!),自然有自己的inode结构.在系统初始化时,即只需如下处理过程:
```
kern_init => fs_init => dev_init => dev_init_stdout => 
dev_create_inode => stdout_device_init => vfs_add_dev
```
在dev_init_stdout中完成了对stdout设备文件的初始化.详见kern/fs/devs/dev_stdout.c,必看!!!

- **stdout初始化**
stdout设备文件的初始化过程主要由stdout_device_init(kern/fs/devs/dev_stdout.c)完成,其具体实现如下:
```
static void stdout_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}
```
可以看到,stdout_open函数完成设备文件打开工作,如果发现用户进程调用open函数的参数flags不是只写(O_WRONLY),则会报错.这里的stdout设备文件实际上就是指的console外设(它其实是串口、并口和CGA的组合型外设).这个设备文件是一个只写设备,如果读这个设备,就会出错.

- **访问操作实现**  
stdout_io函数完成设备的写操作工作,具体实现如下(kern/fs/devs/dev_stdout.c):
```
static int
stdout_io(struct device *dev, struct iobuf *iob, bool write) {
    if (write) {
        char *data = iob->io_base;
        for (; iob->io_resid != 0; iob->io_resid --) {
            cputchar(*data ++);
        }
        return 0;
    }
    return -E_INVAL;
}
```
要写的数据放在iob->io_base所指的内存区域,一直写到iob->io_resid的值为0为止.每次写操作都是通过`cputchar`(重点关注)来完成的,此函数最终将通过console外设驱动来完成把数据输出到串口、并口和CGA显示器上过程.另外,也可以注意到,如果用户想执行读操作,则stdout_io函数直接返回错误值-E_INVAL.

### stdin设备文件
- **初始化**  
stdin设备文件实际上就是指的键盘,这个设备文件是一个只读设备,如果写这个设备,就会出错.stdin设备文件的初始化过程主要由stdin_device_init完成了主要的初始化工作,具体实现如下(kern/fs/devs/dev_stdin.c):
```
static void
stdin_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    p_rpos = p_wpos = 0;
    wait_queue_init(wait_queue);
}
```
相对于stdout的初始化过程,stdin的初始化相对复杂一些,多了一个stdin_buffer缓冲区,描述缓冲区读写位置的变量p_rpos、p_wpos以及用于等待缓冲区的等待队列wait_queue.在stdin_device_init函数的初始化中,也完成了对p_rpos、p_wpos和wait_queue的初始化.  
注意stdin与stdout的区别:`stdin是可读可写的,有自己的缓冲区!!! 而stdout仅可写`

- **访问操作实现**  
stdin_io函数负责完成设备的读操作工作，具体实现如下(kern/fs/devs/dev_stdin.c)：
```
static int
stdin_io(struct device *dev, struct iobuf *iob, bool write) {
    if (!write) {
        int ret;
        if ((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0) {
            iob->io_resid -= ret;
        }
        return ret;
    }
    return -E_INVAL;
}
```
如果是写操作,则stdin_io函数直接报错返回.如果是读操作,则此函数进一步调用dev_stdin_read函数完成对键盘设备的读入操作.dev_stdin_read函数的实现相对复杂一些,主要的流程如下:
```
static int
dev_stdin_read(char *buf, size_t len) {
    int ret = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        for (; ret < len; ret ++, p_rpos ++) {
        try_again:
            if (p_rpos < p_wpos) {
                *buf ++ = stdin_buffer[p_rpos % stdin_BUFSIZE];
            }
            else {
                wait_t __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                local_intr_restore(intr_flag);

                schedule();

                local_intr_save(intr_flag);
                wait_current_del(wait_queue, wait);
                if (wait->wakeup_flags == WT_KBD) {
                    goto try_again;
                }
                break;
            }
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}
```
如果p_rpos < p_wpos,则表示有键盘输入的新字符在stdin_buffer中,于是就从stdin_buffer中取出新字符放到iobuf指向的缓冲区中;如果p_rpos >=p_wpos,则表明没有新字符,这样调用read用户态库函数的用户进程就需要采用等待队列的睡眠操作进入睡眠状态,等待键盘输入字符的产生.  

键盘输入字符后,如何唤醒等待键盘输入的用户进程呢?回顾lab1中的外设中断处理,`当用户敲击键盘时,会产生键盘中断,在trap_dispatch函数中,当识别出中断是键盘中断`(中断号为IRQ_OFFSET + IRQ_KBD)时,会调用dev_stdin_write函数,来把字符写入到stdin_buffer中,且会通过等待队列的唤醒操作唤醒正在等待键盘输入的用户进程. 

### disk0设备文件
...略,详见kern/fs/devs/dev_disk0.c


### 设备层文件IO与SFS的衔接
???待完成
### 设备层文件IO与驱动程序的衔接(以stdout_io为例)
设备文件层初始化device(kern/fs/devs/dev_stdout.c):
```
static void stdout_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}
```
通过这些函数最终调用了驱动程序,完成真正的访问设备,以stdout_io为例:
调用链(从dev_stdout.c开始看):
```
stdout_io() => cputchar() =>
cons_putc() => lpt_putc/cga_putc/serial_putc => lpt_putc_sub() // 这一行调用就是设备驱动程序
```
以将字符输出到并口为例,最终调用的驱动程序是lpt_puts_sub函数(kern/driver/console.c):
```
static void lpt_putc_sub(int c) {
    int i;
    for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i ++) {
        delay();
    }
    outb(LPTPORT + 0, c);
    outb(LPTPORT + 2, 0x08 | 0x04 | 0x01);
    outb(LPTPORT + 2, 0x08);
}
```
可见,它是将字符写到设备对应的端口(寄存器)完成输出!
注意:`对于stdin而言,与stdout有较大差别 => stdin有一个缓冲区暂存外设的数据`,详见dev_stdin.c

# 补充:实验执行流程概述
与实验七相比,实验八增加了文件系统,并因此实现了通过文件系统来加载可执行文件到内存中运行的功能,导致对进程管理相关的实现比较大的调整......  
首先看kern_init函数,可以发现与lab7相比增加了对fs_init函数的调用.fs_init函数就是文件系统初始化的总控函数,它进一步调用了虚拟文件系统初始化函数vfs_init,与文件相关的设备初始化函数dev_init和Simple FS文件系统的初始化函数sfs_init.这三个初始化函数联合在一起,协同完成了整个虚拟文件系统、SFS文件系统和文件系统对应的设备(键盘、串口、磁盘)的初始化工作.其函数调用关系图如下所示:
![ucore文件系统调用关系](fs调用关系.png)
vfs_init主要建立了一个device list双向链表vdev_list,为后续具体设备(键盘、串口、磁盘)以文件的形式呈现建立查找访问通道.dev_init函数通过进一步调用disk0/stdin/stdout_device_init完成对具体设备的初始化,把它们抽象成一个设备文件,并建立对应的inode数据结构,最后把它们链入到vdev_list中.这样通过虚拟文件系统就可以方便地以文件的形式访问这些设备了.sfs_init是完成对Simple FS的初始化工作,并把此实例文件系统挂在虚拟文件系统中,从而让ucore的其他部分能够通过访问虚拟文件系统的接口来进一步访问到SFS实例文件系统

# 补充:文件操作实现
## 打开文件
- **概述**  
假定用户进程需要打开的文件已经存在在硬盘上.以user/sfs_filetest1.c为例,首先用户进程会调用在main函数中的如下语句:
```
int fd1 = safe_open("sfs_filetest1", O_RDONLY);
```
如果ucore能够正常查找到这个文件,就会返回一个代表文件的文件描述符fd1,这样在接下来的读写文件过程中,就直接用这样fd1来代表就可以了.那这个打开文件的过程是如何一步一步实现的呢?

- **通用文件访问接口层的处理流程**  
首先进入通用文件访问接口层的处理流程,即`进一步调用如下用户态函数:open->sys_open->syscall`(都位于user/libs/),从而引起系统调用进入到内核态.到了内核态后,`通过中断处理例程`,会调用到sys_open(kern/syscall/syscall.c)内核函数,并进一步调用sysfile_open(kern/fs/sysfile.c)内核函数.到了这里,需要把位于用户空间的字符串"sfs_filetest1"拷贝到内核空间中的字符串path中,并进入到文件系统抽象层的处理流程完成进一步的打开文件操作中  

- **文件系统抽象层的处理流程**  
通用层的sysfile_open函数调用file_open(kern/fs/file.c)函数,它要给这个即将打开的文件分配一个file数据结构的变量,这个变量其实是当前进程的打开文件数组current->fs_struct->filemap[]中的一个空闲元素(即还没用于一个打开的文件),而这个元素的索引值就是最终要返回到用户进程并赋值给变量fd1.到了这一步还仅仅是给当前用户进程分配了一个file数据结构的变量,还没有找到对应的文件索引节点.  
为此需要进一步调用`vfs_open`(kern/fs/vfs/vfsfile.c)函数来找到path指出的文件所对应的基于inode数据结构的VFS索引节点node.vfs_open函数需要完成两件事情:通过`vfs_lookup`找到path对应文件的inode;调用`vop_open`函数打开文件.后面详细的步骤如下:  
1.找到文件设备的根目录"/"的索引节点需要注意,这里的vfs_lookup函数是一个针对目录的操作函数,它会调用vop_lookup函数来找到SFS文件系统中的"/"目录下的"sfs_filetest1"文件.为此,vfs_lookup函数首先调用get_device函数,并进一步调用vfs_get_bootfs函数(其实调用了)来找到根目录"/"对应的inode.这个inode就是位于vfs.c中的inode变量bootfs_node.这个变量在init_main函数(位于kern/process/proc.c)执行时获得了赋值.  
2.通过调用vop_lookup函数来查找到根目录"/"下对应文件sfs_filetest1的索引节点,如果找到就返回此索引节点.  
3.把file和node建立联系.  
完成第3步后,将返回到file_open(kern/fs/file.c)函数中,通过执行语句"file->node=node;",就把当前进程的current->fs_struct->filemap[fd](即file所指变量)的成员变量node指针指向了代表sfs_filetest1文件的索引节点inode.这时返回fd.经过重重回退,通过系统调用返回,用户态的syscall->sys_open->open->safe_open等用户函数的层层函数返回,最终把把fd赋值给fd1.自此完成了打开文件操作.但这里我们还没有分析第2和第3步是如何进一步调用SFS文件系统提供的函数找位于SFS文件系统上的sfs_filetest1文件所对应的sfs磁盘inode的过程

- **SFS文件系统层的处理流程**  
这里需要分析文件系统抽象层中没有彻底分析的vop_lookup(位于kern/fs/vfs/inode.h,但其它是sfs层sfs_lookup的包装)函数到底做了啥.在sfs_inode.c(kern/fs/sfs/)中的sfs_node_dirops变量定义了".vop_lookup = sfs_lookup",所以我们重点分析sfs_lookup的实现.注意:在lab8中,为简化代码,sfs_lookup函数中并没有实现能够对多级目录进行查找的控制逻辑(在ucore_plus中有实现).  
`sfs_lookup`(kern/fs/sfs/sfs_inode.c)有三个参数:node、path、node_store.其中node是根目录"/"所对应的inode节点;path是文件sfs_filetest1的绝对路径/sfs_filetest1,而node_store是经过查找获得的sfs_filetest1所对应的inode节点;  sfs_lookup函数以"/"为分割符,从左至右逐一分解path获得各个子目录和最终文件对应的inode节点.在本例中是调用sfs_lookup_once查找以根目录下的文件sfs_filetest1所对应的inode节点.当无法分解path后,就意味着找到了sfs_filetest1对应的inode节点,就可顺利返回了.  
当然这里讲得还比较简单,sfs_lookup_once将调用sfs_dirent_search_nolock函数来查找与路径名匹配的目录项,如果找到目录项,则根据目录项中记录的inode所处的数据块索引值找到路径名对应的SFS磁盘inode,并读入SFS磁盘inode对的内容,创建SFS内存inode


## 读文件
- **概述**  
读文件其实就是读出目录中的目录项,首先假定文件在磁盘上且已经打开.用户进程有如下语句:
```
read(fd, data, len);
```
即读取fd对应文件,读取长度为len,存入data中.下面来分析一下读文件的实现.

- **通用文件访问接口层的处理流程**  
即调用如下用户态函数:read->sys_read->syscall(都位于user/libs/),从而引起系统调用进入到内核态.到了内核态以后,通过中断处理例程,会调用到sys_read内核函数,并进一步调用sysfile_read内核函数,进入到文件系统抽象层处理流程完成进一步读文件的操作.  

- **文件抽象层的处理流程**  
1) 检查错误,即检查读取长度是否为0和文件是否可读.  

2) 分配buffer空间,即调用kmalloc函数分配4096字节的buffer空间.  

3) 读文件过程  
3.1)实际读文件  
循环读取文件,每次读取buffer大小.每次循环中,先检查剩余部分大小,若其小于4096字节,则只读取剩余部分的大小.然后调用file_read函数(详细分析见后)将文件内容读取到buffer中,alen为实际大小.调用copy_to_user函数将读到的内容拷贝到用户的内存空间中,调整各变量以进行下一次循环读取,直至指定长度读取完成.最后函数调用层层返回至用户程序,用户程序收到了读到的文件内容.  
3.2)file_read函数  
这个函数是读文件的核心函数.函数有4个参数,fd是文件描述符,base是缓存的基地址,len是要读取的长度,copied_store存放实际读取的长度.函数首先调用fd2file函数找到对应的file结构,并检查是否可读.调用filemap_acquire函数使打开这个文件的计数加1.调用vop_read函数将文件内容读到iob中(详细分析见后).调整文件指针偏移量pos的值,使其向后移动实际读到的字节数iobuf_used(iob).最后调用filemap_release函数使打开这个文件的计数减1,若打开计数为0,则释放file.  

- **SFS文件系统层的处理流程**  
vop_read函数实际上是对sfs_read的包装.在sfs_inode.c中sfs_node_fileops变量定义了.vop_read = sfs_read,所以下面来分析sfs_read函数的实现.  
sfs_read函数调用sfs_io函数.它有三个参数,node是对应文件的inode,iob是缓存,write表示是读还是写的布尔值(0表示读,1表示写),这里是0.函数先找到inode对应sfs和sin,然后调用sfs_io_nolock函数进行读取文件操作,最后调用iobuf_skip函数调整iobuf的指针.  
在sfs_io_nolock函数中,先计算一些辅助变量,并处理一些特殊情况(比如越界),然后有sfs_buf_op = sfs_rbuf,sfs_block_op = sfs_rblock,设置读取的函数操作.接着进行实际操作,先处理起始的没有对齐到块的部分,再以块为单位循环处理中间的部分,最后处理末尾剩余的部分.每部分中都调用sfs_bmap_load_nolock函数得到blkno对应的inode编号.并调用sfs_rbuf或sfs_rblock函数读取数据(中间部分调用sfs_rblock,起始和末尾部分调用sfs_rbuf),调整相关变量.完成后如果offset + alen > din->fileinfo.size(写文件时会出现这种情况,读文件时不会出现这种情况,alen为实际读写的长度),则调整文件大小为offset + alen并设置dirty变量.  
sfs_bmap_load_nolock函数将对应sfs_inode的第index个索引指向的block的索引值取出存到相应的指针指向的单元(ino_store).它调用sfs_bmap_get_nolock来完成相应的操作.sfs_rbuf和sfs_rblock函数最终都调用sfs_rwblock_nolock函数完成操作,而sfs_rwblock_nolock函数调用dop_io->disk0_io->disk0_read_blks_nolock->ide_read_secs完成对磁盘的操作.



# 相关细节/问题概览

- **vnode与inode的关系?**  
1.ucore中没有使用vnode命名.  
2.参考网上部分说法:  
传统的Unix既有v节点(vnode)也有i节点(inode),vnode的数据结构中包含了inode信息.但在Linux中没有使用vnode,使用了通用inode;   vnode("virtual node")仅在文件打开的时候才出现的,而inode定位文件在磁盘的位置,它的信息本身是存储在磁盘等上的,当打开文件的时候从磁盘上读入内存. => 从这个角度看sfs_inode就是vnode,sfs_disk_inode(都在sfs.h)就是inode  
3.不过,ucore在VFS层有一个inode结构体,它可以看做是sfs_inode的包装,与2中所说的inode含义不同!


- **如何理解目录是一个特殊的文件?**  
目录也是一个普通文件,只是目录中存储的数据比较特殊,它存储的是以下格式的数组:  
```
struct sfs_disk_entry {
    uint32_t ino;               // 目录项对应的文件/子目录索引节点所占数据块索引值(ucore直接取磁盘块号)
    char name[SFS_MAX_FNAME_LEN + 1];    // 文件名
};
```
目录文件中,每一个这样的结构就是目录项,目录项的成员ino指向了子目录的索引块

- **ucore的文件系统分为了四层,那么fs目录下那些代码(devs、sfs、vfs之外)属于哪一层?**  
这部分代码最好仍然看做vfs层(不过它也有一些SFS层调用的函数,只能说ucore在分层上没有搞清晰)  


- **以read(fd,base,len)系统调用为例,陷入内核的过程是怎样的?**  
详见上文的讲解"VFS与通用文件系统访问接口的衔接"  

- **文件系统是如何布局的?**  
文件系统位于磁盘上,按磁盘逻辑地址由小到大,依次存放如下内容:  
```
|super block| root-dir inode| freemap | (inode)/(file data)/(dir data) blocks|
```
超块用于记录文件系统信息
超块之后紧接着的一个block是root-dir inode,用于记录根目录相关信息
freemap是所有block的位图,相关操作见bitmap.[ch]

- **通过ucore理解挂载mount**  
挂载其实就是:`加载位于硬盘上的文件系统的超级块superblock和freemap`的工作.这样,在内存中就有了文件系统的全局信息.详见sfs_fs.c/sfs_do_mount()函数  

- **理解磁盘IO以block为读写单位(重要)**  
由于磁盘IO以block为单位,所以要写的数据不足1个block时,必须先将整个block调入内存,然后修改部分数据,最后再将整个block写回磁盘(见fs/sfs/sfs_io.c); => 注意这个过程中,需要使用一个额外的缓冲区sfs->sfs_buffer
```
// 向一个block写数据
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset) {
    assert(offset >= 0 && offset < SFS_BLKSIZE && offset + len <= SFS_BLKSIZE);
    int ret;
    lock_sfs_io(sfs);
    {   
        // 1.先将要写的那个磁盘块读取到内存缓冲区(读到sfs对应的缓冲区sfs_buffer)
        if ((ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 0, 1)) == 0) {
            // 2.将要写的数据buf写入sfs_buffer
            memcpy(sfs->sfs_buffer + offset, buf, len);

            // 3.再将sfs_buffer写入到磁盘
            ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 1, 1);
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}
```