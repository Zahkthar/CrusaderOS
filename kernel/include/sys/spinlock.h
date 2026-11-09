#ifndef SYS_SPINLOCK_H_INCLUDED
#define SYS_SPINLOCK_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "sys/cpu.h"

// +------------+
// | STRUCTURES |
// +------------+

typedef struct Spinlock
{
    volatile uint32_t locked;
} Spinlock;

#define SPINLOCK_INIT ((Spinlock){.locked = 0})

// +-----------+
// | FUNCTIONS |
// +-----------+

static inline void spinlock_init(Spinlock *lock)
{
    lock->locked = 0;
}

static inline bool spinlock_try_acquire(Spinlock *lock)
{
    return __atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE) == 0;
}

static inline void spinlock_acquire(Spinlock *lock)
{
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE))
    {
        while (lock->locked)
        {
            asm volatile("pause");
        }
    }
}

static inline void spinlock_release(Spinlock *lock)
{
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

static inline uint64_t spinlock_acquire_irqsave(Spinlock *lock)
{
    uint64_t rflags = read_rflags();
    cli();
    spinlock_acquire(lock);
    return rflags;
}

static inline void spinlock_release_irqrestore(Spinlock *lock, uint64_t rflags)
{
    spinlock_release(lock);
    write_rflags(rflags);
}

#endif // SYS_SPINLOCK_H_INCLUDED
