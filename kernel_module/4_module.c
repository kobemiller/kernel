#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pid.h>

static int pid;
module_param(pid, int, 0644);

static int __init memtest_init(void)
{
    struct task_struct *p;
    struct vm_area_struct *temp;

    printk("the virtual memory areas(VMA) are:\n");
    p = pid_task(find_vpid(pid), PIDTYPE_PID);
    temp = p->mm->mmap;
    while ( temp )
    {
        printk("start: %p\tend: %p\n", (unsigned long *)temp->vm_start, (unsigned long *)temp->vm_end);
        temp = temp->vm_next;
    }
    return 0;
}

static void __exit memtest_exit(void)
{
    printk("unloading my module.\n");
    return;
}

module_init(memtest_init);
module_exit(memtest_exit);
MODULE_LICENSE("GPL");
