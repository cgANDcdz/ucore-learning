#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>
#include <kmalloc.h>
#include <sync.h>
#include <pmm.h>
#include <stdio.h>

/*
 * SLOB Allocator: Simple List Of Blocks
 *
 * Matt Mackall <mpm@selenic.com> 12/30/03
 *
 * How SLOB works:
 *
 * The core of SLOB is a traditional K&R style heap allocator, with
 * support for returning aligned objects. The granularity of this
 * allocator is 8 bytes on x86, though it's perhaps possible to reduce
 * this to 4 if it's deemed worth the effort. The slob heap is a
 * singly-linked list of pages from __get_free_page, grown on demand
 * and allocation from the heap is currently first-fit.
 *
 * Above this is an implementation of kmalloc/kfree. Blocks returned
 * from kmalloc are 8-byte aligned and prepended with a 8-byte header.
 * If kmalloc is asked for objects of PAGE_SIZE or larger, it calls
 * __get_free_pages directly so that it can return page-aligned blocks
 * and keeps a linked list of such pages and their orders. These
 * objects are detected in kfree() by their page alignment.
 *
 * SLAB is emulated on top of SLOB by simply calling constructors and
 * destructors for every SLAB allocation. Objects are returned with
 * the 8-byte alignment unless the SLAB_MUST_HWCACHE_ALIGN flag is
 * set, in which case the low-level allocator will fragment blocks to
 * create the proper alignment. Again, objects of page-size or greater
 * are allocated by calling __get_free_pages. As SLAB objects know
 * their size, no separate size bookkeeping is necessary and there is
 * essentially no allocation space overhead.
 */

/**********************************************************************************************
 * 										SLOB内存分配器
 * 0.为什么需要slob?
 * 	 => default_pmm.c中的first-fit算法是以page为单位分配的.但是分配对象时往往要不了那么多内存,
 * 		所以需要更细粒度的内存分配,slob就是在first-fit基础上的分配器...
 * 
 * 1.这里一个slob对应一个page(虽然对应多个page也行)
 * 
 * 2.ucore在实现上与linux的slob尚有一点区别:
 * 		完整的slob参考:https://lwn.net/Articles/157944/
 * 		对于这里的实现来说有参考价值的文档:http://www.linuxidc.com/Linux/2012-07/64107.htm
 * 
 * 3.ucore中分配的object(小对象,即下面的block)是8字节对齐的!
 * 
 * 4.这里大对象分配时类似于buddy系统,分配的页面数必须是2的n次方,且2^n-1 < size < 2^n,size是对象大小
 *   大对象分配时就是page对齐的,即从一个全新的page开始,没用完的page之后不会被小对象使用
 * 	 => 这样应该会造成较大的浪费;
 * 		大对象的分配是伙伴系统; 小对象的分配才是slot算法
 * 
 * 
 * ********************************************************************************************/

//some helper
#define spin_lock_irqsave(l, f) local_intr_save(f)		     // 关中断,EFLAGS中断屏蔽为保存到f中
#define spin_unlock_irqrestore(l, f) local_intr_restore(f)  // 开中断,从f恢复EFLAGS的中断屏蔽位
typedef unsigned int gfp_t;			// get free page
#ifndef PAGE_SIZE
#define PAGE_SIZE PGSIZE			// 4096
#endif

#ifndef L1_CACHE_BYTES
#define L1_CACHE_BYTES 64
#endif

#ifndef ALIGN
/**
 * 计算addr的size字节对齐的地址
 * addr通常是虚拟地址,size是2的次方
 * 举例:如果size是页面大小,那么ALIGN计算的就是addr所在页的起始地址
 */ 
#define ALIGN(addr,size)   (((addr)+(size)-1)&(~((size)-1))) 
#endif

/**
 * 1.这里一个block就是一个object之意(小于一个page)
 * 2.所有小于一个页的object/block用slob_block来描述
 * 3.一个slob(ucore占用一个page)包含若干object
 * 4.大于1个page的object/block用bigblock结构体描述
 */ 
struct slob_block {
	int units;				// 
	struct slob_block *next;
};
typedef struct slob_block slob_t;

