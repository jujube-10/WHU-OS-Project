/*
 * lab1: S 态入口。引导核（hart0）初始化串口并输出 Banner，从核等待引导核完成后再各自启动。
 *
 * 环境注意事项（说明书 §2"环境前置条件"与附录 C）:
 *  1. start() 的 M→S 切换清单必须包含 PMP 配置(最简两行):
 *       w_pmpaddr0(0x3fffffffffffffull); w_pmpcfg0(0xf);
 *     否则在新版 QEMU 上 mret 进 S 态的第一条取指即触发 fault(全程无输出)。
 *  2. entry.S 里的陷阱向量标号前加 .balign 4(mtvec 要求 4 字节对齐,
 *     不满足时写入会被硬件静默丢弃)。
 *     —— 本设计不写 mtvec，故本条不适用；理由见 doc/lab1.md §2.4。
 *
 * 多核结构对齐 xv6-riscv/kernel/main.c:
 *   if (cpuid() == 0) { 全量初始化; __atomic_store_n(&started, 1, RELEASE); }
 *   else             { while (__atomic_load_n(&started, ACQUIRE) == 0) ; 各自的 per-hart 初始化 }
 * lab1 没有 per-hart 初始化、也没有调度器，从核等到标志后即停机，输出只由引导核产生。
 */
#include "types.h"
#include "arch/riscv/riscv.h"
#include "course_sid.h"

void console_init(void);
int printf(const char *fmt, ...);

/* 引导核完成全局初始化后置 1；release/acquire 建立"初始化 → 从核继续"的顺序 */
volatile static int started = 0;

/* start() 通过 mret 把每个 hart 的 S 态 mepc 都设到这里 */
void
main(void)
{
  // todo： cpuid封装
  if (r_tp() == 0) {
    /* 引导核：全局初始化 + 输出。
       （xv6 在这个分支里还要建页表、进程表、中断控制器……lab1 只需要串口） */
    console_init();

    /* Banner：OSLAB1 sid=<学号> mod97=0x<学号%97 的十六进制>
       **必须用 %ld / %lx**：xv6 的 printf 里 %d 是 `va_arg(ap, int)`（32 位），
       而 COURSE_SID = 2024302111102 超出 int 范围，用 %d 会被截成 1372514686。
       十六进制走 printint（小写、无前导零），不能用 %p（那是定长 16 位带前导零）。
       行尾 '\n' 也经过 console_putc，故协议 1 下它的后面同样带 '.'。 */
    printf("OSLAB1 sid=%ld mod97=0x%lx\n", COURSE_SID, COURSE_SID % 97);

    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
  } else {
    /* 从核：自旋等引导核把全局状态（串口）初始化完，再各自启动。
       lab1 没有 per-hart 初始化，所以等待之后无事可做。 */
    while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
      ;
  }

  /* lab1 到此为止：没有中断、没有进程，停机等待（wfi 只是 hint，故循环） */
  for (;;)
    asm volatile("wfi");
}
