#ifndef __LOCK_H__
#define __LOCK_H__

#ifdef _WIN32
#include <synchapi.h>
#else
#include <pthread.h>
#endif

class Lock {

#ifdef _WIN32

    CRITICAL_SECTION cs;

public:
    Lock() {
        InitializeCriticalSection(&cs);
    }
    ~Lock() {
        DeleteCriticalSection(&cs);
    }

    void Acquire() {
        EnterCriticalSection(&cs);
    }

    void Release() {
        LeaveCriticalSection(&cs);
    }

#else

    pthread_mutex_t l;

public:
    Lock() {
        pthread_mutex_init(&l, nullptr);
    }

    ~Lock() {
        pthread_mutex_destroy(&l);
    }

    void Acquire() {
        pthread_mutex_lock(&l);
    }

    void Release() {
        pthread_mutex_unlock(&l);
    }

#endif
};

class ScopedLock {

    Lock &lock;

public:
    ScopedLock(Lock &lock)
        : lock(lock) {
        lock.Acquire();
    }

    ~ScopedLock() {
        lock.Release();
    }
};

#endif /* __LOCK_H__ */
