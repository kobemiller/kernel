/*
 * linux/mm/memory.c
 *
 * (c) 1991 linus torvalds
 */
/*
 * demand-loading started 01.12.91 - seems it is high os the list 
 * of things wanted , and it should be easy to implement. - linus
 */
/*
 * ok,demand-loading was easy, shared pages a little bit tricker.shared
 * pages started 02.12.91,seems to work. - linus.
 *
 * tested sharing by executing about 30 /bin/sh: under the old kernel it 
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * also corrected some "invalidate()"s - I wasn't doing enough of them.
 */
/*
 * real VM(paging to/from disk) started 18.12.91. much more work and 
 * thought has to go into this. oh, well..
 * 19.12.91 - works, somewhat. sometimes I get faults, don't konw why.
 *              found it. everything seems to work now.
 * 20.12.91 - ok, making the swap-device changeable like the root.
 */

#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < current->start_code + current->end_code)

unsigned long HIGH_MEMORY = 0;

#define copy_page(from, to) __asm__("cld; rep; movsl" ::"S" (from), "D" (to), "c" (1024):"cx", "di", "si" )

unsigned char mem_map[PAGING_PAGES] = {0,};
/*
 * free a page of memory at physical address 'addr'.
 * used by 'free_page_tables()'
 */
void free_page(unsigned long addr)
{
    if ( addr < LOW_MEM )   
        return;
    if ( addr >= HIGH_MEMORY )
        panic("trying to free nonexistent page");
    addr -= LOW_MEM;
    addr >>= 12;
    if ( mem_map[addr]-- )
        return;
    mem_map[addr] = 0;
    panic("trying to free free page");
}

/*
 * this function frees a continous block of page tables, as needed
 * by 'exit()'. as does copy_page_tables(), this handles only 4Mb blocks. 
 */
int free_page_tables(unsigned long from, unsigned long size)
{
    unsigned long *pg_table;
    unsigned long *dir, nr;

    if ( from & 0x3fffff )
        panic("free_page_tables called with wrong alignment");
    if ( !from )
        panic("trying to free up swapper memory space");
    size = (size + 0x3fffff) >>22;
    dir = (unsigned long *) ((from >> 20) & 0xffc);
    for ( ; size --> 0; dir++ )
    {
        if ( !(1 & *dir) )
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir);
        for ( nr = 0; nr < 1024; nr++ )
        {
            if ( *pg_table )
            {
                if ( 1 & *pg_table )
                    free_page(0xfffff000 & *pg_table);
                else 
                    swap_free(*pg_table >> 1);
                *pg_table = 0;
            }
            pg_table++;
        }
        free_page(0xfffff000 & *dir);
        *dir = 0;
    }
    invalidate();
    return 0;
}

/*
 * well , here is one fo the most complicated funcions in mm. it
 * copies a range of linerar addresses by copying only the pages,
 * let's hope this is bug-free, cause this one I don't want to debug
 *
 * note 2!! when from==0 we are copying kernel space for the first 
 * fork(). Then we DONT want to copy a full page_directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it doesn't take 
 * any more memory - we don't copy-on-write in the low 1 Mb-range, so the pages 
 * can be shared with the kernel. Thus the 
 * special case for nr =xxxx.
 */

