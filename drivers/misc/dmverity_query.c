#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <soc/qcom/scm.h>

#define OEM_SVC_CALLS                          0x03000000
#define MAKE_OEM_SCM_CMD(svc_id, cmd_id)       ((((svc_id << 8) | (cmd_id)) & 0xFFFF) | OEM_SVC_CALLS)

#define SCM_DMVERITY_CMD_ID 0x1
#define SCM_SVC_DMVERITY 245
uint32_t dmverity_resp = 4;

static int verity_scm_call(void)
{
	int ret;
	struct scm_desc descrp = {0};
	descrp.arginfo = SCM_ARGS(4, SCM_VAL, SCM_VAL, SCM_RW, SCM_VAL);
	descrp.args[0] = 3; //command Read
	descrp.args[1] = 0;
	descrp.args[2] = virt_to_phys((void*)&dmverity_resp); // Respnse
	descrp.args[3] = 4;
	
	ret = scm_call2(MAKE_OEM_SCM_CMD(SCM_SVC_DMVERITY, SCM_DMVERITY_CMD_ID), &descrp);
	
	return dmverity_resp;
}

#define DRIVER_DESC   "Read whether odin flash succeeded"

static int dmverity_read(struct seq_file *m, void *v){
	int odin_flag = 0;
	unsigned char ret_buffer[10];
	odin_flag = verity_scm_call();

	memset(ret_buffer, 0, sizeof(ret_buffer));
	snprintf(ret_buffer, sizeof(ret_buffer), "%08x\n", odin_flag);
	seq_write(m, ret_buffer, sizeof(ret_buffer));
	printk(KERN_INFO"dmverity: odin_flag: %x\n", odin_flag);
	return 0;
}
static int dmverity_open(struct inode *inode, struct file *filep){
	return single_open(filep, dmverity_read, NULL);
}

static const struct file_operations dmverity_proc_fops = {
	.open       = dmverity_open,
	.read	    = seq_read,
	
};

/**
 *      dmverity_odin_flag_read_init -  Initialization function for DMVERITY
 *
 *      It creates and initializes dmverity proc entry with initialized read handler 
 */
static int __init dmverity_odin_flag_read_init(void)
{
	//extern int boot_mode_recovery;
	if (/* boot_mode_recovery == */ 1) {
		/* Only create this in recovery mode. Not sure why I am doing this */
        	if (proc_create("dmverity_odin_flag", 0644,NULL, &dmverity_proc_fops) == NULL) {
			printk(KERN_ERR"dmverity_odin_flag_read_init: Error creating proc entry\n");
			goto error_return;
		}
	        printk(KERN_INFO"dmverity_odin_flag_read_init:: Registering /proc/dmverity_odin_flag Interface \n");
	} else {
		printk(KERN_INFO"dmverity_odin_flag_read_init:: not enabling in non-recovery mode\n");
		goto error_return;
	}

        return 0;

error_return:
	return -1;
}


/**
 *      dmverity_odin_flag_read_exit -  Cleanup Code for DMVERITY
 *
 *      It removes /proc/dmverity proc entry and does the required cleanup operations 
 */
static void __exit dmverity_odin_flag_read_exit(void)
{
        remove_proc_entry("dmverity_odin_flag", NULL);
        printk(KERN_INFO"Deregistering /proc/dmverity_odin_flag interface\n");
}


module_init(dmverity_odin_flag_read_init);
module_exit(dmverity_odin_flag_read_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
