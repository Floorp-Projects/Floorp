#include <pthread.h>
#include <stdlib.h>

pthread_mutex_t __attribute__((weak)) rust_libloading_dlerror_mutex = PTHREAD_MUTEX_INITIALIZER;

void __attribute__((weak))
rust_libloading_dlerror_mutex_lock(void)
{
    if (pthread_mutex_lock(&rust_libloading_dlerror_mutex) != 0) {
        abort();
    }
}

void __attribute__((weak))
rust_libloading_dlerror_mutex_unlock(void)
{
    if (pthread_mutex_unlock(&rust_libloading_dlerror_mutex) != 0) {
        abort();
    }
}
