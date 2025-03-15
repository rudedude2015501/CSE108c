#ifndef THREADPOOL_DEFS_H
#define THREADPOOL_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))
#define ROUND_DOWN(a, b) ((a) / (b) * (b))
#define ROUND_UP(a, b) (((a) + (b) - 1) / (b) * (b))
#define UNUSED __attribute__((unused))
// #define PACKED __attribute__((packed))

#ifdef __cplusplus
}
#endif

#endif /* distributed-sgx-sort/common/defs.h */
