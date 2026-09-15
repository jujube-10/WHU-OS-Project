/*
 * lab1: 格式化控制台输出（printf）。
 *
 *
 * 三个部件 :
 *   printint()  按 base 输出整数：低位先入 buf，再反序输出；sign 决定是否输出 '-'
 *   printptr()  输出 "0x" + 定长 16 位十六进制（即 %p 的格式）
 *   printf()    逐字节扫描格式串；"l"/"ll" 通过多看 1~2 个字符识别
 *
 * 支持的转换 :
 *   %d %ld %lld    有符号十进制
 *   %u %lu %llu    无符号十进制
 *   %x %lx %llx    十六进制（小写、**无前导零**）
 *   %p             指针（0x + **定长 16 位，有前导零**）
 *   %c 字符    %s 字符串（空指针 → "(null)"）    %% 百分号
 *   未知转换：原样输出 '%' 与后续字符，便于发现格式串写错
 *
 * 与 xv6 的差异，待完善:
 *   - xv6 的 printk 用 `pr.lock` 自旋锁串行化输出；lab1 还没有锁设施
 *     （spinlock.h 未提供），且当前只有引导核会输出，故暂不加锁。lab2 引入
 *     spinlock 后补上即可。
 *   - xv6 的 `panic()/panicking/panicked` 也在同一个文件里；lab1 没有 panic
 *     机制，未移植。
 *   - 输出统一走 console_putc()，所以协议 1 的 '.' 会作用到 printf 的每个字节。
 */
#include <stdarg.h>

#include "types.h"

void console_putc(int c);

static char digits[] = "0123456789abcdef";

/* 按 base 输出整数。buf[20] 的容量刚好放下 64 位负数的最长形式
   （-9223372036854775808，19 位数字 + 符号 = 20 字节）。 */
static void
printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    console_putc(buf[i]);
}

/* %p：0x 后固定输出 16 位十六进制（左移 4 位逐位取高 4 bit） */
static void
printptr(uint64 x)
{
  int i;

  console_putc('0');
  console_putc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    console_putc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

int
printf(const char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  va_start(ap, fmt);
  for (i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      console_putc(cx);
      continue;
    }
    i++;
    c0 = fmt[i + 0] & 0xff;
    c1 = c2 = 0;
    if (c0)
      c1 = fmt[i + 1] & 0xff;
    if (c1)
      c2 = fmt[i + 2] & 0xff;
    if (c0 == 'd') {
      printint(va_arg(ap, int), 10, 1);
    } else if (c0 == 'l' && c1 == 'd') {
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if (c0 == 'u') {
      printint(va_arg(ap, uint32), 10, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if (c0 == 'x') {
      printint(va_arg(ap, uint32), 16, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if (c0 == 'p') {
      printptr(va_arg(ap, uint64));
    } else if (c0 == 'c') {
      console_putc(va_arg(ap, uint));
    } else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; s++)
        console_putc(*s);
    } else if (c0 == '%') {
      console_putc('%');
    } else if (c0 == 0) {
      break; /* 格式串以孤立的 '%' 结尾 */
    } else {
      /* 打印未知的 % 序列，以引起注意 */
      console_putc('%');
      console_putc(c0);
    }
  }
  va_end(ap);

  return 0;
}
