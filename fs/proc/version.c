#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

static int version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, linux_proc_banner,
		utsname()->sysname,
		utsname()->release,
		utsname()->version);
	return 0;
}

static int version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, version_proc_show, NULL);
}

static const struct file_operations version_proc_fops = {
	.open		= version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

//add by liangdi for boot version 20201201
static int boot_version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", "MT670-V0005-20210525\n");
	return 0;
}

static int boot_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_version_proc_show, NULL);
}

static const struct file_operations boot_version_proc_fops = {
	.open		= boot_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
//add end

static int __init proc_version_init(void)
{
	proc_create("version", 0, NULL, &version_proc_fops);
	proc_create("boot_version", 0, NULL, &boot_version_proc_fops);
	return 0;
}
fs_initcall(proc_version_init);
