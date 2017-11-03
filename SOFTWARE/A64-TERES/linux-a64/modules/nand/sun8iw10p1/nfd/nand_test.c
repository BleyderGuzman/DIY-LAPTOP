
#define _NAND_TEST_C_

#include "nand_blk.h"
#include "nand_dev.h"
//#include <linux/random.h>

extern int debug_data;

extern uint32 gc_all(void* zone);
extern uint32 gc_one(void* zone);
extern uint32 prio_gc_one(void* zone,uint16 block,uint32 flag);
extern void print_nftl_zone(void * zone);
extern void print_free_list(void* zone);
extern void print_block_invalid_list(void* zone);
extern uint32 nftl_set_zone_test(void * _zone,uint32 num);
extern int nand_dbg_phy_read(unsigned short nDieNum, unsigned short nBlkNum, unsigned short nPage);
extern int nand_dbg_zone_phy_read(void *zone,uint16 block,uint16 page);
extern int nand_dbg_zone_erase(void *zone,uint16 block,uint16 erase_num);
extern int nand_dbg_zone_phy_write(void *zone,uint16 block,uint16 page);
extern int nand_dbg_phy_write(unsigned short nDieNum, unsigned short nBlkNum, unsigned short nPage);
extern int nand_dbg_phy_erase(unsigned short nDieNum, unsigned short nBlkNum);
extern int _dev_nand_read2(char * name,__u32 start_sector,__u32 len,unsigned char *buf);
extern void nand_phy_test(void);
extern int nand_check_table(void *zone);


extern int _dev_nand_read(struct _nand_dev *nand_dev,__u32 start_sector,__u32 len,unsigned char *buf);
extern int _dev_nand_write(struct _nand_dev *nand_dev,__u32 start_sector,__u32 len,unsigned char *buf);
extern int _dev_flush_write_cache(struct _nand_dev *nand_dev, __u32 num);
extern struct _nand_dev * _get_nand_dev_by_name(char * name);

struct task_struct *p_udisk_test_thread = NULL;
int udisk_test_thread_running = 0;
struct _nand_dev *udisk_test_nand_dev = NULL;

unsigned char* udisk_test_buf; //128K
unsigned char* udisk_test_mapping; // (500*1024) * 8 byte

#define  MAX_PTR_PER_TEST   (500*1024)

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static unsigned long rand = 100;
static inline unsigned int my_random(void)
{
	/* See "Numerical Recipes in C", second edition, p. 284 */
	rand = rand * 1664525L + 1013904223L;
	return rand;
}

