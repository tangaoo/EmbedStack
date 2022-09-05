
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/string_64.h>
#include <asm/string.h>
#include <asm/uaccess.h>


#define DEV_NAME                 "pcie_fpga"
#define PCIE_DEMO_VENDOR_ID      0x10EE
#define PCIE_DEMO_DEVICE_ID      0x7038 
#define BAR_NUM                  6

#define CHANNEL_NUM              8     // 通道数
#define BING_PANG                2     // 兵乓数
#define IRQ_SIZE                 (CHANNEL_NUM * BING_PANG) // 中断数
#define MEM_BLOCK_SIZE           (4 * 1024 * 1024)         // 兵乓数

#define tt_min(a, b)             ((a) > (b) ? (b) : (a))


struct bar_node {
    unsigned long phys;
    unsigned long virt;
    unsigned long length;
    unsigned long used;
};

typedef irqreturn_t (*isr_func_t)(int, void*);

typedef struct __fpga_pcie_device{
	struct pci_dev* pcie_dev;           // pci device
	struct cdev     cdev;               
	dev_t           devno;
	struct class*   class;
	void*           block_virt[CHANNEL_NUM * BING_PANG];
	void*           block_phy[CHANNEL_NUM * BING_PANG];
	char            ready[CHANNEL_NUM * BING_PANG];      // data ready flag
	char*           read_flag;                           // after app read finished, set 0
	isr_func_t      isr_func[CHANNEL_NUM * BING_PANG];
	void*           bar_virt; 
	void*           bar_phy; 
	struct semaphore sema;
	struct mutex    mutex;
		
}fpga_pcie_device_t, *fpga_pcie_device_ref_t;

static fpga_pcie_device_t  g_dev; 
static int s_irq[IRQ_SIZE] = {0};


static int fpga_pcie_dev_init(fpga_pcie_device_ref_t pdev);


