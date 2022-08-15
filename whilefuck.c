/*
  Simple hermetic C89 brainfuck interpreter implementation with while loop dispatch

  https://esolangs.org/wiki/Brainfuck#Conventions
  https://www.muppetlabs.com/~breadbox/bf/standards.html
  http://brainfuck.org/brainfuck.html
  Behaviour:
    - Input cannot be longer than 10000 bytes
    - Data tape limit is 30000 cells
    - Cells are wrapping, meaning that decreasing 0 will result in 255, and increasing 255 will result in 0
    - Data pointer isnt wrapping, meaning that < at 0 will result in UB, and > at 29999 too
    - [ and ] should always have matching target, otherwise behavior is undefined
    - `,` at EOF is no-op
  * Note that some UBs are runtime catchable when compiled without NDEBUG macro
*/

/* todo: Make it usable as a library */

#include "bfdef.h"
#include <stdio.h>

int main(int argc, char** argv) {
  unsigned char src[BF_SRC_LEN];
  unsigned char cell[BF_CELL_LEN];
  unsigned char *src_ptr = src, *cmd_ptr = src, *cell_ptr = cell;
  FILE *src_in = argc > 1 ? fopen(argv[1], "r") : NULL;
  PREVENT(src_in == NULL);
  src_ptr += fread(src, sizeof(unsigned char), sizeof(src), src_in);
  PREVENT(!feof(src_in));
  fclose(src_in);
  while (cmd_ptr < src_ptr) {
    int l = 1;
    PREVENT(cell_ptr < cell || cell_ptr >= cell + sizeof cell);
    switch (*cmd_ptr++) {
    case '>': ++cell_ptr; break;
    case '<': --cell_ptr; break;
    case '+': ++*cell_ptr; break;
    case '-': --*cell_ptr; break;
    case '.': putchar(*cell_ptr); break;
    case ',': if (!feof(stdin)) *cell_ptr = getchar(); break;
    case '[': if (!*cell_ptr) do {
        PREVENT(cmd_ptr >= src_ptr);
        l -= (*cmd_ptr == ']') - (*cmd_ptr == '[');
      } while (++cmd_ptr, l);
      break;
    case ']': if (*cell_ptr) do {
        PREVENT(cmd_ptr - 1 <= src); 
        l += (*(cmd_ptr - 2) == ']') - (*(cmd_ptr - 2) == '[');
      } while (--cmd_ptr, l);
      break;
    }
  }
  return 0;
}
