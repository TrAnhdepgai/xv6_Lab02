#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

void
fail(char *msg)
{
  printf("sysinfotest: FAIL: %s\n", msg);
  exit(1);
}

int
main(int argc, char *argv[])
{
  struct sysinfo info1, info2, info3;

  printf("sysinfotest: starting\n");

  if(sysinfo(&info1) < 0)
    fail("sysinfo failed");
  printf("sysinfotest: freemem=%l bytes, nproc=%l\n", info1.freemem, info1.nproc);

  if(info1.freemem == 0)
    fail("freemem should be > 0");

  // Allocate 1 page and verify freemem decreases
  char *p = sbrk(PGSIZE);
  if(p == (char *)-1)
    fail("sbrk(PGSIZE) failed");

  if(sysinfo(&info2) < 0)
    fail("sysinfo after sbrk failed");
  if(info2.freemem > info1.freemem - PGSIZE)
    fail("freemem did not decrease after sbrk");

  // Deallocate 1 page and verify freemem recovers
  if(sbrk(-PGSIZE) == (char *)-1)
    fail("sbrk(-PGSIZE) failed");

  if(sysinfo(&info3) < 0)
    fail("sysinfo after sbrk(-PGSIZE) failed");
  if(info3.freemem != info1.freemem)
    fail("freemem did not match after freeing page");

  // Verify nproc increments on fork
  int pid = fork();
  if(pid < 0)
    fail("fork failed");
  if(pid == 0){
    struct sysinfo child_info;
    if(sysinfo(&child_info) < 0)
      exit(1);
    if(child_info.nproc != info1.nproc + 1)
      exit(1);
    exit(0);
  } else {
    int status;
    wait(&status);
    if(status != 0)
      fail("nproc check in child failed");
  }

  // Invalid address should return -1
  if(sysinfo((struct sysinfo *)0xffffffffffffffffL) >= 0)
    fail("sysinfo with invalid address should fail");

  printf("sysinfotest: OK\n");
  exit(0);
}
