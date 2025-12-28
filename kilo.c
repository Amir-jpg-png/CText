#include <unistd.h>

int main(void){
  char c;
  while(read(STDIN_FILENO, &c, sizeof(typeof(c))) == 1);
  return 0;
}