#define SLOB_UNIT sizeof(slob_t)		// slob结点的字节数
#define SLOB_UNITS(size) (((size) + SLOB_UNIT - 1)/SLOB_UNIT)
#define SLOB_ALIGN L1_CACHE_BYTES      // slob按照8字节对齐

/**
 * 大于一个page的对象(object/block)
 */ 
struct bigblock {
	int order;			 // => 这个大对象占用的page数为=2^order
	void *pages;		 // 这个大对象起始虚拟地址(page对齐,即低12bit为0!)
	struct bigblock *next;
};
typedef struct bigblock bigblock_t;

// slobfree是小对象的空闲链表
static slob_t arena = { .next = &arena, .units = 1 };   // next指向自己,从而初始化为循环链表
static slob_t *slobfree = &arena;		// 空闲slob链表(小对象)

// 
static bigblock_t *bigblocks;


/**
 * 调用alloc_pages()分配2的order次方个物理页
 * 返回第一个页的起始虚拟地址
 * - gfp:没用上
 * - order:
 */ 
static void* __slob_get_free_pages(gfp_t gfp, int order)
{
  struct Page * page = alloc_pages(1 << order);
  if(!page)
    return NULL;
  return page2kva(page);
}

// 分配一个物理页,返回页面虚拟地址
#define __slob_get_free_page(gfp) __slob_get_free_pages(gfp, 0)


/**
 * 释放虚拟地址kva对应页面开始的2^order个页面
 * 且kva是page对齐的,即它是某个page的起始地址
 */ 
static inline void __slob_free_pages(unsigned long kva, int order)
{
  free_pages(kva2page(kva), 1 << order);
}

static void slob_free(void *b, int size);

/**
 * 分配slob?
 * - size:请求分配的内存大小(不大于1个page)
 * - gfp:没用上
 * - align:多少字节对齐
 */ 
static void *slob_alloc(size_t size, gfp_t gfp, int align)
{
  assert( (size + SLOB_UNIT) < PAGE_SIZE );

	slob_t *prev, *cur, *aligned = 0;
	int delta = 0, units = SLOB_UNITS(size);
	unsigned long flags;	// 用于保存中断标志位

	spin_lock_irqsave(&slob_lock, flags);
	prev = slobfree;
	// 从空闲slob链表中找
	for (cur = prev->next; ; prev = cur, cur = cur->next) {
		if (align) {
			aligned = (slob_t *)ALIGN((unsigned long)cur, align);
			delta = aligned - cur;
		}
		if (cur->units >= units + delta) { /* room enough? */
			if (delta) { /* need to fragment head to align? */
				aligned->units = cur->units - delta;
				aligned->next = cur->next;
				cur->next = aligned;
				cur->units = delta;
				prev = cur;
				cur = aligned;
			}

			if (cur->units == units) /* exact fit? */
				prev->next = cur->next; /* unlink */
			else { /* fragment */
				prev->next = cur + units;
				prev->next->units = cur->units - units;
				prev->next->next = cur->next;
				cur->units = units;
			}

			slobfree = prev;
			spin_unlock_irqrestore(&slob_lock, flags);
			return cur;
		}
		if (cur == slobfree) {
			spin_unlock_irqrestore(&slob_lock, flags);

			if (size == PAGE_SIZE) /* trying to shrink arena? */
				return 0;

			cur = (slob_t *)__slob_get_free_page(gfp);
			if (!cur)
				return 0;

			slob_free(cur, PAGE_SIZE);
			spin_lock_irqsave(&slob_lock, flags);
			cur = slobfree;
		}
	}
}

static void slob_free(void *block, int size)
{
	slob_t *cur, *b = (slob_t *)block;
	unsigned long flags;

	if (!block)
		return;

	if (size)
		b->units = SLOB_UNITS(size);

	/* Find reinsertion point */
	spin_lock_irqsave(&slob_lock, flags);
	for (cur = slobfree; !(b > cur && b < cur->next); cur = cur->next)
		if (cur >= cur->next && (b > cur || b < cur->next))
			break;

	if (b + b->units == cur->next) {
		b->units += cur->next->units;
		b->next = cur->next->next;
	} else
		b->next = cur->next;

	if (cur + cur->units == b) {
		cur->units += b->units;
		cur->next = b->next;
	} else
		cur->next = b;

	slobfree = cur;

	spin_unlock_irqrestore(&slob_lock, flags);
}



