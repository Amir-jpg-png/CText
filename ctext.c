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
#include <time.h>
#include <stdarg.h>

/* Defines */
#define CTEXT_VERSION "0.0.1"
#define CTRL_KEY(k) (k & 0x1f)
#define TAB_STOP 8

enum editorKey
{
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
struct abuff
{
  char *b;
  int len;
};

#define ABUFF_INIT {NULL, 0}

void abAppend(struct abuff *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuff *ab)
{
  free(ab->b);
}

/* Prototypes */
void editorClearScreen(void);
void editorAppendRow(char *s, size_t len);

/* Data */

typedef struct erow
{
  int size;
  int rsize;
  char *chars;
  char* render;
} erow;

struct EditorState
{
  int cx, cy;
  int rx;
  int screenrows, screencols;
  int numrows;
  int rowoff, coloff;
  erow *row;
  char* filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
};

struct EditorState editorState;

/* Terminal */
void die(const char *s)
{
  editorClearScreen();
  perror(s);
  exit(1);
}

void disableRawMode(void)
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editorState.orig_termios) == -1)
  {
    die("tcsetattr");
  }
}

void enableRawMode(void)
{
  if (tcgetattr(STDIN_FILENO, &editorState.orig_termios) == -1)
    die("tcsetattr");
  atexit(disableRawMode);

  struct termios raw = editorState.orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL | ISTRIP | BRKINT);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr enableRawMode");
}

