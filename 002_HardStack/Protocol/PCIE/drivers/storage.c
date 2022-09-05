
#define _GNU_SOURCE

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <fcntl.h>

// size
#define BUF_SIZE           (4 * 1024 * 1024)
#define BUF_SIZE_RAW       (BUF_SIZE - 64)
#define BUF_RAW_OFFSET     (32)
#define WRITE_CNT          (100)
#define DEV_PATH           "/dev/pcie_fpga"
#define SIZE_PER_FILE      (1 * 1024 * 1024 * 1024)
#define WRITE_TASK         (8)

// 磁盘路径
#define PATH0              "/mnt/d0/"
#define PATH1              "/mnt/d1/"
#define PATH2              "/mnt/d2/"
#define PATH3              "/mnt/d3/"

typedef struct _msgbuff
{
	long mtype;
	void *data;
}msgbuff_t, *msgbuff_ref_t;

typedef struct _storage{
	char  path[128];
	char* data;
	char  channel;
	char  ready;
}storage_t, *storage_ref_t;


// global 
static pthread_t pid_w[WRITE_TASK] = {0};
static pthread_t pid_r = 0;
static storage_t g_stg[WRITE_TASK];
static int s_msgid = 0;

/*
 * malloc memory for transport data from PCIe driver to disk
 */
char* generate_data(void)
{
	char* data = (char*)malloc(BUF_SIZE);
	if(data == NULL)
	{
		printf("malloc data failed\n");
		exit(-1);
	}

	// fill data
	memset(data, 0, BUF_SIZE);

	return data;
}

/*
 * free memory
 */
void remove_data(storage_ref_t stg)
{
	int i;

	for(i = 0; i < WRITE_TASK; i++)
	{
		if(g_stg[i].data)	
			free(g_stg[i].data);
	}
}

/*
 * analyse head info of data.
 */
int analyse_data(char* data)
{
	// 0x55555555AAAAAAAA
	unsigned long long prefix =  *(unsigned long long*)data;	
	unsigned int channel = *(unsigned int*)(data + 8);
	unsigned int idx = *(unsigned int*)(data + 12);

	return 0;
}

/*
 * read task work 
 */
void* thread_read_pcie(void* arg)
{
	void *data = NULL;
	unsigned long long i = 0;
	unsigned long long prefix;
	unsigned int channel;
	unsigned int idx;
	msgbuff_t msg;

	// file read to
	int fd_w = open(DEV_PATH, O_RDONLY);
    if (fd_w == -1)
        printf("opening file %s failed\n", DEV_PATH);

	while(1)
	{
		data = g_stg[i++ % WRITE_TASK].data;
		if(data == NULL)
		{
			printf("get data failed\n");		
			continue;
		}
		if (read(fd_w, data, BUF_SIZE) != BUF_SIZE)
			printf("read() returned error or partial write occurred\n");

		// analyse head info of data
		prefix =  *(unsigned long long*)data;	
		channel = *(unsigned int*)((char*)data + 8) + 1;
		idx = *(unsigned int*)((char*)data + 12);
		printf("prefix,%llX, channel,%u, idx,%u \n", prefix, channel, idx);

		if(prefix != 0x55555555AAAAAAAA)
		{
			printf("prefix wrong, %llx\n", prefix);
			continue;
		}

		if(channel <=0 || channel > 8)
		{
			printf("channel wrong, %u\n", channel);
			continue;
		}

		if(g_stg[channel].ready != 0)
		{
			printf("warning, channel %uth data have been covered\n", channel);
		}
		// set ready flag
		g_stg[channel].ready = 1;

		// send msg 
		msg.mtype = channel;
		msg.data = data;
		if(msgsnd(s_msgid, &msg, sizeof(msgbuff_t) - sizeof(long), 0) == -1)
		{
			printf("msgsend failed\n");
		}
	}	
}

/*
 * write task work 
 */
void* thread_write_nvme(void* arg)
{
	int openFlags, filePerms;
	storage_ref_t stg = (storage_ref_t)arg;
	char* data = NULL;
	char* path = stg->path;
	int channel= stg->channel;
	unsigned int idx = 0, file_idx = 0;
	int fd;
	char full_path[128] = {0};
	char dir[64] = {0};
	msgbuff_t msg;

	// create new file
    // openFlags = O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT;
    openFlags = O_CREAT | O_WRONLY | O_TRUNC;
    filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; // rw-rw-rw- 

	printf("debug %d\n", channel);
	while(1)
	{
		if(msgrcv(s_msgid, &msg, sizeof(msg), channel, 0) == -1)
		{
			printf("msgrcv failed\n");			
		}
		printf("debug, channel %dth get msg ,%p\n", channel, msg.data);

		if(idx == 0)
		{
			// get time 
			time_t now = time(NULL);
			struct tm *now_tm = gmtime(&now);

			if(channel == 1 || channel == 2)
				strcpy(dir, PATH0);
			if(channel == 3 || channel == 4)
				strcpy(dir, PATH1);
			if(channel == 5 || channel == 6)
				strcpy(dir, PATH2);
			if(channel == 7 || channel == 8)
				strcpy(dir, PATH3);

			// prefix + time, eg: test_20220225_135750
			snprintf(full_path, sizeof(full_path), "%schannel%d_%04d%02d%02d_%02d%02d%02d_%d", 
							dir,
							channel,
				   		    now_tm->tm_year + 1900,
							now_tm->tm_mon + 1,
							now_tm->tm_mday,
							now_tm->tm_hour,
							now_tm->tm_min,
							now_tm->tm_sec,
							file_idx);

			// create new file
    		fd = open(full_path, openFlags, filePerms);
    		if (fd == -1)
			{
		        printf("opening file %s\n", path);
			}
		}

    	/* Transfer data until we encounter end of input or an error */
		if (write(fd, msg.data, BUF_SIZE) != BUF_SIZE)
			printf("write() returned error or partial write occurred\n");

		g_stg[channel].ready = 0;

		if((++idx) >= SIZE_PER_FILE / BUF_SIZE)
		{
			// close file
			if (close(fd) == -1)
			{
				printf("close output\n");
				exit(-1);	
			}
			idx = 0;
			file_idx++;
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	void* ret_thread = NULL;
	int ret;

	// init
	memset(&g_stg, 0, sizeof(g_stg));	

	// create msg queue
	key_t key = ftok(".", 123);
	s_msgid = msgget(key, IPC_CREAT|0666);
	if(s_msgid < 0)
	{
		printf("create msg queue failed\n");
		exit(-1);
	}
	

	// create write pthreads
	for(int i = 0; i < WRITE_TASK; i++)
	{
		g_stg[i].data = generate_data();
		g_stg[i].channel = i + 1;
		g_stg[i].ready = 0;

		ret = pthread_create(&pid_w[i], NULL, thread_write_nvme, &g_stg[i]);
		if(ret != 0)
			printf("create %dth write pthread failed\n", i);
	}

	ret = pthread_create(&pid_r, NULL, thread_read_pcie, NULL);
	if(ret != 0)
		printf("create read pthread failed\n");

	// wait thread exit, it will not run here normally.
	ret = pthread_join(pid_r, &ret_thread);
	if(ret != 0)
		printf("join pthread failed\n");

	for(int i = 0; i < WRITE_TASK; i++)
	{
		ret = pthread_join(pid_w[i], &ret_thread);
		if(ret != 0)
			printf("join pthread failed\n");
	}
	
	remove_data(g_stg);

	return 0;
}
