/*
 *  M 态入口（复位后 QEMU 把每个 hart 都送到 0x80000000，即 _entry）。
 *
 */
#include "course_sid.h"

    .section .text
    .globl _entry
_entry:
    /* ① 关中断。复位时 mstatus.MIE 与 mie 本就为 0，故这条是"定序"而非"修复"：
       中断使能必须晚于"栈 + 处理程序"就绪，且显式清零不依赖复位值的约定。 */
    csrw  mie, zero
    li    t0, 8                 /* mstatus.MIE = bit 3 */
    csrc  mstatus, t0

    /* ② 每核独立栈：sp = stack0 + (hartid + 1) * (LAB1_STACK_KB * 1024)。
       必须"一核一份"——所有 hart 都会真的往下跑，共用一份栈会互相踩。
       步长是 8*1024=8192，超出 addi 的 12 位有符号立即数范围，故 li + mul + add。 */
    la    sp, stack0
    li    t0, LAB1_STACK_KB * 1024      /* 必须与 start.c 中 stack0 的步长一致 */
    csrr  t1, mhartid
    addi  t1, t1, 1
    mul   t0, t0, t1
    add   sp, sp, t0

    /* ③ 进入 M 态 C 代码（start.c），所有 hart 都走这里 */
    call  start

spin:                           /* start() 正常不返回；这里兜底 */
    j     spin
