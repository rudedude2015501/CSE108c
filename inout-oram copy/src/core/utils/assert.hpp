#pragma once
#include <cstdio>

#define err_abort(file, func, line, condition) ({ \
  printf("Assertion failed (%s): function %s, line %d, file %s.\n", condition, func, line, file);  \
  abort(); \
})

#define my_assert(condition) \
  ((condition) ? ((void)0) : err_abort(__FILE__, __func__, __LINE__, #condition))

