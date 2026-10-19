#include <stdlib.h>
#include "utils/helix_memory.h"
void* helix_malloc(int size){ return calloc(1, size); }
void helix_free(void *p){ free(p); }
