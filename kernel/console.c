/*
 * lab1: NS16550 串口轮询输出 + 控制台输出协议层。
 *
 * 环境注意事项（说明书 §2"环境前置条件"与附录 C）:
 *  1. start() 的 M→S 切换清单必须包含 PMP 配置(最简两行):
 *       w_pmpaddr0(0x3fffffffffffffull); w_pmpcfg0(0xf);
 *     否则在新版 QEMU 上 mret 进 S 态的第一条取指即触发 fault(全程无输出)。
 *  2. entry.S 里的陷阱向量标号前加 .balign 4(mtvec 要求 4 字节对齐,
 *     不满足时写入会被硬件静默丢弃)。
 *     —— 本设计不写 mtvec，故本条不适用；理由见 doc/lab1.md §2.4。
 *
 * 层次划分（协议与硬件解耦）:
 *   console_putc()  协议层：按 LAB1_BANNER_PROTOCOL 决定每字节的输出格式
 *      └─ emit()    节流层：每输出 THROTTLE_PERIOD_BYTES 字节注入一次 nop 空转
 *           └─ uart_putc()  硬件层：轮询 LSR.THRE 后写 THR
 */
#include "types.h"
#include "arch/riscv/riscv.h"
#include "arch/riscv/memlayout.h"
#include "course_sid.h"

/* ---- 16550 寄存器（QEMU virt: UART0 = 0x10000000，寄存器步长 1 字节） ---- */
#define Reg(reg)         ((volatile unsigned char *)(UART0 + (reg)))
#define ReadReg(reg)     (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

#define THR 0   /* 写：发送保持寄存器 */
#define IER 1   /* 中断使能寄存器 */
#define FCR 2   /* 写：FIFO 控制寄存器 */
#define LCR 3   /* 线路控制寄存器（bit7 = DLAB） */
#define LSR 5   /* 线路状态寄存器 */

#define LCR_EIGHT_BITS  (3 << 0)  /* 8 位数据、无校验、1 停止位，并清除 DLAB */
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR  (3 << 1)
#define LSR_TX_IDLE     (1 << 5)  /* THRE：THR 已空，可接收下一个字符 */

/* ---- 个性化参数 ---- */
#define THROTTLE_PERIOD_BYTES (16 + COURSE_SID % 16)  /* 输出满 30 字节后空转 */
#define THROTTLE_NOP_ROUNDS   1000     // 这里自己设为1000

void console_putc(int c);

/* 硬件层：轮询 LSR.THRE=1 后写 THR。THR 只有 1 字节深，
   未就绪时强写会丢字符/覆盖未发出的字节，且不报错。 */
static void
uart_putc(int c)
{
  while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);
}

/* 节流层：空转 nop。asm volatile 保证 -O 下循环不被优化掉。 */
static void
throttle(void)
{
  for (int i = 0; i < THROTTLE_NOP_ROUNDS; i++)
    asm volatile("nop");
}

/* 最低层输出点：真实写出 1 字节，并按节流周期注入空转 */
static unsigned long out_bytes;

static void
emit(int c)
{
  uart_putc(c);
  out_bytes++;
  if (out_bytes % THROTTLE_PERIOD_BYTES == 0)
    throttle();
}

/* 协议 */
void
console_putc(int c)
{
  emit(c);
#if LAB1_BANNER_PROTOCOL == 1
  /* 协议 1：每个字节（含最后一个字节）后紧跟一个 '.' */
  emit('.');
#elif LAB1_BANNER_PROTOCOL == 0
  /* 协议 0：原始明文，无附加字符（行尾由调用者输出 '\n'） */
#else
#error "LAB1_BANNER_PROTOCOL 目前只实现 0/1"
#endif
}

/* 初始化：只做"轮询输出"所需的最小配置 */
void
console_init(void)
{
  WriteReg(IER, 0x00);                            /* 关全部中断：确定走轮询路径 */
  WriteReg(LCR, LCR_EIGHT_BITS);                  /* 8N1（QEMU 不模拟波特率，故不设分频） */
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);/* 清空并使能 FIFO */
}
