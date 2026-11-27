#include <stdint.h>

typedef enum {
    SEASHELL_STATUS_OK = 1,
    SEASHELL_STATUS_ERR = -1,
    SEASHELL_STATUS_EXIT = 0,
} seashell_status;

typedef struct {
    char** cmds;
    uint8_t capacity;
    uint8_t head;
    uint8_t tail;
    uint8_t size;
} seashell_cmdbuf;

// =----- circular buffer -----=
seashell_cmdbuf* seashell_init_msgbuf(seashell_cmdbuf *buf);
int seashell_cmdbuf_is_empty(seashell_cmdbuf *buf);
int seashell_cmdbuf_is_full(seashell_cmdbuf *buf);
void seashell_cmdbuf_push(seashell_cmdbuf *buf, char* cmd);
char* seashell_cmdbuf_pop(seashell_cmdbuf *buf);
char* seashell_cmdbuf_get(seashell_cmdbuf *buf, uint8_t i);
void seashell_printbuf(seashell_cmdbuf *buf);

// =----- logistics -----=
void seashell_print_startup_header(void);
void seashell_print_cursor(void);
char* seashell_cpyline(char* line);

// =----- main logic -----=
int seashell_loop(void);
char* seashell_readline(seashell_cmdbuf *history);
char** seashell_splitline(char *line);
int seashell_launch(char **args);
int seashell_execute(char **args);

// =----- built-ins -----=
int seashell_num_builtins();
int seashell_cd(char **args);
int seashell_help(char **args);
int seashell_exit(char **args);
