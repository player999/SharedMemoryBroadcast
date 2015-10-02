#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#define LOCKFILE "/var/run/lock/vitstreaming.lock"

#define IMWIDTH 1920
#define IMHEIGHT 1080

char _T_framebuffer[IMWIDTH * IMHEIGHT] = {0};

int main(int argc, char *argv[])
{
	int pid;
	FILE *fh;
	int md;
	int counter = 0;

	/* Find streaming client pid */
	fh = fopen(LOCKFILE, "r");
	if (NULL != fh) {
		fscanf(fh, "%d", &pid);
		fclose(fh);
		if (kill(pid, 0)) {
			printf("Streaming client not found\n");
			return -1;
		}
	}
	else {
		printf("Streaming client not found\n");
		return -1;
	}

	while(1) {
		int bytes;
		_T_framebuffer[counter] = 0xFF;
		counter++;
		md = shm_open("vit_image", O_CREAT | O_RDWR,  S_IRUSR | S_IWUSR |
			S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		write(md, _T_framebuffer, IMWIDTH * IMHEIGHT);
		fsync(md);
		close(md);
		kill(pid, SIGUSR1);
		usleep(40000);//40ms
	}

}