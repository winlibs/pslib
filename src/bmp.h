
#include "ps_intern.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned char* read_bmp (PSDoc *psdoc, const char* file, int* w,
	int* h, int* bps, int* spp,
	int* xres, int* yres, unsigned char** color_table,
	int* color_table_size, int* color_table_elements);

#ifdef __cplusplus
}
#endif