int editorReadKey(void)
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, sizeof(char))) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b')
  {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[')
    {
      if (seq[1] >= '0' && seq[1] <= '9')
      {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~')
        {
          switch (seq[1])
          {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      }
      else
      {
        switch (seq[1])
        {
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
  else
  {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols)
{
  char buf[32];
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  printf("\r\n");

  int i = 0;

  while (i < sizeof(buf))
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols)
{
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  }
  else
  {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* file i/o  */

void editorOpen(char *filename)
{
  free(editorState.filename);
  editorState.filename = strdup(filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1)
  {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
    {
      linelen--;
      editorAppendRow(line, linelen);
    }
  }
  free(line);
  fclose(fp);
}

/* Row operations */
int editorRowCxtoRx(erow* row, int cx){
  int rx = 0;
  int j;
  for (j= 0; j < cx; j++){
    if (row->chars[j] == '\t'){
      rx += (TAB_STOP - 1) - (rx % TAB_STOP);
    }
    rx++;
  }
  return rx;
}

/* Output */

void editorScroll(void)
{
  editorState.rx = 0;
  if (editorState.cy < editorState.numrows){
    editorState.rx = editorRowCxtoRx(&editorState.row[editorState.cy], editorState.cx);
  }
  if (editorState.cy < editorState.rowoff)
  {
    editorState.rowoff = editorState.cy;
  }

  if (editorState.cy >= editorState.rowoff + editorState.screenrows)
  {
    editorState.rowoff = editorState.cy - editorState.screenrows + 1;
  }

  if (editorState.rx < editorState.coloff){
    editorState.coloff = editorState.rx;
  }

  if (editorState.rx >= editorState.coloff + editorState.screencols){
    editorState.coloff = editorState.rx - editorState.screencols + 1;
  }
}

void editorUpdateRow(erow* row){
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++){
    if (row->chars[j] == '\t') tabs++;
  }
  free(row->render);
  row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);
  int idx = 0;
  for (j = 0; j < row->size; j++){
    if (row->chars[j] == '\t'){
      row->render[idx++] = ' ';
      while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len)
{
  editorState.row = realloc(editorState.row, sizeof(erow) * (editorState.numrows + 1));
  int at = editorState.numrows;
  editorState.row[at].size = len;
  editorState.row[at].chars = malloc(len + 1);
  memcpy(editorState.row[at].chars, s, len);
  editorState.row[at].chars[len] = '\0';

  editorState.row[at].rsize = 0;
  editorState.row[at].render = NULL;
  editorUpdateRow(&editorState.row[at]);

  editorState.numrows++;
}

void editorDrawRows(struct abuff *ab)
{
  int y;
  for (y = 0; y < editorState.screenrows; y++)
  {
    int filerow = y + editorState.rowoff;
    if (filerow >= editorState.numrows)
    {
      if (editorState.numrows == 0 && y == editorState.screenrows / 3)
      {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "\x1b[32mCText\x1b[0m editor -- version %s", CTEXT_VERSION);
        if (welcomelen > editorState.screencols)
          welcomelen = editorState.screencols;
        int padding = (editorState.screencols - welcomelen) / 2;
        if (padding)
        {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      }
      else
      {
        abAppend(ab, "~", 1);
      }
    }
    else
    {
      int len = editorState.row[filerow].rsize - editorState.coloff;
      if (len < 0) len = 0;
      if (len > editorState.screencols) len = editorState.screencols;
      abAppend(ab, &editorState.row[filerow].render[editorState.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuff* ab){
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines", editorState.filename ? editorState.filename : "[No Name]", editorState.numrows);
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", editorState.cy + 1, editorState.numrows);
  if (len > editorState.screencols) len = editorState.screencols;
  abAppend(ab, status, len);
  while (len < editorState.screencols) {
    if (editorState.screencols - len == rlen){
      abAppend(ab, rstatus, rlen);
      break;
    }
    abAppend(ab, " ", 1);
    len++;
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorSetStatusMessage(const char* fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(editorState.statusmsg, sizeof(editorState.statusmsg), fmt, ap);
  va_end(ap);
  editorState.statusmsg_time = time(NULL);
}

void editorDrawMessageBar(struct abuff* ab){
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(editorState.statusmsg);
  if (msglen > editorState.screencols) msglen = editorState.screencols;
  if (msglen && time(NULL) - editorState.statusmsg_time < 5){
    abAppend(ab, editorState.statusmsg, msglen);
  }
}

void editorClearScreen(void)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorRefreshScreen(void)
{
  editorScroll();
  struct abuff ab = ABUFF_INIT;
  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25l", 6);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (editorState.cy - editorState.rowoff) + 1, (editorState.rx - editorState.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/* Input */

void editorMoveCursor(int key)
{
  erow *row = (editorState.cy >= editorState.numrows) ? NULL : &editorState.row[editorState.cy];
  switch (key)
  {
   case ARROW_DOWN:
     if (editorState.cy != editorState.rowoff - 1) editorState.cy++;
     break;
  case ARROW_UP:
    if (editorState.cy != 0)
      editorState.cy--;
    break;
  case ARROW_LEFT:
    if (editorState.cx != 0)
      editorState.cx--;
    else if (editorState.cy > 0 && editorState.numrows > 0){
      editorState.cy--;
      editorState.cx = editorState.row[editorState.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if(row && editorState.cx < row->size)
      editorState.cx++;
    else if (row && editorState.cx == row->size && editorState.numrows > 0){
      editorState.cy++;
      editorState.cx = 0;
    }
    break;
  }

  row = (editorState.cy >= editorState.numrows) ? NULL : &editorState.row[editorState.cy];
  int rowlen = row ? row->size : 0;
  if (editorState.cx > rowlen){
    editorState.cx = rowlen;
  }
}

void editorProcessKeypress(void)
{
  int c = editorReadKey();
  switch (c)
  {
  case CTRL_KEY('x'):
    editorClearScreen();
    exit(0);
    break;
  case ARROW_DOWN:
  case ARROW_UP:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  case PAGE_UP:
  case PAGE_DOWN:
  {
    if (c == PAGE_UP){
      editorState.cy = editorState.rowoff;
    } else if (c == PAGE_DOWN){
      editorState.cy = editorState.rowoff + editorState.screenrows - 1;
      if (editorState.cy > editorState.numrows){
        editorState.cy = editorState.numrows;
      }
    }
    int times = editorState.screenrows;
    while (times--){
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
  }
  case HOME_KEY:
    editorState.cx = 0;
    break;
  case END_KEY:
    if (editorState.cy < editorState.numrows){
      editorState.cx = editorState.row[editorState.cy].size;
    }
    break;
  case DEL_KEY:
    break;
  }
}

/* Init */

void initEditor(void)
{
  editorState.numrows = 0;
  editorState.row = NULL;
  editorState.cx = 0;
  editorState.rx = 0;
  editorState.cy = 0;
  editorState.rowoff = 0;
  editorState.coloff = 0;
  editorState.filename = NULL;
  editorState.statusmsg[0] = '\0';
  editorState.statusmsg_time = 0;
  if (getWindowSize(&editorState.screenrows, &editorState.screencols) == -1)
    die("getWindowSize");
  editorState.screenrows -= 2;
}

int main(int argc, char *argv[])
{
  enableRawMode();
  initEditor();
  if (argc >= 2)
  {
    editorOpen(argv[1]);
  }
  editorSetStatusMessage("HELP: Ctrl-X = quit");
  while (true)
  {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  editorClearScreen();
  return 0;
}
