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
    uint64_t rflags;
    asm volatile("pushfq; pop %0" : "=r"(rflags) : : "memory");
    cli();

    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE))
    {
        while (lock->locked)
        {
            asm volatile("pause");
        }
    }

    return rflags;
}

static inline void spinlock_release_irqrestore(Spinlock *lock, uint64_t rflags)
{
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
    asm volatile("push %0; popfq" : : "r"(rflags) : "memory", "cc");
}

#endif // SYS_SPINLOCK_H_INCLUDED
