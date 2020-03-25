#include <stdarg.h>

#define USER_LIMIT 20
struct user {
	pid_t pid;		/* PID of user process */
};


typedef struct {
	pid_t oss_pid;									/* PID of master process */
	sem_t mutex;										/* mutex for synchronized access */
	struct timespec clock;					/* oss time */

	struct user users[USER_LIMIT];

	unsigned int N; //size of numbers[]
	unsigned int numbers[1];
} OSS;

OSS * oss_init(const unsigned int numbers_count);
void oss_deinit();

#define ADDER_LOGFILE "adder_log"

int log_message(FILE * f, const char *fmt, ...);
