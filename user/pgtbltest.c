#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

//
// pgtbltest: test cho Lab Page Table.
//
//   pgtbltest              chay cac test cua do an (pgaccess + sysinfo)
//   pgtbltest <ten> ...    chi chay cac test duoc liet ke
//   pgtbltest ugetpid      test ugetpid goc cua MIT (can map trang USYSCALL,
//                          khong nam trong yeu cau do an nen mac dinh khong chay)
//
// Moi test chay trong mot tien trinh con rieng, nen mot test bi loi (hoac
// bi kernel kill vi page fault) khong lam dung cac test con lai.
//

#define NPAGES 32

char *testname = "???";

void
err(char *why)
{
  printf("pgtbltest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

// In mask thuc te va mask mong doi roi bao loi.
void
err_mask(char *why, unsigned int got, unsigned int want)
{
  printf("pgtbltest: %s: got abits=0x%x, want 0x%x\n", testname, got, want);
  err(why);
}

// Cap phat n trang moi bang sbrk, dia chi bat dau duoc can theo PGSIZE.
// Cac trang nay chua bi user-space cham vao lan nao (kernel chi memset
// qua dia chi vat ly), nen bit PTE_A cua chung phai bang 0.
char *
alloc_pages(int n)
{
  uint64 cur = (uint64)sbrk(0);
  uint64 pad = PGROUNDUP(cur) - cur;
  if(pad != 0 && sbrk(pad) == (char *)-1)
    err("sbrk (align) failed");
  char *p = sbrk(n * PGSIZE);
  if(p == (char *)-1)
    err("sbrk failed");
  if(((uint64)p % PGSIZE) != 0)
    err("sbrk returned unaligned address");
  return p;
}

// Goi pgaccess va bao loi neu syscall that bai.
unsigned int
scan(char *base, int len)
{
  unsigned int abits = 0xdeadbeef;
  if(pgaccess(base, len, &abits) < 0)
    err("pgaccess returned -1");
  return abits;
}

// Doc 1 byte (khong ghi). volatile de compiler khong bo lenh doc.
void
touch_read(char *p)
{
  volatile char c = *(volatile char *)p;
  (void)c;
}

void
touch_write(char *p)
{
  *(volatile char *)p += 1;
}

// ---------------------------------------------------------------------
// Test goc cua MIT
// ---------------------------------------------------------------------

void
ugetpid_test()
{
  int i;

  printf("ugetpid_test starting\n");
  testname = "ugetpid_test";

  for (i = 0; i < 64; i++) {
    int ret = fork();
    if (ret != 0) {
      wait(&ret);
      if (ret != 0)
        exit(1);
      continue;
    }
    if (getpid() != ugetpid())
      err("missmatched PID");
    exit(0);
  }
  printf("ugetpid_test: OK\n");
}

void
pgaccess_test()
{
  char *buf;
  unsigned int abits;
  printf("pgaccess_test starting\n");
  testname = "pgaccess_test";
  buf = malloc(32 * PGSIZE);
  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess failed");
  buf[PGSIZE * 1] += 1;
  buf[PGSIZE * 2] += 1;
  buf[PGSIZE * 30] += 1;
  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess failed");
  if (abits != ((1 << 1) | (1 << 2) | (1 << 30)))
    err("incorrect access bits set");
  free(buf);
  printf("pgaccess_test: OK\n");
}

// ---------------------------------------------------------------------
// Phan 1: pgaccess voi nhieu page
// ---------------------------------------------------------------------

void
pgaccess_multi_test()
{
  char *buf;
  unsigned int abits, want;
  int i;

  printf("pgaccess_multi_test starting\n");
  testname = "pgaccess_multi_test";

  buf = alloc_pages(NPAGES);
  scan(buf, NPAGES);   // xoa moi bit A con sot lai

  // 1. Tung page mot: chi page i bi cham => chi bit i duoc bat.
  for(i = 0; i < NPAGES; i++){
    touch_write(buf + i * PGSIZE);
    abits = scan(buf, NPAGES);
    if(abits != (1U << i))
      err_mask("single page i should set only bit i", abits, 1U << i);
  }

  // 2. Cac page chan.
  for(i = 0; i < NPAGES; i += 2)
    touch_write(buf + i * PGSIZE);
  abits = scan(buf, NPAGES);
  if(abits != 0x55555555)
    err_mask("even pages", abits, 0x55555555);

  // 3. Cac page le.
  for(i = 1; i < NPAGES; i += 2)
    touch_write(buf + i * PGSIZE);
  abits = scan(buf, NPAGES);
  if(abits != 0xAAAAAAAA)
    err_mask("odd pages", abits, 0xAAAAAAAA);

  // 4. Tat ca 32 page.
  for(i = 0; i < NPAGES; i++)
    touch_write(buf + i * PGSIZE);
  abits = scan(buf, NPAGES);
  if(abits != 0xFFFFFFFF)
    err_mask("all pages", abits, 0xFFFFFFFF);

  // 5. Nhieu byte trong cung 1 page van chi la 1 bit.
  for(i = 0; i < PGSIZE; i += 512)
    touch_write(buf + 7 * PGSIZE + i);
  abits = scan(buf, NPAGES);
  if(abits != (1U << 7))
    err_mask("many bytes in one page", abits, 1U << 7);

  // 6. Doc (khong ghi) cung phai bat bit A.
  touch_read(buf + 3 * PGSIZE);
  touch_read(buf + 17 * PGSIZE);
  abits = scan(buf, NPAGES);
  want = (1U << 3) | (1U << 17);
  if(abits != want)
    err_mask("read-only access", abits, want);

  // 7. Quet mot vung con [4, 12): bit 0 cua mask ung voi page 4.
  //    Page 0 va 20 nam ngoai vung quet nen khong duoc bao cao,
  //    va bit A cua chung KHONG duoc bi xoa.
  touch_write(buf + 0 * PGSIZE);
  touch_write(buf + 4 * PGSIZE);
  touch_write(buf + 6 * PGSIZE);
  touch_write(buf + 11 * PGSIZE);
  touch_write(buf + 20 * PGSIZE);
  abits = scan(buf + 4 * PGSIZE, 8);
  want = (1U << 0) | (1U << 2) | (1U << 7);
  if(abits != want)
    err_mask("sub-range [4,12)", abits, want);
  abits = scan(buf, NPAGES);
  want = (1U << 0) | (1U << 20);
  if(abits != want)
    err_mask("pages outside scanned range must keep PTE_A", abits, want);

  // 8. Dia chi base khong can trang: lam tron xuong dau page.
  touch_write(buf + 5 * PGSIZE);
  abits = scan(buf + 5 * PGSIZE + 123, 2);
  if(abits != 1)
    err_mask("unaligned base", abits, 1);

  // 9. len nho hon 32: chi cac bit thap duoc dung, bit cao = 0.
  for(i = 0; i < NPAGES; i++)
    touch_write(buf + i * PGSIZE);
  abits = scan(buf, 3);
  if(abits != 0x7)
    err_mask("len=3", abits, 0x7);
  scan(buf, NPAGES);

  printf("pgaccess_multi_test: OK\n");
}

// ---------------------------------------------------------------------
// Phan 2: chua touch -> 0, da touch -> bit = 1, doc lan 2 -> 0
// ---------------------------------------------------------------------

void
pgaccess_touch_test()
{
  char *buf;
  unsigned int abits, want;

  printf("pgaccess_touch_test starting\n");
  testname = "pgaccess_touch_test";

  buf = alloc_pages(NPAGES);

  // a) Chua touch: page vua sbrk, chua goi pgaccess nao truoc do.
  abits = scan(buf, NPAGES);
  if(abits != 0)
    err_mask("fresh pages (never touched) must report 0", abits, 0);

  // Goi them lan nua khi van chua touch: van la 0.
  abits = scan(buf, NPAGES);
  if(abits != 0)
    err_mask("untouched pages, 2nd scan", abits, 0);

  // b) Da touch.
  touch_write(buf + 0 * PGSIZE);
  touch_read(buf + 9 * PGSIZE);
  touch_write(buf + 31 * PGSIZE);
  want = (1U << 0) | (1U << 9) | (1U << 31);
  abits = scan(buf, NPAGES);
  if(abits != want)
    err_mask("touched pages", abits, want);

  // c) Doc lan 2 khong touch gi them: kernel phai da xoa PTE_A => 0.
  abits = scan(buf, NPAGES);
  if(abits != 0)
    err_mask("2nd scan must be 0 (PTE_A not cleared?)", abits, 0);

  // d) Touch lai sau khi da xoa: bit phai bat lai duoc.
  touch_write(buf + 9 * PGSIZE);
  abits = scan(buf, NPAGES);
  if(abits != (1U << 9))
    err_mask("re-touch after clear", abits, 1U << 9);
  abits = scan(buf, NPAGES);
  if(abits != 0)
    err_mask("3rd scan must be 0", abits, 0);

  printf("pgaccess_touch_test: OK\n");
}

// ---------------------------------------------------------------------
// Tham so khong hop le / trang chua map
// ---------------------------------------------------------------------

void
pgaccess_args_test()
{
  char *buf;
  unsigned int abits;

  printf("pgaccess_args_test starting\n");
  testname = "pgaccess_args_test";

  buf = alloc_pages(NPAGES);

  if(pgaccess(buf, -1, &abits) >= 0)
    err("len < 0 should fail");
  if(pgaccess(buf, NPAGES + 1, &abits) >= 0)
    err("len > 32 should fail");
  if(pgaccess(buf, 1, (void *)0xffffffffffffffffL) >= 0)
    err("invalid mask address should fail");

  // len = 0: khong quet page nao, mask = 0.
  touch_write(buf);
  abits = scan(buf, 0);
  if(abits != 0)
    err_mask("len=0", abits, 0);
  scan(buf, NPAGES);

  // Cac page da bi tra lai bang sbrk(-n) khong con map: khong duoc crash,
  // va khong duoc bao cao la da truy cap.
  touch_write(buf + 30 * PGSIZE);
  touch_write(buf + 31 * PGSIZE);
  if(sbrk(-2 * PGSIZE) == (char *)-1)
    err("sbrk(-2 pages) failed");
  abits = scan(buf + 30 * PGSIZE, 2);
  if(abits != 0)
    err_mask("unmapped pages must report 0", abits, 0);

  printf("pgaccess_args_test: OK\n");
}

// ---------------------------------------------------------------------
// Phan 3: sysinfo truoc / sau sbrk
// ---------------------------------------------------------------------

void
getinfo(struct sysinfo *info)
{
  if(sysinfo(info) < 0)
    err("sysinfo failed");
}

void
sysinfo_sbrk_test()
{
  struct sysinfo s0, s1, s2, s3, s4, s5;
  const int N = 10;
  const int BIG = 256;   // 1 MB
  uint64 used;
  char *p;
  int i;

  printf("sysinfo_sbrk_test starting\n");
  testname = "sysinfo_sbrk_test";

  getinfo(&s0);
  printf("sysinfo_sbrk_test: before sbrk: freemem=%l bytes (%l pages), nproc=%l\n",
         s0.freemem, s0.freemem / PGSIZE, s0.nproc);
  if(s0.freemem == 0)
    err("freemem should be > 0");
  if(s0.freemem % PGSIZE != 0)
    err("freemem should be a multiple of PGSIZE");

  // sbrk(0) khong cap phat gi.
  sbrk(0);
  getinfo(&s1);
  if(s1.freemem != s0.freemem)
    err("sbrk(0) changed freemem");

  // Cap phat N page: freemem giam it nhat N page. Co the giam them vai
  // page cho page-table cap thap, nhung khong qua 3 page.
  p = sbrk(N * PGSIZE);
  if(p == (char *)-1)
    err("sbrk(N pages) failed");
  getinfo(&s2);
  used = s0.freemem - s2.freemem;
  printf("sysinfo_sbrk_test: after sbrk(%d pages): freemem=%l bytes, used %l pages\n",
         N, s2.freemem, used / PGSIZE);
  if(s2.freemem >= s0.freemem)
    err("freemem did not decrease after sbrk");
  if(used < N * PGSIZE)
    err("freemem decreased less than N pages");
  if(used > (N + 3) * PGSIZE)
    err("freemem decreased much more than N pages");
  if(s2.nproc != s0.nproc)
    err("nproc changed after sbrk");

  // xv6 cap phat ngay (khong lazy): ghi vao vung moi khong lam doi freemem.
  for(i = 0; i < N; i++)
    p[i * PGSIZE] = i;
  getinfo(&s3);
  if(s3.freemem != s2.freemem)
    err("touching sbrk'd memory changed freemem");

  // Tra lai N page: freemem tang dung N page (page-table khong bi giai phong).
  if(sbrk(-N * PGSIZE) == (char *)-1)
    err("sbrk(-N pages) failed");
  getinfo(&s4);
  printf("sysinfo_sbrk_test: after sbrk(-%d pages): freemem=%l bytes\n", N, s4.freemem);
  if(s4.freemem - s3.freemem != N * PGSIZE)
    err("freemem did not grow by exactly N pages after sbrk(-N)");

  // Cap phat lon (1 MB) roi tra lai.
  p = sbrk(BIG * PGSIZE);
  if(p == (char *)-1)
    err("sbrk(1MB) failed");
  getinfo(&s5);
  printf("sysinfo_sbrk_test: after sbrk(1MB): freemem=%l bytes\n", s5.freemem);
  if(s4.freemem - s5.freemem < BIG * PGSIZE)
    err("freemem did not decrease by >= 1MB");
  if(sbrk(-BIG * PGSIZE) == (char *)-1)
    err("sbrk(-1MB) failed");
  getinfo(&s1);
  if(s1.freemem - s5.freemem != BIG * PGSIZE)
    err("freemem did not grow by exactly 1MB after sbrk(-1MB)");

  printf("sysinfo_sbrk_test: OK\n");
}

// ---------------------------------------------------------------------

struct test {
  void (*fn)();
  char *name;
  int deflt;     // chay mac dinh khi khong co tham so
} tests[] = {
  { ugetpid_test,        "ugetpid",  0 },
  { pgaccess_test,       "pgaccess", 1 },
  { pgaccess_multi_test, "multi",    1 },
  { pgaccess_touch_test, "touch",    1 },
  { pgaccess_args_test,  "args",     1 },
  { sysinfo_sbrk_test,   "sysinfo",  1 },
  { 0, 0, 0 },
};

// Chay test trong tien trinh con. Tra ve 1 neu pass.
int
run(struct test *t)
{
  int pid, status;

  pid = fork();
  if(pid < 0){
    printf("pgtbltest: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    t->fn();
    exit(0);
  }
  wait(&status);
  if(status != 0){
    printf("pgtbltest: %s: FAILED\n", t->name);
    return 0;
  }
  return 1;
}

int
main(int argc, char *argv[])
{
  struct test *t;
  int i, ran = 0, failed = 0;

  for(t = tests; t->fn; t++){
    int want = t->deflt;
    if(argc > 1){
      want = 0;
      for(i = 1; i < argc; i++)
        if(strcmp(argv[i], t->name) == 0 || strcmp(argv[i], "all") == 0)
          want = 1;
    }
    if(!want)
      continue;
    ran++;
    if(!run(t))
      failed++;
  }

  if(ran == 0){
    printf("usage: pgtbltest [all | ugetpid pgaccess multi touch args sysinfo]\n");
    exit(1);
  }
  if(failed){
    printf("pgtbltest: %d of %d tests FAILED\n", failed, ran);
    exit(1);
  }
  printf("pgtbltest: all tests succeeded\n");
  exit(0);
}
