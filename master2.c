#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <math.h>

#include "common.h"

static OSS *ossaddr = NULL;
static FILE * fLog = NULL;

unsigned int max_procs = USER_LIMIT;
unsigned int num_procs = 0;
unsigned int ext_procs = 0;

unsigned int max_users = 0;
unsigned int num_users = 0;

static void wait_users(struct user * users, const unsigned int size, const int nowait){
	int i, status;
	const int flags = (nowait) ? WNOHANG : 0;
	for(i=0; i < size; i++){
		if(users[i].pid > 0){
			if(waitpid(users[i].pid, &status, flags) > 0){
				users[i].pid = 0;
				num_procs--;
				ext_procs++;
				log_message(fLog, "Child %u was terminated at time %lu.%lu\n", i, ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
			}
		}
	}
}

static void stop_users(struct user * users, const unsigned int size){
	int i;

	for(i=0; i < size; i++){
		if(users[i].pid > 0){
			if(kill(users[i].pid, SIGTERM)){
				users[i].pid = 0;
				num_procs--;
				ext_procs++;
				log_message(fLog, "Child %u was terminated at time %lu.%lu\n", i, ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
			}
		}
	}
}

static void signal_handler(const int sig){

	switch(sig){
		case SIGCHLD:
			wait_users(ossaddr->users, max_users, 1);
			break;
		case SIGINT: case SIGTERM: case SIGALRM:
			stop_users(ossaddr->users, max_users);
			ext_procs = USER_LIMIT;
		default:
			printf("Signal %d at time %lu.%lu\n", sig, ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
			break;
	}
}

static int get_user_id(){
	int i;
	for(i=0; i < USER_LIMIT; i++){
		if(ossaddr->users[i].pid == 0){
			return i;
		}
	}
	return -1;	//no space left
}

static int user_fork(const unsigned int index, const unsigned int len){
	char arg[100], arg2[100];

	const unsigned int user_id = get_user_id();
	if(user_id == -1){	//can't start another adder process
		return 0;	//no space for another user process
	}

	snprintf(arg, sizeof(arg), "%u", index);
	snprintf(arg2, sizeof(arg2), "%u", len);

	const pid_t pid = fork();
	switch(pid){
		case -1:
			perror("fork");
			break;

		case 0:
			execl("./bin_adder", "./bin_adder", arg, arg2, NULL);
			perror("execl");
			exit(EXIT_FAILURE);

		default:
			ossaddr->users[user_id].pid = pid;
			log_message(fLog, "Started bin_adder with PID=%d at index %d and %d len, at time %lu.%lu\n",
				pid, index, len, ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
			break;
	}

	return pid;
}

static void advance_time(){
	const unsigned int ns_in_sec = 1000000000;
	const unsigned int ns_step = 10000;

	ossaddr->clock.tv_nsec += ns_step;
	if(ossaddr->clock.tv_nsec > ns_in_sec){
    ossaddr->clock.tv_sec += 1;
    ossaddr->clock.tv_nsec %= ns_in_sec;
  }
}

int parse_numbers(const char * datafile, unsigned int * numbers){
	int count = 0;
	char buf[100];

	FILE * fData = fopen(datafile, "r");
	while(fgets(buf, sizeof(buf), fData)){
		if(numbers){
			numbers[count] = atoi(buf);
		}
		count++;
	}
	fclose(fData);

	return count;
}

static int adder_loop(const unsigned int N, unsigned int len){

	int index = 0;
	num_users = num_procs = ext_procs = 0;
	max_users = (N / len);   // n/2 pairs of processes

	log_message(fLog, "N %d at time %d s %d ns\n", N, ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);


	while(ext_procs < max_users){

		if(	(num_procs < max_procs) &&
				(num_users < max_users)){

			if(index == ((max_users-1)*len) ){	//if last pair
				len += (N % len);									//if N is odd, add 1 to len
			}

			const pid_t pid = user_fork(index, len);
			if(pid > 0){
				index += len;

				num_users++;
				num_procs++;

			}else if(pid == -1){	//if error
				exit(1);
			}
		}

		advance_time();
		wait_users(ossaddr->users, max_users, 1);
	}

	return 0;
}

static int group_results(const unsigned int N, const unsigned int len){
	int i, j=1;
	for(i=len; i < N; i+=len){
		ossaddr->numbers[j++] = ossaddr->numbers[i];
	}
	return N / len;
}

int main(const int argc, char * const argv[]) {

	if(argc != 2){
		fprintf(stderr, "Usage: ./master <datafile>\n");
		return -1;
	}

	signal(SIGINT,	signal_handler);
  signal(SIGALRM, signal_handler);
	//signal(SIGCHLD, signal_handler);	//handler may interrupt log_message, and cause deadlock !!!

	alarm(100);

	fLog = fopen(ADDER_LOGFILE, "a");
	if(fLog == NULL){
		perror("fopen");
		return -1;
	}

	//count numbers
	int N = parse_numbers(argv[1], NULL);

	ossaddr = oss_init(N);
	if(ossaddr == NULL){
		return EXIT_FAILURE;
	}

	//put numbers in shared memory
	if(parse_numbers(argv[1], ossaddr->numbers) != N){
		oss_deinit();
		return -1;
	}

	//first adder uses group of size log(N)
	unsigned int len = (unsigned int)log((double)N);
	adder_loop(N, len);
	group_results(N, len);
	N /= len;

	//second adder uses groups of size 2
	while(N > 1){
		adder_loop(N, 2);
		N = group_results(N, 2);
	}

	log_message(fLog, "Finished with result %d at %lu.%lu\n", ossaddr->numbers[0], ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
	fclose(fLog);

	//wait_users(ossaddr->users, max_users, 0);
	oss_deinit();

	return EXIT_SUCCESS;
}
