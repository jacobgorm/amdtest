#include <io.h>
#include <ws2tcpip.h>

#include "asprintf.h"

typedef long long int ssize_t;

#define close _close
#define fileno _fileno
#define read _read
#define strdup _strdup
#define sleep _sleep