void slob_init(void) {
  cprintf("use SLOB allocator\n");
}

inline void kmalloc_init(void) {
    slob_init();
    cprintf("kmalloc_init() succeeded!\n");
}

size_t slob_allocated(void) {
  return 0;
}

size_t kallocated(void) {
   return slob_allocated();
}

/**
 * 计算size字节要占2^order个的物理页
 * 返回order(order是指数)
 * 注:可见,这里大对象分配时类似于buddy系统,分配的页面数必须是2的n次方,且2^n-1 < size < 2^n
 */ 
static int find_order(int size)
{
	int order = 0;
	for ( ; size > 4096 ; size >>=1)  // >>= 表示右移并赋值
		order++;
	return order;
}

/**
 * 分配大小为size字节的物理内存(被kmalloc调用)
 * => 返回对应内存区域的起始虚拟地址va
 * - size:要分配的字节数
 * - gfp:
 */ 
static void *__kmalloc(size_t size, gfp_t gfp)
{
	slob_t *m;
	bigblock_t *bb;
	unsigned long flags;

	// 请求分配的内存小于1个page(- SLOB_UNIT是因为第一个slob作为头结点?)
	if (size < PAGE_SIZE - SLOB_UNIT) {
		m = slob_alloc(size + SLOB_UNIT, gfp, 0);
		return m ? (void *)(m + 1) : 0;
	}

	bb = slob_alloc(sizeof(bigblock_t), gfp, 0);
	if (!bb)
		return 0;

	bb->order = find_order(size);
	bb->pages = (void *)__slob_get_free_pages(gfp, bb->order);

	if (bb->pages) {
		spin_lock_irqsave(&block_lock, flags);
		bb->next = bigblocks;
		bigblocks = bb;
		spin_unlock_irqrestore(&block_lock, flags);
		return bb->pages;
	}

	slob_free(bb, sizeof(bigblock_t));
	return 0;
}

/**
 * 分配大小为size字节的物理内存
 * => 返回对应内存区域的起始虚拟地址va
 * (!!!内存分配的统一接口)
 * */
void *kmalloc(size_t size)
{
  return __kmalloc(size, 0);
}


/**
 * 释放虚拟地址block处的那个对象(object)
 */ 
void kfree(void *block)
{
	bigblock_t *bb, **last = &bigblocks;
	unsigned long flags;

	if (!block)
		return;

	// 1.如果虚拟地址block与page起始地址对齐(也就是说这个对象/ojbect起始地址恰好是某个page的开始)
	//   => 检查大对象链表,确定要释放的是否为大对象....
	// 解释:(PAGE_SIZE-1)的高20bit为0,低12bit为1; &结果为0则说明block的低12bit为0,所以block是的页起始地址
	if (!((unsigned long)block & (PAGE_SIZE-1))) {
		spin_lock_irqsave(&block_lock, flags);
		// might be on the big block list  => 遍历大对象链表
		for (bb = bigblocks; bb; last = &bb->next, bb = bb->next) {
			if (bb->pages == block) {		// 说明要释放的是某个大对象
				*last = bb->next;
				spin_unlock_irqrestore(&block_lock, flags);
				__slob_free_pages((unsigned long)block, bb->order);  // 释放大对象
				slob_free(bb, sizeof(bigblock_t));					 // 释放该大对象的描述结构??
				return;
			}
		}
		spin_unlock_irqrestore(&block_lock, flags);
	}

	// 2.控制流走到这里,释放的对象是小对象(无论起始地址是否与page对齐)
	slob_free((slob_t *)block - 1, 0);		// 释放小对象
	return;
}


unsigned int ksize(const void *block)
{
	bigblock_t *bb;
	unsigned long flags;

	if (!block)
		return 0;

	if (!((unsigned long)block & (PAGE_SIZE-1))) {
		spin_lock_irqsave(&block_lock, flags);
		for (bb = bigblocks; bb; bb = bb->next)
			if (bb->pages == block) {
				spin_unlock_irqrestore(&slob_lock, flags);
				return PAGE_SIZE << bb->order;
			}
		spin_unlock_irqrestore(&block_lock, flags);
	}

	return ((slob_t *)block - 1)->units * SLOB_UNIT;
}



