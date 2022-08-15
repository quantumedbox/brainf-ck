/*
  Simple brainfuck C99 interpreter implementation with x86_64 jit in mmap'ed
  executable memory

  https://esolangs.org/wiki/Brainfuck#Conventions
  https://www.muppetlabs.com/~breadbox/bf/standards.html
  http://brainfuck.org/brainfuck.html
  Behaviour:
    - Input cannot be longer than 10000 bytes
    - Data tape limit is 30000 cells
    - Cells are wrapping, meaning that decreasing 0 will result in 255, and
  increasing 255 will result in 0
    - Data pointer isnt wrapping, meaning that < at 0 will result in UB, and >
  at 29999 too
    - [ and ] should always have matching target, otherwise behavior is
  undefined
  * Note that some UBs are runtime catchable when compiled without NDEBUG macro
*/

/* todo: no op at EOF for , from stdin */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "extern/mman.h"
#else
#include <sys/mman.h>
#endif

#ifndef NDEBUG
#define PREVENT(cond)                                                          \
  do                                                                           \
    if (cond)                                                                  \
      return -1;                                                               \
  while (0)
#else
#define PREVENT(cond) (void)(cond)
#endif

#define INPUT_LIMIT (10000U)
#define MAX_BYTES_PER_INSTRUCTION (9U)
#define CODE_SIZE (INPUT_LIMIT * MAX_BYTES_PER_INSTRUCTION + 64)
uint64_t putchar_reg = (uint64_t)&putchar;
uint64_t getchar_reg = (uint64_t)&getchar;

#define OUT_BYTES(...)                                                         \
  do {                                                                         \
    unsigned char arr__[] = {__VA_ARGS__};                                     \
    memcpy(code_ptr, arr__, sizeof(arr__));                                    \
    code_ptr += sizeof(arr__);                                                 \
  } while (0)

int main(int argc, char **argv) {
  unsigned char *code = mmap(NULL, CODE_SIZE, PROT_EXEC | PROT_WRITE,
                             MAP_ANONYMOUS, -1, (off_t)0);
  unsigned char *code_ptr = code;
  unsigned char *pending_fwds_off[10000];
  unsigned char **pending_fwds_off_ptr = pending_fwds_off;
  void *prev_fwd_jmp_to = NULL;
  unsigned char data[30000];
  FILE *src_in = argc > 1 ? fopen(argv[1], "rb") : NULL;

  PREVENT(code == MAP_FAILED);
  PREVENT(src_in == NULL);

  /* Stack pointer should be 16 byte aligned (note that `call` will push rip on
   * the stack) */
  /* push rbp */
  OUT_BYTES(0x55);
  /* mov rbp, rsp */
  OUT_BYTES(0x48, 0x89, 0xe5);
  /* push rbx */
  OUT_BYTES(0x53);
  /* push r12 */
  OUT_BYTES(0x41, 0x54);
  /* push r13 */
  OUT_BYTES(0x41, 0x55);
  /* sub rsp, 24 */
  /* todo: Investigate why on Microsoft x64 ABI functions overrite caller's stack if we dont over allocate stack */
  OUT_BYTES(0x48, 0x83, 0xec, 0x18);
  /* mov rbx, rdi */
  OUT_BYTES(0x48, 0x89, 0xfb);
  /* mov r12, rsi */
  OUT_BYTES(0x49, 0x89, 0xf4);
  /* mov r13, rdx */
  OUT_BYTES(0x49, 0x89, 0xd5);

  while (!feof(src_in)) {
    PREVENT(code_ptr + MAX_BYTES_PER_INSTRUCTION >= code + CODE_SIZE);
    switch (fgetc(src_in)) {
    case '>':
      /* inc rbx */
      OUT_BYTES(0x48, 0xff, 0xc3);
      break;
    case '<':
      /* dec rbx */
      OUT_BYTES(0x48, 0xff, 0xcb);
      break;
    case '+':
      /* incb [rbx] */
      OUT_BYTES(0xfe, 0x03);
      break;
    case '-':
      /* decb [rbx] */
      OUT_BYTES(0xfe, 0x0b);
      break;
    case '.':
    #ifdef _WIN32
      /* todo: Better way to detect used ABI */
      /* mov ecx, [rbx] */
      OUT_BYTES(0x8b, 0x0b);
    #else
      /* mov edi, [rbx] */
      OUT_BYTES(0x8b, 0x3b);
    #endif
      /* call [r12] */
      OUT_BYTES(0x41, 0xff, 0x14, 0x24);
      break;
    case ',':
      /* call [r13] */
      OUT_BYTES(0x41, 0xff, 0x55, 0x00);
      /* mov [rbx], al */
      OUT_BYTES(0x88, 0x03);
      break;
    case '[': {
      /* cmpb [rbx], 0 */
      OUT_BYTES(0x80, 0x3b, 0x00);
      /* je OFFSET_TO_TARGET */
      OUT_BYTES(0x0f, 0x84);
      *pending_fwds_off_ptr++ = code_ptr;
      code_ptr += sizeof(uint32_t);
      prev_fwd_jmp_to = code_ptr;
      break;
    }
    case ']': {
      uint32_t offset;
      while (pending_fwds_off_ptr != pending_fwds_off) {
        --pending_fwds_off_ptr;
        offset = (ptrdiff_t)code_ptr - (ptrdiff_t)*pending_fwds_off_ptr + 5;
        memcpy(*pending_fwds_off_ptr, &offset, sizeof(uint32_t));
      }
      /* cmpb [rbx], 0 */
      OUT_BYTES(0x80, 0x3b, 0x00);
      /* jne OFFSET_TO_TARGET */
      OUT_BYTES(0x0f, 0x85);
      PREVENT(prev_fwd_jmp_to == NULL);
      offset = (ptrdiff_t)prev_fwd_jmp_to - (ptrdiff_t)code_ptr - 4;
      memcpy(code_ptr, &offset, sizeof(uint32_t));
      code_ptr += 4;
      break;
    }
    }
  }
  fclose(src_in);
  PREVENT(pending_fwds_off_ptr != pending_fwds_off);

  /* add rsp, 24 */
  OUT_BYTES(0x48, 0x83, 0xc4, 0x18);
  /* pop r13 */
  OUT_BYTES(0x41, 0x5d);
  /* pop r12 */
  OUT_BYTES(0x41, 0x5c);
  /* pop rbx */
  OUT_BYTES(0x5b);
  /* pop rbp */
  OUT_BYTES(0x5d);
  /* ret */
  OUT_BYTES(0xc3);

  /* todo: Ensure that System V x64 calling convention is used across compilers
   */
  ((void(__attribute__((sysv_abi)) *)(uint64_t, uint64_t, uint64_t))code)(
      (uint64_t)&data, (uint64_t)&putchar_reg, (uint64_t)&getchar_reg);

  PREVENT(munmap(code, CODE_SIZE) == 0);

  return 0;
}
