#ifndef KLIBC_STRING_H_INCLUDED
#define KLIBC_STRING_H_INCLUDED

// +-----------+
// | FUNCTIONS |
// +-----------+

void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *ptr, int value, size_t count);

#endif // KLIBC_STRING_H_INCLUDED