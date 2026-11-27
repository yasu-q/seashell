#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>

#include "seashell.h"

#define SEASHELL_RL_BUFSIZE 512
#define SEASHELL_TOK_BUFSIZE 64
#define SEASHELL_CD_BUFSIZE 512
#define SEASHELL_MSG_BUF_CAPACITY 64
#define SEASHELL_TOK_DELIM " \t\r\n\a"

char *help_msg =    "seashell\n\n"
                    "list of built-in commands:\n"
                    "   help\n"
                    "   cd [dir]\n"
                    "   exit";

char *header_msg =  "\033[36m▄█████ ██████ ▄████▄ ▄█████ ██  ██ ██████ ██     ██\033[0m\n"     
                    "\033[36m▀▀▀▄▄▄ ██▄▄   ██▄▄██ ▀▀▀▄▄▄ ██████ ██▄▄   ██     ██\033[0m\n"     
                    "\033[36m█████▀ ██▄▄▄▄ ██  ██ █████▀ ██  ██ ██▄▄▄▄ ██████ ██████\033[0m";

char *builtin_commands[] = {
    "cd",
    "exit",
    "help",
    "quit"
};

int (*builtin_command_func[]) (char **) = {
    &seashell_cd,
    &seashell_exit,
    &seashell_help,
    &seashell_exit      // quit redirects here
};

// =----- debug -----=

#ifdef DEBUG
static void print_args(char **args) {
    int i = 0;

    if (args == NULL) {
        printf("args was NULL");
        return;
    }

    printf("content of args:\n");
    while (args[i]) {
        printf("   %s\n", args[i]);
        i += 1;
    }
    if (i == 0) {
        printf("   <args was empty>\n");
    }
}
#endif

// =----- circular buffer -----=

seashell_cmdbuf* seashell_init_msgbuf(seashell_cmdbuf *buf) {
    if (!buf) {
        fprintf(stderr, "seashell: buf was null");
        return NULL;
    }

    buf->cmds = malloc(sizeof(char*) * SEASHELL_MSG_BUF_CAPACITY);
    if (!buf->cmds) {
        perror("seashell");
        free(buf);
        return NULL;
    }

    buf->capacity = SEASHELL_MSG_BUF_CAPACITY;
    buf->size = 0;
    buf->tail = 0;
    buf->head = 0;

    return buf;
}

int seashell_cmdbuf_is_empty(seashell_cmdbuf *buf) {
    if (!buf) {
        fprintf(stderr, "seashell: cmdbuf is null");
        return -1;
    }
    return buf->size == 0;
}

int seashell_cmdbuf_is_full(seashell_cmdbuf *buf) {
    if (!buf) {
        fprintf(stderr, "seashell: cmdbuf is null");
        return -1;
    }
    return buf->size == buf->capacity;
}

// Assume cmd is already alloacted somewhere or is a const string
void seashell_cmdbuf_push(seashell_cmdbuf *buf, char* cmd) {
    if (!buf) {
        fprintf(stderr, "seashell: cmdbuf is null");
        return;
    }

    buf->cmds[buf->head] = cmd;
    buf->head = (buf->head + 1) % buf->capacity;

    // Check if overwrote head
    if (seashell_cmdbuf_is_full(buf)) {
        buf->tail = (buf->tail + 1) % buf->capacity;
    } else {
        buf->size += 1;
    }
}

char* seashell_cmdbuf_pop(seashell_cmdbuf *buf) {
    char* cmd = NULL;

    if (!buf) {
        fprintf(stderr, "seashell: pop from empty buffer");
        return NULL;
    }

    cmd = buf->cmds[buf->tail];
    buf->tail = (buf->tail + 1) % buf->capacity;
    buf->size -= 1;
    
    return cmd;
}

char* seashell_cmdbuf_get(seashell_cmdbuf *buf, uint8_t i) {
    uint8_t ind;

    if (!buf) {
        fprintf(stderr, "seashell: cmdbuf is null");
        return NULL;
    }
    if (i >= buf->size) {
        fprintf(stderr, "seashell: index too large");
        return NULL;
    }

    ind = (buf->tail + i) % buf->capacity;

    return buf->cmds[ind];
}