unsigned int udisk_test_get_rand(unsigned int min,unsigned int max)
{
    unsigned int max_num,rand_dat;

    max_num = max - min;

    rand_dat = my_random();

    rand_dat = rand_dat % max_num;

    return min + rand_dat;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_fill_buf(unsigned int counter,unsigned int addr,unsigned int len,unsigned char *buf)
{
    int i;
    unsigned int *p_dat1;
    unsigned int *p_dat2;

    memset(buf,0xa3,len*512);

    for(i=0;i<len;i++)
    {
        p_dat1 = (unsigned int *)(buf + i*512);
        p_dat2 = (unsigned int *)(buf + i*512 + 4);

        *p_dat1 = addr+i;
        *p_dat2 = counter;
    }
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_check_buf(unsigned int counter,unsigned int addr,unsigned int len,unsigned char *buf)
{
    int i;
    unsigned int *p_dat1;
    unsigned int *p_dat2;

    for(i=0;i<len;i++)
    {
        p_dat1 = (unsigned int *)(buf + i*512);
        p_dat2 = (unsigned int *)(buf + i*512 + 4);

        if((*p_dat1 != (addr+i)) || (*p_dat2 != counter))
        {
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_updata_mapping(unsigned int ptr,unsigned int addr,unsigned int len,unsigned char *mapping)
{
    unsigned int *p_addr;
    unsigned int *p_len;

    p_addr = (unsigned int *)(mapping + ptr*8);
    p_len = (unsigned int *)(mapping + ptr*8 + 4);
    *p_addr = addr;
    *p_len = len;

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_write_mapping(unsigned char *mapping)
{
    unsigned int addr,len;

    addr = 0x16000;
    len = (MAX_PTR_PER_TEST * 8);
    len >>= 9;

    _dev_nand_write(udisk_test_nand_dev,addr,len,mapping);

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_read_mapping(unsigned char *mapping)
{
    unsigned int addr,len;

    addr = 0x16000;
    len = (MAX_PTR_PER_TEST * 8);
    len >>= 9;

    _dev_nand_read(udisk_test_nand_dev,addr,len,mapping);

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
unsigned int udisk_test_check_mapping(unsigned char *mapping,unsigned char *buf)
{
    unsigned int magic_dat;
    unsigned int i,counter,flag;

    unsigned int* p_addr;
    unsigned int* p_len;

    udisk_test_read_mapping(mapping);

    magic_dat = *(unsigned int*)mapping;

    if((magic_dat != 0xf8f8f8f8) && (magic_dat != 0xffffffff))
    {
        printk("udisk test can not get mapping 0x%x!\n",magic_dat);
        return 0xfffffff0;
    }

    if(magic_dat == 0xffffffff)
    {
        printk("udisk test first !\n");
        return 0xffffffff;
    }

//////////////
    p_addr = (unsigned int*)(mapping + 8);
    p_len = (unsigned int*)(mapping + 8 + 4);

    _dev_nand_read(udisk_test_nand_dev,*p_addr,*p_len,udisk_test_buf);
    p_addr = (unsigned int*)(udisk_test_buf + 4);
    counter = *p_addr;
//////////////

    flag = 0;
    for(i=1;i<MAX_PTR_PER_TEST;i++)
    {
        p_addr = (unsigned int*)(mapping + i*8);
        p_len = (unsigned int*)(mapping + i*8 + 4);

        if(*p_addr == 0xffffffff)
        {
            printk("udisk test check num:%d !\n",i);
            break;
        }

        _dev_nand_read(udisk_test_nand_dev,*p_addr,*p_len,udisk_test_buf);

        flag = udisk_test_check_buf(counter,*p_addr,*p_len,udisk_test_buf);
        if(flag != 0)
        {
            printk("udisk test check_mapping  error %d!\n",i);
            return 0xfffffff0;
        }
    }

    return counter;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int udisk_test_thread(void *arg)
{
    unsigned int min_addr,max_addr,min_len,max_len;
    unsigned int addr,len,counter,ptr;

    long time_out = 1*HZ;
    udisk_test_nand_dev  = _get_nand_dev_by_name("UDISK");

    min_len = 1;
    max_len = 256; //128k

    min_addr = 0x32000;  //start from 100M
    max_addr = udisk_test_nand_dev->size - 1 - max_len;

    if(min_addr >= max_addr)
    {
        udisk_test_thread_running = 3;
        printk("udisk test not start 1!\n");
    }

    ptr = 1;
    counter = udisk_test_check_mapping(udisk_test_mapping,udisk_test_buf);
    if(counter == 0xfffffff0)
    {
        udisk_test_thread_running = 3;
        ptr = MAX_PTR_PER_TEST;
        printk("udisk test error!!!!\n");
    }

    counter += 1;
    memset(udisk_test_mapping,0xff,MAX_PTR_PER_TEST*8);
    memset(udisk_test_mapping,0xf8,8);

    while (!kthread_should_stop())
    {
        if(udisk_test_thread_running != 1)
        {
            if(ptr < MAX_PTR_PER_TEST)
            {
                udisk_test_write_mapping(udisk_test_mapping);
            }
            udisk_test_thread_running = 3;
            goto udisk_test_sleep;
        }

        //mutex_lock(nftl_blk->blk_lock);
//////////////////////////////////////////////////////////////////////

        addr = udisk_test_get_rand(min_addr,max_addr);
        len = udisk_test_get_rand(min_len,max_len);

        udisk_test_updata_mapping(ptr,addr,len,udisk_test_mapping);

        udisk_test_fill_buf(counter,addr,len,udisk_test_buf);


        printk("udisk test thread running addr:0x%x len:%d total size:0x%x conter:0x%x,ptr:%d !\n",addr,len,udisk_test_nand_dev->size,counter,ptr);
        _dev_nand_write(udisk_test_nand_dev,addr,len,udisk_test_buf);

        ptr++;
        if(ptr >= MAX_PTR_PER_TEST)
        {
            printk("max write time in one test!\n");
            udisk_test_write_mapping(udisk_test_mapping);
        }

//////////////////////////////////////////////////////////////////////
        //mutex_unlock(nftl_blk->blk_lock);

udisk_test_sleep:
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(time_out);
    }
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void udisk_test_start(struct _nftl_blk* nftl_blk)
{
    if(p_udisk_test_thread == NULL)
    {
        udisk_test_thread_running = 1;

        udisk_test_buf = kmalloc(0x20000, GFP_KERNEL); //128K
        udisk_test_mapping = kmalloc(MAX_PTR_PER_TEST*8, GFP_KERNEL); // (500*1024) * 8 byte  =

        p_udisk_test_thread = kthread_run(udisk_test_thread, nftl_blk, "%sd", "nftl");
        printk("udisk test thread start!\n");
    }
    else
    {
        printk("udisk test thread already start!\n");
    }
    return;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void udisk_test_stop(void)
{
    if(p_udisk_test_thread == NULL)
    {
        printk("udisk test thread already stop!\n");
        return;
    }

    udisk_test_thread_running = 2;

    while(1)
    {
        schedule_timeout(HZ);
        if(udisk_test_thread_running == 3)
        {
            kthread_stop(p_udisk_test_thread);

            kfree(udisk_test_mapping);
            kfree(udisk_test_buf);
            _dev_flush_write_cache(udisk_test_nand_dev, 0xffff);
            udisk_test_thread_running = 0;
            p_udisk_test_thread = NULL;
            printk("udisk test thread stop!\n");
            break;
        }
    }
    return;
}
