#ifndef __CONDITION_H__
#define __CONDITION_H__

#include <type_traits>

#if defined(_WIN32)
#include <synchapi.h>
#endif

class Condition {

#ifdef _WIN32

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

    CRITICAL_SECTION l;
    CONDITION_VARIABLE c;

public:
    Condition() {
        InitializeCriticalSection(&l);
        InitializeConditionVariable(&c);
    }

    ~Condition() {
    }

    void Lock() {
        EnterCriticalSection(&l);
    }

    void Unlock() {
        LeaveCriticalSection(&l);
    }

    void Wait() {
        SleepConditionVariableCS(&c, &l, INFINITE);
    }

    void Signal() {
        WakeAllConditionVariable(&c);
    }

#else

    pthread_mutex_t l;
    pthread_cond_t c;

public:
    Condition() {
        pthread_mutex_init(&l, nullptr);
        pthread_cond_init(&c, nullptr);
    }

    ~Condition() {
        pthread_mutex_destroy(&l);
        pthread_cond_destroy(&c);
    }

    void Lock() {
        pthread_mutex_lock(&l);
    }

    void Unlock() {
        pthread_mutex_unlock(&l);
    }

    void Wait() {
        pthread_cond_wait(&c, &l);
    }

    void Signal() {
        pthread_cond_signal(&c);
    }

#endif
};

/* a simple condition variable class for storing pointers, resets value on
 * read, and blocks when value is null or until aborted. */

template <class T>
class ConditionVariable : Condition {

    static_assert(std::is_pointer_v<T>, "must be a pointer type");

private:
    T value = nullptr;
    bool done = false;

public:

    void Abort() {
        Lock();
        done = true;
        Unlock();
        Signal();
    }

    operator T() {
        T r;
        Lock();
        for (;;) {
            r = value;
            if (r || done) {
                break;
            }
            Wait();
        }
        value = nullptr;
        Unlock();
        return r;
    }

    ConditionVariable &operator=(const T &new_value) {
        Lock();
        delete value;
        value = new_value;
        Unlock();
        Signal();
        return *this;
    }
};

#endif /* __CONDITION_H__ */
