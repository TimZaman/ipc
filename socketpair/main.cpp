#include <iostream>
#include <vector>
#include <semaphore.h>
#include <thread> 
#include <unistd.h>  // usleep
#include <sys/socket.h>


void child(int socket) {
    // const char hello[] = "hello parent, I am child";
    printf("child socket=%d\n", socket);
    std::string hello = "X";
    for (int i=0; i<10; i++) {
        usleep(1000000);
        
        

        write(socket, hello.c_str(), hello.length()+1); /* NB. this includes nul */
        /* go forth and do childish things with this end of the pipe */

        char buf[1024];
        int n = read(socket, buf, sizeof(buf));
        printf("child received '%.*s'\n", n, buf);
        hello = std::string(buf) + " c" + std::to_string(i);
    }
}


void parent(int socket) {
    /* do parental things with this end, like reading the child's message */
    // const char hello[] = "hello parent, I am child";
    printf("parent socket=%d\n", socket);
    for (int i=0; i<10; i++) {
        char buf[1024];
        int n = read(socket, buf, sizeof(buf));
        printf("parent received '%.*s'\n", n, buf);

        std::string hello = std::string(buf) + " p" + std::to_string(i);

        write(socket, hello.c_str(), hello.length()+1);
    }
}


int main() {
    int fd[2];
    static const int parentsocket = 0;
    static const int childsocket = 1;
    pid_t pid;

    /* 1. call socketpair ... */
    socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);

    /* 2. call fork ... */
    pid = fork();
    if (pid == 0) { /* 2.1 if fork returned zero, you are the child */
        // close(fd[parentsocket]); /* Close the parent file descriptor */
        child(fd[childsocket]);
        // usleep(10000000);
    } else { /* 2.2 ... you are the parent */
        // close(fd[childsocket]); /* Close the child file descriptor */
        parent(fd[parentsocket]);
    }
    exit(0); /* do everything in the parent and child functions */
}
