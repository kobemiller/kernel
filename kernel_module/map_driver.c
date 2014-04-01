#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/semaphore.h>
#include <asm/pgtable.h>
#include <linux/mm_types.h>
#include <linux/sched.h>

struct mapdrvo
{
    struct cdev mapdevo;
    atomic_t usage;
};
#define MAPLEN (PAGE_SIZE * 10)

int mapdrvo_open(struct inode *inode, struct file *file);
int mapdrvo_release(struct inode *inode, struct file *file);
int mapdrvo_mmap(struct file *file, struct vm_area_struct *vma);
void map_vopen(struct vm_area_struct *vma);
void map_vclose(struct vm_area_struct *vma);
struct page *map_nopage(struct vm_area_struct *vma, unsigned long address, int *type);

static struct file_operations mapdrvo_fops = 
{
    .owner = THIS_MODULE,
    .mmap = mapdrvo_mmap,
    .open = mapdrvo_open,
    .release = mapdrvo_release,
};
static struct vm_operations_struct map_vm_ops = 
{
    .open = map_vopen,
    .close = map_vclose,
    //.nopage = map_nopage,
};
static int *vmalloc_area = NULL;
static int major;

volatile void *vaddr_to_kaddr(volatile void *address)
{
    pgd_t *pgd;
    pmd_t *pmd;
    pte_t *ptep, pte;

    unsigned long va, ret = 0UL;
    va = (unsigned long)address;
    pgd = pgd_offset_k(va);

    if ( !pgd_none(*pgd) )
    {
        pmd = pmd_offset((pud_t *)pgd, va);
        if ( !pmd_none(*pmd) )
        {
            ptep = pte_offset_kernel(pmd, va);
            pte  = *ptep;
            if ( pte_present(pte) )
            {
                ret = (unsigned long)page_address(pte_page(pte));
                ret |= (va & (PAGE_SIZE - 1));
            }
        }
    }
    return ((volatile void *)ret);
}

struct mapdrvo *md;
MODULE_LICENSE("GPL");

static int __init mapdrvo_init(void)
{
    unsigned long virt_addr;
    int result, err;
    dev_t dev = 0;
    dev = MKDEV(0, 0);
    major = MAJOR(dev);
    md = kmalloc(sizeof(struct mapdrvo), GFP_KERNEL);
    if ( !md )
        goto fail1;
    result = alloc_chrdev_region(&dev, 0, 1, "mapdrvo");
    if ( result < 0 )
    {
        printk(KERN_WARNING"mapdrvo: can't get major %d\n", major);
        goto fail2;
    }
    cdev_init(&md->mapdevo, &mapdrvo_fops);
    md->mapdevo.owner = THIS_MODULE;
    md->mapdevo.ops = &mapdrvo_fops;
    err = cdev_add(&md->mapdevo, dev, 1);
    if (err)
    {
        printk(KERN_NOTICE"error: %d adding mapdrvo", err);
        goto fail3;
    }
    atomic_set(&md->usage, 0);

    vmalloc_area = vmalloc(MAPLEN);
    if ( !vmalloc_area )
        goto fail4;
    for ( virt_addr = (unsigned long)vmalloc_area;
            virt_addr < (unsigned long)(&(vmalloc_area[MAPLEN / sizeof(int)]));
            virt_addr += PAGE_SIZE )
    {
        SetPageReserved(virt_to_page(vaddr_to_kaddr((void *)virt_addr)));
    }
    strcpy((char *)vmalloc_area, "hello world from kernel space!");
    printk("vmalloc_area at 0x%p(phys 0x%lx)\n", vmalloc_area, 
            (unsigned long)virt_to_phys((void *)vaddr_to_kaddr(vmalloc_area)));
    return 0;
fail4:
    cdev_del(&md->mapdevo);
fail3:
    unregister_chrdev_region(dev, 1);
fail2:
    kfree(md);
fail1:
    return -1;
}

static void __exit mapdrvo_exit(void)
{
    unsigned long virt_addr;
    dev_t devno = MKDEV(major, 0);
    for ( virt_addr = (unsigned long)vmalloc_area;
            virt_addr < (unsigned long)(&(vmalloc_area[MAPLEN / sizeof(int)]));
            virt_addr += PAGE_SIZE )
    {
        ClearPageReserved(virt_to_page(vaddr_to_kaddr((void *)virt_addr)));
    }
    if ( vmalloc_area )
        vfree(vmalloc_area);
    cdev_del(&md->mapdevo);
    unregister_chrdev_region(devno, 1);
    kfree(md);
}

int mapdrvo_open(struct inode *inode, struct file *file)
{
    struct mapdrvo *md;
    md = container_of(inode->i_cdev, struct mapdrvo, mapdevo);
    atomic_inc(&md->usage);
    return (0);
}

int mapdrvo_release(struct inode *inode, struct file *file)
{
    struct mapdrvo *md;
    md = container_of(inode->i_cdev, struct mapdrvo, mapdevo);
    atomic_dec(&md->usage);
    return (0);
}

int mapdrvo_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
    unsigned long size   = vma->vm_end - vma->vm_start;
    if ( offset&~PAGE_MASK )
    {
        printk("offset not aligned:%ld\n", offset);
        return -ENXIO;
    }
    if ( size > MAPLEN )
    {
        printk("size too big\n");
        return (-ENXIO);
    }
    if ( (vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED) )
    {
        printk("writeable mappings must be shared, rejecting\n");
        return (-EINVAL);
    }
    vma->vm_flags |= VM_LOCKED;
    if ( offset == 0 )
    {
        vma->vm_ops = &map_vm_ops;
        map_vopen(vma);
    }
    else 
    {
        printk("offset out of range\n");
        return -ENXIO;
    }
    return (0);
}

void map_vopen(struct vm_area_struct *vma)
{
    
}

void map_vclose(struct vm_area_struct *vma)
{

}

struct page *map_nopage(struct vm_area_struct *vma, unsigned long address, int *type)
{
    unsigned long offset;
    unsigned long virt_addr;
    offset = address - vma->vm_start + (vma->vm_pgoff << PAGE_SHIFT);
    virt_addr = (unsigned long)vaddr_to_kaddr(&vmalloc_area[offset / sizeof(int)]);
    if ( virt_addr == 0UL )
    {
        return ((struct page *)0UL);
    }
    get_page(virt_to_page(virt_addr));
    printk("map_drv: page fault for offset 0x%lx (ksegx%lx)\n", offset, virt_addr);
    return (virt_to_page(virt_addr));
}

module_init(mapdrvo_init);
module_exit(mapdrvo_exit);
