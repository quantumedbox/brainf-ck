#include <assert.h>

#ifndef NDEBUG
#define PREVENT(cond) do if (cond) assert(0); while (0)
#else
#define PREVENT(cond) (void)(cond)
#endif

#ifndef BF_SRC_LEN
#define BF_SRC_LEN (10000)
#endif

#ifndef BF_CELL_LEN
#define BF_CELL_LEN (30000)
#endif
