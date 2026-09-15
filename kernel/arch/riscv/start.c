/*
 * lab1: M 态初始化 + M→S 特权级切换；并定义 entry.S 使用的初始内核栈。
 *
 * 环境注意事项（说明书 §2"环境前置条件"与附录 C）:
 *  1. start() 的 M→S 切换清单必须包含 PMP 配置(最简两行):
 *       w_pmpaddr0(0x3fffffffffffffull); w_pmpcfg0(0xf);
 *     否则在新版 QEMU 上 mret 进 S 态的第一条取指即触发 fault(全程无输出)。
 *  2. entry.S 里的陷阱向量标号前加 .balign 4(mtvec 要求 4 字节对齐,
 *     不满足时写入会被硬件静默丢弃)。
 *     —— 本设计不写 mtvec，故本条不适用；理由见 doc/lab1.md §2.4。
 */
#include "types.h"
#include "arch/riscv/riscv.h"
#include "course_sid.h"

void main(void);

/* 初始内核栈，一核一份（同 xv6 的 stack0[4096 * NCPU]）：
   每核 LAB1_STACK_KB = 4KB × (1 + 学号%3)，共 NCPU 份，16 字节对齐(psABI)。
   entry.S 用这个步长按 hartid 选址：sp = stack0 + (hartid+1) * (LAB1_STACK_KB*1024)。
   Bare 模式下它就是物理地址，位于内核映像的 .bss 中。 */
#define NCPU 8 /* 必须 ≥ 实际 hart 数（QEMU virt 默认 1、上限 512；取 8 与 xv6 一致） */
__attribute__((aligned(16))) char stack0[LAB1_STACK_KB * 1024 * NCPU];

/* entry.S 跳到这里，此时：M 态、无分页、栈已就绪 */
void
start(void)
{
  /* 1. 不启用分页：Bare 模式（M 态本身不受 satp 影响，这里为 S 态定调，同时显式化） */
  w_satp(0);

  /* 2. PMP：无固件（-bios none）时 PMP 无任何条目，
        规范规定"S/U 态无匹配条目即访问失败"，故必须显式放开全部物理内存。
        pmpcfg0=0xf = R|W|X|A(TOR)；pmpaddr0 在 TOR 模式存"上界>>2"，
        取全 1 的 54 位 ⇒ 覆盖 [0, 2^56)。缺这两行 ⇒ 进 S 态第一条取指就 fault。 */
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  /* 3. 委托：把所有异常/中断交给 S 态（此处只是"交给谁"，并不使能） */
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  /* 4. M 态不使能任何中断源；S 态的 sstatus.SIE 复位为 0，天然关闭 */
  w_mie(0);
  w_sie(0);

  /* 5. mstatus：MPP=S（mret 后进 S 态），保持 MIE=0 */
  uint64 x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  /* 6. mret 的返回地址 = S 态入口 */
  w_mepc((uint64)main);

  /* 7. 把 hartid 记到 tp，供 C 代码用 r_tp() 判断当前核 */
  w_tp(r_mhartid());

  /* 8. 定序，然后"人造一次 trap 返回"：mret 按 MPP 降级到 S 态并从 mepc 取指 */
  io_fence();
  asm volatile("mret");
}
