#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("%s\n", "A shell??");
    fflush(stdout); // flushing the inital strings

    char* command[] = {"ls", NULL};
    pid_t wstatus, w; // flags for concurrecy

    const int cpid = fork();

    if (cpid < 0) { // error
        printf("%s\n", strerror(errno));
        exit(1);
    }

    if (cpid == 0) { // child process
        execvp(command[0], command);
        exit(0); // exit normally
    } else { // parent process
        w = waitpid(cpid, &wstatus, WCONTINUED | WUNTRACED);

        if (w == -1 || !WIFEXITED(wstatus)) {
            printf("%s\n", "Child process exited abnormally!");
        }
    }

    return 0;
}
