#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

size_t strlen(const char*);
int isspace(int c);
int isdigit(int c);
int atoi(const char *s);
void *memset(void *dest, int c, size_t n);
int strcmp(const char *l, const char *r);

#endif // __STRING_H__

