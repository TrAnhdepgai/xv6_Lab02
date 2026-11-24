#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

void
fail(char *msg)
{
  printf("pgaccesstest: FAIL: %s\n", msg);
  exit(1);
}

int
main(int argc, char *argv[])
{
  char *buf;
  unsigned int abits;

  printf("pgaccesstest: starting\n");

  buf = malloc(32 * PGSIZE);
  if(buf == 0)
    fail("malloc failed");

  // Clear any existing access bits
  if(pgaccess(buf, 32, &abits) < 0)
    fail("initial pgaccess failed");

  // Access pages 1, 2, 30
  buf[PGSIZE * 1] += 1;
  buf[PGSIZE * 2] += 1;
  buf[PGSIZE * 30] += 1;

  if(pgaccess(buf, 32, &abits) < 0)
    fail("second pgaccess failed");
  if(abits != ((1U << 1) | (1U << 2) | (1U << 30)))
    fail("incorrect access bits set for pages 1, 2, 30");

  // Verify PTE_A was cleared after reading
  if(pgaccess(buf, 32, &abits) < 0)
    fail("third pgaccess failed");
  if(abits != 0)
    fail("access bits were not cleared");

  // Access boundary pages 0 and 31
  buf[0] += 1;
  buf[PGSIZE * 31] += 1;
  if(pgaccess(buf, 32, &abits) < 0)
    fail("boundary pgaccess failed");
  if(abits != ((1U << 0) | (1U << 31)))
    fail("incorrect access bits set for boundary pages 0 and 31");

  // Invalid arguments should return -1
  if(pgaccess(buf, -1, &abits) >= 0)
    fail("negative len should fail");
  if(pgaccess(buf, 33, &abits) >= 0)
    fail("len > 32 should fail");
  if(pgaccess(buf, 1, (void *)0xffffffffffffffffL) >= 0)
    fail("invalid mask address should fail");

  free(buf);
  printf("pgaccesstest: OK\n");
  exit(0);
}