#ifdef DEBUG
void seashell_printbuf(seashell_cmdbuf *buf) {
    if (!buf) {
        return;
    }

    printf("msg buffer contents:\n");
    for (int i = 0; i < buf->size; i += 1) {
        printf("   %s\n", buf->cmds[i]);
    }
    if (buf->size == 0) {
        printf("   <empty buffer>\n");
    }
}
#endif

// =----- logistics -----=

void seashell_print_startup_header() {
    printf("\033[H\033[2J"); // clear terminal. scrolls down instead of clearing buf
    printf("%s\n\n", header_msg);
    printf("for a list of commands, enter 'help'\n\n\n");
}

void seashell_print_cursor() {
    char current_directory[SEASHELL_CD_BUFSIZE];
    char *home_path = NULL;
    char *display_path = NULL;
    size_t home_len = 0;

    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        perror("seashell:");
        return;
    }

    home_path = getenv("HOME");
    home_len = strlen(home_path);

    display_path = current_directory + home_len;
    // Weird escape characters are for colors
    printf("\033[36mseashell:\033[0m\033[37m~%s\033[0m$ ", display_path);
    // "\033[<COLOR NUMBER>m<TEXT>\033[0m"

    fflush(stdout);
}

// Line will be allocated
char* seashell_cpyline(char* line) {
    size_t line_len = 0;
    char* buf_line = NULL;

    if (!line) {
        fprintf(stderr, "seashell: line was NULL");
        return NULL;
    }
    line_len = strlen(line);
    buf_line = malloc(line_len + 1);
    if (!buf_line) {
        perror("seashell");
        return NULL;
    }

    strcpy(buf_line, line);

    return buf_line;
}

// =----- main logic -----=

int seashell_loop(void) {
    char *line = NULL;
    char *buf_line = NULL;
    char **args = NULL;
    int status = 0;

    seashell_cmdbuf buf;
    seashell_init_msgbuf(&buf);

    do {
        seashell_print_cursor();
        line = seashell_readline(&buf);

        // Buffer
        buf_line = seashell_cpyline(line);
        seashell_cmdbuf_push(&buf, buf_line);

        args = seashell_splitline(line);
        status = seashell_execute(args);

        free(line);
        free(args);
    } while (status);

    return status;
}

char* seashell_readline(seashell_cmdbuf *history) {
    const int bufsize = SEASHELL_RL_BUFSIZE;
    int buf_position = 0;
    int c;
    int history_index;
    int draft_saved = 0;
    
    char *buf;
    const char *replacement = NULL;
    char draft[SEASHELL_RL_BUFSIZE] = "";

    struct termios old_terminal;
    struct termios new_terminal;

    if (!history) {
        fprintf(stderr, "seashell: history was null");
        return NULL;
    }

    buf = malloc(bufsize);
    if (!buf) {
        perror("malloc fail in seashell_readline()");
        return NULL;
    }

    if (tcgetattr(STDIN_FILENO, &old_terminal) == -1) {
        perror("tcgetattr");
        free(buf);
        return NULL;
    }
    new_terminal = old_terminal;
    new_terminal.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal) == -1) {
        perror("tcsetattr");
        free(buf);
        return NULL;
    }

    history_index = history->size;
    buf[0] = '\0';

    while (1) {
        c = getchar();

        if (c == '\n' || c == EOF) {
            // At new line, return line
            buf[buf_position] = '\0';

            // Manually add a newline since we don't putchar later
            if (c == '\n') {
                putchar('\n');
            }
            // Reset terminal to default
            if (tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal) == -1) {
                perror("tcsetattr");
            }

            return buf;
        } else if (c == 127 || c == '\b') {
            if (buf_position > 0) {
                buf_position -= 1;
                buf[buf_position] = '\0';

                // Visually backspace
                printf("\b \b");
                fflush(stdout);

                history_index = history->size;
                memcpy(draft, buf, buf_position + 1);
                draft_saved = 1;
            }
        } else if (c == '\033') {
            int seq_prefix = getchar();
            int seq_code = getchar();
            replacement = NULL;

            if (seq_prefix == '[') {
                switch (seq_code) {
                    case 'A':
                        // Up arrow
                        if (history->size > 0) {
                            // Save current line if not saved
                            if (!draft_saved) {
                                memcpy(draft, buf, buf_position + 1);
                                draft_saved = 1;
                            }
                            // Prev line in history
                            if (history_index > 0) {
                                history_index -= 1;
                            }
                            replacement = seashell_cmdbuf_get(
                                history, 
                                (uint8_t)history_index
                            );
                        }
                        break;
                    case 'B':
                        // Down arrow
                        if (history_index < history->size) {
                            history_index += 1;
                            if (history_index == history->size) {
                                // If alreay at end of queue, stay
                                replacement = draft;
                            } else {
                                replacement = seashell_cmdbuf_get(
                                    history, 
                                    (uint8_t)history_index
                                );
                            }
                        }
                        break;
                }
            }

            // If the buffer was replaced (i.e. arrow key pressed)
            if (replacement) {
                // Delete line
                while (buf_position > 0) {
                    printf("\b \b");
                    buf_position -= 1;
                }
                // Copy replacement to buffer
                while (buf_position < bufsize - 1 && replacement[buf_position] != '\0') {
                    buf[buf_position] = replacement[buf_position];
                    // Print line to terminal
                    putchar(buf[buf_position]);
                    buf_position += 1;
                }
                buf[buf_position] = '\0';
                fflush(stdout);
            }
        } else {
            // Add char to buf and echo to terminal
            if (buf_position < bufsize - 1) {
                buf[buf_position] = (char)c;
                buf_position += 1;
                buf[buf_position] = '\0';
                putchar(c);
                history_index = history->size;
                memcpy(draft, buf, buf_position + 1);
                draft_saved = 1;
            } else {
                // Buffer full
                putchar('\a');
            }
            fflush(stdout);
        }
    }
}

