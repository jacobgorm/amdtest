#include <stdio.h>
#include <cstdlib>

#include "asprintf.h"

#define err(__x, fmt, ...) do { printf( fmt ": %s", ##__VA_ARGS__, strerror(errno)); exit(__x); } while (0);
#define errx(__x, fmt, ...) do { printf( fmt, ##__VA_ARGS__ ); exit(__x); } while (0);
#define warn(fmt, ...) do { printf( fmt ": %s", ##__VA_ARGS__, strerror(errno)); } while (0);
#define warnx(fmt, ...) do { printf( fmt, ##__VA_ARGS__ ); } while (0);
