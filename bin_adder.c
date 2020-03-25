#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>	//atoi
#include <semaphore.h>
#include "common.h"

static OSS *ossaddr = NULL;
static FILE * fLog = NULL;

int main(const int argc, char * argv[]) {

	const unsigned int index = atoi(argv[1]);
	unsigned int len = atoi(argv[2]);

	ossaddr = oss_init(0);
	if(ossaddr == NULL){
		return 1;
	}

	fLog = fopen(ADDER_LOGFILE, "a");
	if(fLog == NULL){
		perror("fopen");
		return 2;
	}

	log_message(fLog, "Adder %d started at index %d, %d numbers\n", getpid(), index, len);
	while(len > 1){
		ossaddr->numbers[index] += ossaddr->numbers[index + --len];
	}
	log_message(fLog, "Adder %d finished with result %d\n", getpid(), ossaddr->numbers[index]);

	oss_deinit();
	fclose(fLog);
	return 0;

}
