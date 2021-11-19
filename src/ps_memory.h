#ifndef __PX_MEMORY_H__
#define __PX_MEMORY_H__
size_t ps_strlen(const char *str);
char * ps_strdup(PSDoc *p, const char *str);
void * ps_calloc(PSDoc *p, size_t size, const char *caller);

void * _ps_malloc(PSDoc *p, size_t size, const char *caller);
void * _ps_realloc(PSDoc *p, void *mem, size_t size, const char *caller);
void _ps_free(PSDoc *p, void *mem);
#endif
