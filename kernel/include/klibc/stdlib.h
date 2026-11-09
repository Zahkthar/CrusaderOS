#ifndef KLIBC_STDLIB_H_INCLUDED
#define KLIBC_STDLIB_H_INCLUDED

// +-----------------------------+
// | [NUMERIC <=> STR] FUNCTIONS |
// +-----------------------------+

char *itoa(int value, char *buffer, int base);
int atoi(const char *str);

#endif // KLIBC_STDLIB_H_INCLUDED