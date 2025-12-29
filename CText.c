/* Includes */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <string.h>

/* Defines */
#define CTEXT_VERSION "0.0.1"
#define CTRL_KEY(k) (k & 0x1f)

enum editorKey{
  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_LEFT,
  ARROW_RIGHT,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/* Append Buffer */
struct abuff {
  char *b;
  int len;
};

#define ABUFF_INIT {NULL, 0}

void abAppend(struct abuff* ab, const char* s, int len){
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;

}

void abFree(struct abuff* ab){
  free(ab->b);
}

/* Prototypes */
void editor_clear_screen(void);

/* Data */
struct EditorState{
  int cx;
  int cy;
  int screenrows, screencols;
  struct termios orig_termios;
};

struct EditorState editorState;

/* Terminal */
void die(const char* s){
  editor_clear_screen();
  perror(s);
  exit(1);
}

void disable_raw_mode(void){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &editorState.orig_termios) == -1){
    die("tcsetattr");
  }
}

void enable_raw_mode(void) {
  if(tcgetattr(STDIN_FILENO, &editorState.orig_termios) == -1) die("tcsetattr"); atexit(disable_raw_mode);

  struct termios raw = editorState.orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL | ISTRIP | BRKINT);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr enable_raw_mode");
}

int editor_read_key(void){
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, sizeof(char))) != 1){
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c =='\x1b'){
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '['){
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~'){
          switch (seq[1]){
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      }else {
        switch (seq[1]){
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
        }
      }
    }
    return '\x1b';
  }
  else {
    return c;
  }
}

int get_cursor_position(int * rows, int* cols){
  char buf[32];
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  printf("\r\n");

  int i = 0;

  while (i < sizeof(buf)){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int get_window_size(int* rows, int* cols){
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return get_cursor_position(rows, cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* Output */
void editor_draw_rows(struct abuff *ab){
  int y;
  for (y = 0; y < editorState.screenrows; y++) {
    if (y == editorState.screenrows / 4) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
        "CText editor -- version %s", CTEXT_VERSION);
      if (welcomelen > editorState.screencols) welcomelen = editorState.screencols;
      int padding = (editorState.screencols - welcomelen) / 2;
      if (padding){
        abAppend(ab, "%", 1);
        padding--;
      }
      while(padding--) {
        abAppend(ab, " ", 1);
      }
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "%", 1);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < editorState.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editor_clear_screen(void){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editor_refresh_screen(void){
  struct abuff ab = ABUFF_INIT;
  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25l", 6);

  editor_draw_rows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editorState.cy + 1, editorState.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/* Input */

void editor_move_cursor(int key){
  switch (key) {
    case ARROW_DOWN:
      if (editorState.cy != editorState.screenrows - 1) editorState.cy++;
      break;
    case ARROW_UP:
      if (editorState.cy != 0) editorState.cy--;
      break;
    case ARROW_LEFT:
      if (editorState.cx != 0) editorState.cx--;
      break;
    case ARROW_RIGHT:
      if (editorState.cx != editorState.screenrows - 1) editorState.cx++;
      break;
  }
}

void editor_process_keypress(void){
  int c = editor_read_key();
  switch (c) {
    case CTRL_KEY('x'):
      editor_clear_screen();
      exit(0);
      break;
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editor_move_cursor(c);
      break;
    case PAGE_UP:
    case PAGE_DOWN:{
      int times = editorState.screenrows;
      while(times--){
        editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    }
    case HOME_KEY:
      editorState.cx = 0;
      break;
    case END_KEY:
      editorState.cx = editorState.screencols - 1;
      break;
    case DEL_KEY:
      break;
  }
}

/* Init */

void init_editor(void){
  editorState.cx = 0;
  editorState.cy = 0;
  if(get_window_size(&editorState.screenrows, &editorState.screencols) == -1) die("get_window_size");
}

int main(void){
  enable_raw_mode();
  init_editor();
  while(true){
    editor_refresh_screen();
    editor_process_keypress();
  }
  return 0;
}
