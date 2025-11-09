#ifndef GLOBAL_H
#define GLOBAL_H

// Prefer pulling standard definitions first; fall back to manual define.
#ifndef INT64_MAX
#include <stdint.h>
#endif
#ifndef INT64_MAX
#include <limits.h>
#endif
#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807LL
#endif

#endif // GLOBAL_H
