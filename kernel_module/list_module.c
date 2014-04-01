#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>

struct namelist           //定义结构体
{
    char *name;
    struct list_head list;
};

struct namelist *head;     //定义链表头结点

static int __init linklist_init(int ac, char *av[])
{
    
    INIT_LIST_HEAD(&head->list);
    if ( ac == 1 )
		do_ls(".");
	else
		while ( --ac )
		{
			printk("%s:\n", * ++av);
			do_ls( *av );
		}
    return 0;
}

void do_ls(char dirname[])
/*
 * list files in directory called dirname
 */
{
	DIR *dir_ptr;
	struct dirent *direntp;

	if ( (dir_ptr = opendir(dirname)) == NULL )
		fprintk(stderr, "ls1:cannot open %s\n", dirname);
	else
	{
  		while ( (direntp = readdir(dir_ptr)) != NULL )
		{
			list_insert(namehead, direntp->d_name);
		}
		closedir(dir_ptr);
	}
	print_list(namehead);
}

int max_len(struct namelist *namehead)
{
	int max_length = 0, n;
	struct list_head *pos;
	struct namelist *p;

	list_for_each(pos, &namehead->list)
	{
		p = list_entry(pos, struct namelist, list);
		max_length = strlen(p->name) > max_length ? : max_length;
        /*这是我套用三目运算符，不知道用的对不对，下边是普通版本*/
        /*
        n = strlen(p->name);
        if ( max_length < n )
            max_length = n;
        */
	}

	return max_length;
}

int name_count(struct namelist *namehead)
{
	int count = 0;
	struct list_head *pos;

	list_for_each(pos, &namehead->list)
		count++;

	return count;
}

int get_width()
{
	struct winsize size;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &size);
	return size.ws_col;
}

void print_list(struct namelist *namehead)
{
	int width, count, max_length;
	int m, i;
	struct list_head *pos;
	struct namelist *p;

	width = get_width();
	count = name_count(namehead);
	max_length = max_len(namehead);
	m = width / max_length;         
    /*我对分栏的处理是这样的：
     * 求得当前终端的宽度，然后除以最长的文件名的长度，得到的m值即为分栏数
     */

	list_for_each(pos, &namehead->list)
	{
		for ( i = 0; i < m; i++ )
		{
			p = list_entry(pos, struct namelist, list);
			printk("%-*s", m, p->name);
		}
		printk("\n");
	}
}

void list_insert(struct namelist *namehead, char *dname)
{
	struct namelist *listnode;

	listnode = (struct namelist *)malloc(sizeof(struct namelist));
	strcpy(listnode->name, dname);
	list_add_tail(&listnode->list, &namehead->list);
}

static void __exit linklist_cleanup(void)
{
	struct namelist *cur;
   	 struct list_head *pos, *tmp;
    	int i;
	//清空链表
	for ( i = 0; i < MAX_SIZE; i++ )
        list_for_each_safe(pos, tmp, &head->list)
        {
		list_del(pos);
            cur = list_entry(pos, struct namelist, list);
            kfree(cur);
        }
	printk("<1>goodbye,leaving kernel space!...\n");
}

module_init(linklist_init);
module_exit(linklist_cleanup);
MODULE_LICENSE("GPL");
