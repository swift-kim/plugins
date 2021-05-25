#ifndef _STATICLIBRARY1_H_
#define _STATICLIBRARY1_H_

/**
 * This header file is included to define _EXPORT_.
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// This method is exported from staticlibrary1.a
extern bool tizenstaticlibrary1(void);

#ifdef __cplusplus
}
#endif
#endif // _STATICLIBRARY1_H_

