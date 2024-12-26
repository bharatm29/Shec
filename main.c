#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>

#define CTRL(x) ((x) & 0x1f)

#define QUIT CTRL('Q')
#define ESCAPE CTRL('[')

#define SHELL "[shec]$ "

int lines = 0;

void handleCommand(char** const command) {
    int fds[2];
    if (pipe(fds) == -1) { // Setup the pipe
        printw("Error creating pipe: %s\n", strerror(errno));
        return;
    }

    const int cpid = fork();

    if (cpid < 0) { // Error in fork
        printw("Fork error: %s\n", strerror(errno));
        return;
    }

    if (cpid == 0) { // Child process
        close(fds[0]); // child doesn't read
        dup2(fds[1], STDOUT_FILENO); // stdout -> child's write end
        dup2(fds[1], STDERR_FILENO); // stdout -> child's write end

        close(fds[1]); // close write end after redirecting

        execvp(command[0], command);
        perror("execvp failed");

        exit(1);
    } else { // Parent process
        close(fds[1]); // Close write end of the pipe
        char buffer[2]; // sizeof(1) doesn't work but this works???
        ssize_t bytesRead;

        while ((bytesRead = read(fds[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0'; // Null-terminate the buffer
            for (int i = 0; i < bytesRead; i++) {
                if (buffer[i] == '\n') {
                    lines++;
                }
                printw("%c", buffer[i]);
            }
            // printw("%s", buffer); // Print to the ncurses window
        }

        if (bytesRead == -1) {
            printw("Error reading from pipe: %s\n", strerror(errno));
        }

        close(fds[0]); // Close read end of the pipe
        int wstatus;
        waitpid(cpid, &wstatus, 0); // Wait for child process to finish

        if (!WIFEXITED(wstatus)) {
            printw("Child process exited abnormally!\n");
        }
    }
}

int main() {
    // Initialize ncurses window
    initscr();
    keypad(stdscr, TRUE);
    cbreak();
    noecho();
    move(0, 0);

    printw("%s", SHELL);

    int ch;
    while ((ch = getch()) != ERR) {
        // printw("{%s}", keyname(ch));
        #define ENTER '\n' // 10 is usually \n or \r
        switch (ch) {
            case QUIT:
            case ESCAPE: {
                exit(0);
            } break;
            case ENTER:
            case KEY_ENTER: {
                printw("Command execution here");
                lines += 2;
                move(lines, 0);
                printw(SHELL);
            } break;
            case KEY_BACKSPACE: {
                printw("delete a char");
            } break;
            default: {
                printw("%c", ch);
                break;
            }
        }
    }

    endwin();

    return 0;
}
