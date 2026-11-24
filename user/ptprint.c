#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: ptprint command [args...]\n");
    exit(1);
  }
  setvmprint(1);
  exec(argv[1], argv + 1);
  setvmprint(0);  // exec lỗi thì tắt cờ
  fprintf(2, "ptprint: exec %s failed\n", argv[1]);
  exit(1);
}