// This will modify line
char **seashell_splitline(char *line) {
    int bufsize = SEASHELL_TOK_BUFSIZE;
    int buf_position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;
    char **temp_buff = NULL;

    if (!tokens) {
        perror("malloc fail in seashell_splitline()");
        return NULL;
    }

    // Read first token
    token = strtok(line, SEASHELL_TOK_DELIM);

    while (token != NULL) {
        tokens[buf_position] = token;
        buf_position += 1;

        // Resizing logic
        if (buf_position >= bufsize) {
            bufsize += SEASHELL_RL_BUFSIZE;
            temp_buff = realloc(tokens, bufsize * sizeof(char*));

            if (!temp_buff) {
                free(tokens);
                perror("realloc fail in seashell_splitline()");
                return NULL;
            }

            tokens = temp_buff;
        }

        // Next token
        token = strtok(NULL, SEASHELL_TOK_DELIM); // next token
    }

    tokens[buf_position] = NULL; // NULL to mark end
    return tokens;
}

int seashell_launch(char **args) {
    pid_t child_pid;
    int status;

    child_pid = fork();

    if (child_pid == -1) {
        perror("fork fail in seashell_launch()");
        return SEASHELL_STATUS_ERR;
    }
    if (child_pid == 0) { // Child process
        // Launch new process
        if (execvp(args[0], args) == -1) {
            perror("seashell");
        }
        // Kill child
        exit(EXIT_FAILURE);
    } else { // Parent process
        // Wait for child to complete
        do {
            // Make checking the return is not a bad idea later
            waitpid(child_pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    } 

    return 1;
}

int seashell_execute(char **args) {
    if (args[0] == NULL) {
        // Empty command
        return SEASHELL_STATUS_OK;
    }

    // Check if the command is a built-in
    for (int i = 0; i < seashell_num_builtins(); i += 1) {
        if (strcmp(args[0], builtin_commands[i]) == 0) {
            return (*builtin_command_func[i])(args);
        }
    }

    return seashell_launch(args);
}

// =----- builtin commands -----=

int seashell_num_builtins() {
    return sizeof(builtin_commands) / sizeof(char*);
}

int seashell_cd(char **args) {
    if (args == NULL) {
        fprintf(stderr, "seashell: args was NULL in seashell_cd()\n");
        return SEASHELL_STATUS_ERR;
    }
    if (args[1] == NULL) {
        fprintf(stderr, "seashell: expected argument for cd\n");
        return SEASHELL_STATUS_ERR;
    }
    if (chdir(args[1]) != 0) {
        perror("seashell");
        return SEASHELL_STATUS_ERR;
    }

    return SEASHELL_STATUS_OK;
}

int seashell_help(char **args) {
    printf("%s\n", help_msg);
    return SEASHELL_STATUS_OK;
}   

int seashell_exit(char **args) {
    printf("seashell: cya\n");
    return SEASHELL_STATUS_EXIT;
}
