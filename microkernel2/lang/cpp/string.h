#ifndef __CPP_STRING_H__
#define __CPP_STRING_H__

#include <types.h>

__EXTERN_C__

long int atol(const char *s);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t len);
result_t memcpy_s(void *dest, size_t dest_size, const void *src, size_t size);
int memcmp(const void *s1, const void *s2, size_t len);

__EXTERN_C_END__

#endif
