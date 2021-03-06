#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <defs.h>

/****************************************************************************
 *                  自定义ELF文件相关数据结构 
 * 多个节构成一个段
 * **************************************************************************/
/*************** --
 *    ELF头    *  |
 *   段头部表   *   
 *   .init     *代码段
 *   .text     *  |
 *   .rodata   * --
 *   .data     *数据段 
 *   .bss      * --
 *   .symtab   * .
 *   .debug    * .
 *   .line     * .
 *   .strtab   *
 *  节头部表    *
 * *************/

#define ELF_MAGIC   0x464C457FU         // "\x7FELF" in little endian

/* file header */
struct elfhdr {
    uint32_t e_magic;     // must equal ELF_MAGIC
    uint8_t e_elf[12];
    uint16_t e_type;      // 1=relocatable, 2=executable, 3=shared object, 4=core image
    uint16_t e_machine;   // 3=x86, 4=68K, etc.
    uint32_t e_version;   // file version, always 1
    uint32_t e_entry;     // entry point if executable  => (如果是可执行文件),这个字段指出程序入口!
    uint32_t e_phoff;     // 段头部表距离文件头的偏移
    uint32_t e_shoff;     // file position of section header or 0
    uint32_t e_flags;     // architecture-specific flags, usually 0
    uint16_t e_ehsize;    // size of this elf header
    uint16_t e_phentsize; // size of an entry in program header
    uint16_t e_phnum;     // 段的个数(即段头部表中有几个proghdr数组) 
    uint16_t e_shentsize; // size of an entry in section header
    uint16_t e_shnum;     // number of entries in section header or 0
    uint16_t e_shstrndx;  // section number that contains section name strings
};

/* program section header */
// proghdr对应段头部表中的一个条目; 段头部表就是一个proghdr数组
struct proghdr {
    uint32_t p_type;   // 段类型
    uint32_t p_offset; // 段相对ELF文件头的偏移值
    uint32_t p_va;     // 段的第一个字节的虚拟内存地址
    uint32_t p_pa;     // physical address, not used
    uint32_t p_filesz; // 此段在文件中的大小
    uint32_t p_memsz;  // 段在内存中占用的字节数 (bigger if contains bss)
    uint32_t p_flags;  // read/write/execute bits 
    uint32_t p_align;  // required alignment, invariably hardware page size
};

/* values for Proghdr::p_type */
#define ELF_PT_LOAD                     1

/* flag bits for Proghdr::p_flags */
#define ELF_PF_X                        1
#define ELF_PF_W                        2
#define ELF_PF_R                        4

#endif /* !__LIBS_ELF_H__ */

