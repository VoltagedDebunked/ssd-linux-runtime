#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fs.h> // For BLKDISCARD/TRIM
#include <syslog.h>
#include <signal.h>
#include <asm/unistd.h> // Assembly system calls

#define BUFFER_SIZE 4096 // 4KB buffer for SSD performance

// Direct I/O Optimization with low-level assembly for system calls
void optimize_io(const char *filepath, void *buffer, size_t size) {
    int fd = open(filepath, O_RDWR | O_DIRECT | O_SYNC);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        return;
    }

    ssize_t ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "0"(__NR_pread64), "D"(fd), "S"(buffer), "d"(size), "r"(0)
        : "memory");

    if (ret == -1) {
        syslog(LOG_ERR, "Error in pread: %s", strerror(errno));
    } else {
        syslog(LOG_INFO, "SSD optimized read completed with assembly.");
    }

    close(fd);
}

// SSD TRIM management using ioctl with assembly for direct system call
void setup_trim(const char *device) {
    int fd = open(device, O_RDWR);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open device for TRIM: %s", strerror(errno));
        return;
    }

    unsigned long range[2] = {0, BUFFER_SIZE}; // Example trimming the first 4KB block
    int ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "0"(__NR_ioctl), "D"(fd), "S"(BLKDISCARD), "d"(range)
        : "memory");

    if (ret == -1) {
        syslog(LOG_ERR, "Failed to issue TRIM: %s", strerror(errno));
    } else {
        syslog(LOG_INFO, "SSD TRIM command issued via ioctl.");
    }

    close(fd);
}

// Monitor SSD health using S.M.A.R.T. (ATA pass-through commands)
void monitor_health(const char *device) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "smartctl -H %s", device);
    int ret = system(cmd);

    if (ret == 0) {
        syslog(LOG_INFO, "SSD health check completed successfully.");
    } else {
        syslog(LOG_ERR, "Failed to run S.M.A.R.T. health check.");
    }
}

void daemonize() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    openlog("ssd-linux-runtime", LOG_PID | LOG_CONS, LOG_DAEMON);
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Failed to create a new SID for the child process");
        exit(EXIT_FAILURE);
    }
    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "Failed to change directory to root");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    syslog(LOG_NOTICE, "SSD-Linux-Runtime daemon started successfully");
}

void signal_handler(int sig) {
    switch(sig) {
        case SIGHUP:
            syslog(LOG_NOTICE, "Hangup signal caught, reloading configuration...");
            break;
        case SIGTERM:
            syslog(LOG_NOTICE, "Termination signal caught, shutting down...");
            closelog();
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
    }
}

int main() {
    daemonize();
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    const char *filepath = "/dev/sda"; // Example device path
    char *buffer;

    if (posix_memalign((void **)&buffer, 512, BUFFER_SIZE) != 0) {
        syslog(LOG_ERR, "Failed to allocate aligned memory");
        return 1;
    }

    while (1) {
        optimize_io(filepath, buffer, BUFFER_SIZE);
        setup_trim(filepath);
        monitor_health(filepath);

        sleep(10);
    }

    free(buffer);
    return 0;
}