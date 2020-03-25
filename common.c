#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <limits.h>
#include "common.h"

static OSS * ossaddr = NULL;
static int shmid = -1;

OSS * oss_init(const unsigned int numbers_count){

	const int shmflg = (numbers_count) ? S_IRUSR | S_IWUSR | IPC_CREAT : 0;
	const int key = ftok("makefile", 1234);
	const size_t size = sizeof(OSS) + (sizeof(unsigned int)*numbers_count);

	shmid = shmget(key, size, shmflg);
	if (shmid == -1) {
		perror("shmget");
		return NULL;
	}

	ossaddr = (OSS*) shmat(shmid, (void *)0, 0);
	if (ossaddr == (void *)-1) {
		perror("shmat");
		shmctl(shmid, IPC_RMID, NULL);
		return NULL;
	}

	if(numbers_count){ /* if we create the memory */

		ossaddr->oss_pid = getpid();
		ossaddr->clock.tv_sec = 0;
		ossaddr->clock.tv_nsec = 0;

		bzero(ossaddr->users, sizeof(struct user)*USER_LIMIT);
		bzero(ossaddr->numbers, sizeof(unsigned int)*numbers_count);

  	if(sem_init(&ossaddr->mutex, 1, 1) == -1){
  		perror("sem_init");
			oss_deinit();
  		return NULL;
  	}
  }

	return ossaddr;
}

void oss_deinit(){

  if(ossaddr->oss_pid == getpid()){
    if(sem_destroy(&ossaddr->mutex) == -1){
      perror("sem_destroy");
    }
		if(shmdt(ossaddr) == -1){
	    perror("shmdt");
	  }
		if(shmctl(shmid, IPC_RMID,NULL) == -1){
      perror("shmctl");
    }

  }else{
		if(shmdt(ossaddr) == -1){
			perror("shmdt");
		}
	}
}

//Send a message to the log file
int log_message(FILE * f, const char *fmt, ...)
{
	va_list args;

	//lock the semaphore to get synchronized access to log file
	if(sem_wait(&ossaddr->mutex) == -1){
		perror("sem_wait");
		return -1;
	}

	//print the message to log
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);


	if(sem_post(&ossaddr->mutex) == -1){
		perror("sem_wait");
		return -1;
	}

	return 0;
}