// 16 ISR
static irqreturn_t isr0_func(int irq, void *dev)
{
//	printk("isr0, got interrupt \n");

	g_dev.ready[0] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr1_func(int irq, void *dev)
{
	//printk("isr1, got interrupt \n");

	g_dev.ready[1] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr2_func(int irq, void *dev)
{
	//printk("isr2, got interrupt \n");

	g_dev.ready[2] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr3_func(int irq, void *dev)
{
	//printk("isr3, got interrupt \n");

	g_dev.ready[3] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr4_func(int irq, void *dev)
{
//	printk("isr4, got interrupt \n");

	g_dev.ready[4] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr5_func(int irq, void *dev)
{
//	printk("isr5, got interrupt \n");

	g_dev.ready[5] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr6_func(int irq, void *dev)
{
//	printk("isr6, got interrupt \n");

	g_dev.ready[6] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr7_func(int irq, void *dev)
{
//	printk("isr7, got interrupt \n");

	g_dev.ready[7] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr8_func(int irq, void *dev)
{
//	printk("isr8, got interrupt \n");

	g_dev.ready[8] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr9_func(int irq, void *dev)
{
//	printk("isr9, got interrupt \n");

	g_dev.ready[9] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr10_func(int irq, void *dev)
{
//	printk("isr10, got interrupt \n");

	g_dev.ready[10] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr11_func(int irq, void *dev)
{
//	printk("isr11, got interrupt \n");

	g_dev.ready[11] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr12_func(int irq, void *dev)
{
//	printk("isr12, got interrupt \n");

	g_dev.ready[12] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr13_func(int irq, void *dev)
{
//	printk("isr13, got interrupt \n");

	g_dev.ready[13] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr14_func(int irq, void *dev)
{
//	printk("isr14, got interrupt \n");

	g_dev.ready[14] = 1;
	up(&g_dev.sema);

	return 0;
}

static irqreturn_t isr15_func(int irq, void *dev)
{
//	printk("isr15, got interrupt \n");

	g_dev.ready[15] = 1;
	up(&g_dev.sema);

	return 0;
}

static isr_func_t s_isr_funcs[] = {isr0_func,  isr1_func,  isr2_func,  isr3_func, 
		                           isr4_func,  isr5_func,  isr6_func,  isr7_func, 
								   isr8_func,  isr9_func,  isr10_func, isr11_func, 
								   isr12_func, isr13_func, isr14_func, isr15_func};

/* Barn(n=0,1 or 0,1,2,3,4,5) physical address, length, virtual address */
struct bar_node bars[BAR_NUM] = {
    /* Bar0 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 1,
    },
    /* Bar1 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 0,
    },
    /* Bar2 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 0,
    },
    /* Bar3 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 0,
    },
    /* Bar4 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 0,
    },
    /* Bar5 */
    {
        .phys = 0,
        .virt = 0,
        .length = 0,
        .used = 0,
    },
};

/*
 * open operation
 */
static int pcie_fpga_open(struct inode *inode,struct file *filp)
{
    printk(KERN_INFO "Open device\n");
    return 0;
}
/*
 * release opertion 
 */
static int pcie_fpga_release(struct inode *inode,struct file *filp)
{
    return 0;
}
/*
 * read operation @note count > 4MB
 */
static ssize_t pcie_fpga_read(struct file *filp, char __user *buffer, size_t count, loff_t *offset)
{
	int i, ret, channel = -1;

	// check parms
	if(count < MEM_BLOCK_SIZE)
	{
		printk("app buff < 4MB, failed\n");
		return -EFAULT;
	}

	// wait sema, ISR will send the sema
	if(down_interruptible(&g_dev.sema))
		return -ERESTARTSYS;
	
	// mutex 
	if(mutex_lock_interruptible(&g_dev.mutex))
		return -ERESTARTSYS;
	// find ready channel
	for(i = 0; i < IRQ_SIZE; i++)
	{
		if(g_dev.ready[i]) 
		{
			channel = i;	
			break;
		}
	}
	g_dev.ready[channel] = 0;
	mutex_unlock(&g_dev.mutex);

	ret = copy_to_user(buffer, g_dev.block_virt[channel], MEM_BLOCK_SIZE);
	if(ret) 
	{
		printk("copy to user failed\n");
		return -EFAULT;
	}

	// clear flag
	*(g_dev.read_flag + channel) = 0;

    return count;
}
/*
 * write operation
 */
static ssize_t pcie_fpga_write(struct file *filp,const char __user *buf, size_t count,loff_t *offset)
{
    return 0;
}

/*
 * file_operations
 */
static struct file_operations pcie_dev_fops = {
    .owner     = THIS_MODULE,
    .open      = pcie_fpga_open,
    .release   = pcie_fpga_release,
    .write     = pcie_fpga_write,
    .read      = pcie_fpga_read,
};


static int pcie_fpga_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int result, i, payload = 0, irqs = 0;
	unsigned long tmp = 0;

    printk("PCIe probe starting....Vendor %#x DeviceID %#x\n", id->vendor, id->device);

    /* Enable PCIe */
    if (pci_enable_device(pdev)) {
        result = -EIO;
        goto end;
    }

    pci_set_master(pdev);
	g_dev.pcie_dev = pdev;

    if (unlikely(pci_request_regions(pdev, DEV_NAME))) {
        printk("Failed: pci_request_regions\n");
        result = -EIO;
        goto enable_device_err;
    }

    /* Obtain bar0 to bar5 information */
    for (i = 0; i < BAR_NUM; i++) 
	{
        if (!bars[i].used)
            continue;
        /* Obtain bar physical address */
        bars[i].phys = pci_resource_start(pdev, i);
		if (bars[i].phys < 0) 
		{
			printk("Failed: Bar%d pci_resource_start\n", i);
			result = -EIO;
			goto request_regions_err;
		}

        /* Obtain the length for Bar */
        bars[i].length = pci_resource_len(pdev, 0);
        if (bars[i].length != 0)
            bars[i].virt = (unsigned long)ioremap(bars[i].phys, bars[i].length);

        printk("Bar%d=> phys: %#lx virt: %#lx length: %#lx\n", i, bars[i].phys, bars[i].virt, bars[i].length);
    }

	// only BAR0 be used
	g_dev.bar_virt = (void *)bars[0].virt;
	g_dev.bar_phy  = (void *)bars[0].phys;

	// write block addr to FPGA
	for(i = 0; i < CHANNEL_NUM * BING_PANG; i++)
	{
		*((unsigned long*)g_dev.bar_virt + i) = (unsigned long)g_dev.block_phy[i];
		tmp = *((unsigned long*)g_dev.bar_virt + i);
		printk(" 0x%lX, ", tmp);
	}
	// enable FPGA
	*((unsigned long*)g_dev.bar_virt + i) = 1;

	// read finished flag
	g_dev.read_flag = (char*)g_dev.bar_virt + CHANNEL_NUM * BING_PANG + 1;

	printk("\n");
	printk("kmalloc success \n" );


	// alloc irqs and register ISR function
	irqs = pci_alloc_irq_vectors(pdev, 1, IRQ_SIZE, PCI_IRQ_MSI);	
	if (irqs <= 0)
	{
		printk("alloc irq verctor failed \n");
		goto request_regions_err;
	}
	printk("alloc %d irqs success \n", irqs);

	for (i = 0; i < irqs; i++)
	{
		s_irq[i] = pci_irq_vector(pdev, i);	

		result = request_irq(s_irq[i], s_isr_funcs[i], 0, DEV_NAME, NULL);
		if (unlikely(result))
		{
			printk("request_irq %d failed\n", i);
		}
	}

	// set MAX payload
	pcie_set_mps(pdev, 512);
	payload = pcie_get_mps(pdev);
	printk("set payload = %d\n", payload);

    return 0;


request_regions_err:
    pci_release_regions(pdev);

enable_device_err:
    pci_disable_device(pdev);

end:

    return result;
}

static void pcie_fpga_remove(struct pci_dev *pdev)
{
    int i;

	for(i =0; i < IRQ_SIZE; i++)
	{
		if(s_irq[i]) free_irq(s_irq[i], NULL);
	}

//	pci_disable_msi(pdev);

	pci_free_irq_vectors(pdev);	

    for (i = 0; i < BAR_NUM; i++)
        if (bars[i].virt > 0)
            iounmap((void *)bars[i].virt);

    pci_release_regions(pdev);

    pci_disable_device(pdev);
}


static struct pci_device_id pcie_fpga_ids[] = {
    { PCIE_DEMO_VENDOR_ID, PCIE_DEMO_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pcie_fpga_ids);

static struct pci_driver pci_drivers = {
    .name = DEV_NAME,
    .id_table = pcie_fpga_ids,
    .probe = pcie_fpga_probe,
    .remove = pcie_fpga_remove,
};

static int fpga_pcie_dev_init(fpga_pcie_device_ref_t pdev)
{
	int i, ret = 0;		

	memset(pdev, 0, sizeof(fpga_pcie_device_t));		

	// malloc data
	for(i = 0; i < CHANNEL_NUM * BING_PANG; i++)
	{
		pdev->block_virt[i] = kmalloc(MEM_BLOCK_SIZE, GFP_KERNEL);
		if(pdev->block_virt[i] == NULL)
		{
			printk("Malloc %dth block failed\n", i);
			ret = -1;
			goto alloc_err;
		}
		pdev->block_phy[i]  = (void *)virt_to_phys(pdev->block_virt[i]);

	}

	// init sema mutex
	sema_init(&pdev->sema, 0);
	mutex_init(&pdev->mutex);

	// isr function init
	for(i = 0; i < CHANNEL_NUM * BING_PANG; i++)
	{
		pdev->isr_func[i] = s_isr_funcs[i]; 
	}

	return 0;

alloc_err:
	for(i = 0; i < CHANNEL_NUM * BING_PANG; i++)
	{
		if(pdev->block_virt[i])
			kfree(pdev->block_virt[i]);
	}

	return ret;
}


/*
 * Init module
 */
static __init int pcie_fpga_init(void)
{
    int ret;

	// init dev
    ret = fpga_pcie_dev_init(&g_dev);
	if(ret != 0)
	{
		printk("pcie dev init failed\n");
		return ret;
	}

    /* Register PCIe driver */
    ret = pci_register_driver(&pci_drivers);
	if(ret != 0)
	{
		printk("pci register failed\n");
		return ret;
	}

	// register char device
	ret = alloc_chrdev_region(&g_dev.devno, 0, 1, DEV_NAME);	
	if(ret < 0)
	{
		printk("alloc chrdev region failed\n");
		return ret;
	}

	cdev_init(&g_dev.cdev, &pcie_dev_fops);
	ret = cdev_add(&g_dev.cdev, g_dev.devno, 1);
	if(ret < 0)
	{
		printk("cdev add failed\n");
		return ret;
	}
	g_dev.class = class_create(THIS_MODULE, DEV_NAME);	
	device_create(g_dev.class, NULL, g_dev.devno, NULL, DEV_NAME);

    return 0;
}

/*
 * Exit module
 */
static __exit void pcie_fpga_exit(void)
{
	int i;

	// free memory
	for(i = 0; i < CHANNEL_NUM * BING_PANG; i++)
	{
		if(g_dev.block_virt[i])
			kfree(g_dev.block_virt[i]);
	}

	// unregister char device
	device_destroy(g_dev.class, g_dev.devno);
	class_destroy(g_dev.class);

	cdev_del(&g_dev.cdev);
	unregister_chrdev_region(g_dev.devno, 1);

    /* Unregister PCIe driver */
    pci_unregister_driver(&pci_drivers);
}
/*
 * module information
 */
module_init(pcie_fpga_init);
module_exit(pcie_fpga_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tangtao");
