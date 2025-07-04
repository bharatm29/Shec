#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>


#define QUIT CTRL('d')
#define ESCAPE CTRL('[')
#define KEY_BACKSPACE 127

#define SHELL "[shec]$ "

#define MOVE_DOWN(LINE) printf("\E[%dB", LINE);
#define MOVE_DOWN_START(LINE) printf("\E[%dE", LINE);

typedef struct {
    char** data; // its basically a pointer to an array of strings
    size_t len;
    size_t capacity;
} DA;

DA* MAKE_DA() {
    DA* da = malloc(sizeof(DA) * 1); // allocate on heap (mf C)
    da->capacity = 10;
    da->len = 0;
    da->data = malloc(sizeof(char*) * da->capacity);

    return da;
}

void DA_APPEND(DA* da, char* str) {
    if (da->len == da->capacity) {
        da->capacity *= 2;
        char** data = malloc(sizeof(char*) * da->capacity);
        memmove(data, da->data, da->len);
    }

    da->data[da->len++] = str;
}

char** const parseCommand(char* const prompt) {
    DA* da = MAKE_DA();
    char* tok = strtok(prompt, " ");
    if (strcmp(tok, "exit") == 0) {
        exit(0);
    }

    DA_APPEND(da, tok);

    while ((tok = strtok(NULL, " ")) != NULL) {
        DA_APPEND(da, tok);
    }

    return da->data;
}

void enableRawMode();
void handleCommand(char* const prompt) {
    char** command = parseCommand(prompt);

    int fds[2];
    if (pipe(fds) == -1) { // Setup the pipe
        printf("Error creating pipe: %s\n", strerror(errno));
        return;
    }

    const int cpid = fork();

    if (cpid < 0) { // Error in fork
        printf("Fork error: %s\n", strerror(errno));
        return;
    }

    if (cpid == 0) { // Child process
        close(fds[0]);

        dup2(fds[1], STDOUT_FILENO); // stdout -> child's write end
        dup2(fds[1], STDERR_FILENO); // stderr -> child's write end

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
            printf("%s", buffer);
        }

        if (bytesRead == -1) {
            printf("Error reading from pipe: %s\n", strerror(errno));
        }

        close(fds[0]); // Close read end of the pipe
        int wstatus;
        waitpid(cpid, &wstatus, 0); // Wait for child process to finish

        if (!WIFEXITED(wstatus)) {
            printf("Child process exited abnormally!\n");
        }

        enableRawMode(); // since execvp changes terminal settings, we have to call this shit here again
    }
}


struct termios orig_termios;

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);

    atexit(disableRawMode);

    struct termios raw = orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    enableRawMode();

    printf("%s", SHELL);

    int ch;
    char* prompt = malloc(sizeof(char) * 1024);
    size_t prompt_t = 0;

    DA* history = MAKE_DA();
    size_t hist_back = 0;

    while (read(STDIN_FILENO, &ch, 1) == 1) {
        switch (ch) {
            case QUIT: {
                printf("\n");
                exit(0);
            } break;
            case CTRL('h'):
            case KEY_BACKSPACE: {
                if (prompt_t <= 0) break;

                prompt_t--;

                printf("\E[1D \E[1D");
            } break;
            case CTRL('l'): { // clear screen
                printf("\E[H\E[2J");
                printf(SHELL);
            } break;

            case ESCAPE: {
                printf("Something happend\n");
            } break;

            case '\n': // ENTER
            case '\r': { // \n and \r since its byte-by-byte input in raw mode
                if (prompt_t > 0) {
                    MOVE_DOWN_START(1);

                    prompt[prompt_t] = '\0';

                    DA_APPEND(history, strndup(prompt, prompt_t + 1));

                    handleCommand(prompt);
                    hist_back = 0;
                }

                prompt_t = 0;
                MOVE_DOWN_START(1);
                printf(SHELL);
            } break;
            default: {
                if (!isprint(ch)) break;

                if (prompt_t >= 1024) {
                    printf("%s\n", "Reached max character limit of 1024 chars for shell promt");
                    exit(130);
                }

                prompt[prompt_t++] = ch;
                printf("%c", ch);
                break;
            }
        }
    }

    return 0;
}
