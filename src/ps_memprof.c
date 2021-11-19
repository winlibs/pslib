/*
 *    (c) Copyright 2004  Uwe Steinmann.
 *    All rights reserved.
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the
 *    Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *    Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps_intern.h"
#include "libps/pslib-mp.h"
#include "ps_error.h"

#define MAXMEM 15000

struct mem {
	void *ptr;
	int size;
	char *caller;
};

static struct mem memlist[MAXMEM];
static int peakmem = 0;
static int summem = 0;

PSLIB_API void PSLIB_CALL
PS_mp_init() {
	memset(memlist, 0, MAXMEM*sizeof(struct mem));
}

PSLIB_API void * PSLIB_CALL
PS_mp_malloc(PSDoc *p, size_t size, const char *caller) {
	void *a;
	int i;
	a = (void *) malloc(size);
	if(NULL == a)
		return NULL;
	i = 0;
	/* Find a free slot in the list of memory blocks */
	while((i < MAXMEM) && (memlist[i].ptr != NULL)) {
		i++;
	}
	if(i >= MAXMEM) {
		fprintf(stderr, _("Aiii, no more space for new memory block. Enlarge MAXMEM in %s."), __FILE__);
		fprintf(stderr, "\n");
	}
	memlist[i].ptr = a;
	memlist[i].size = size;
	summem += size;
	peakmem = (summem > peakmem) ? summem : peakmem;
	memlist[i].caller = strdup(caller);
	return(a);
}

PSLIB_API void * PSLIB_CALL
PS_mp_realloc(PSDoc *p, void *mem, size_t size, const char *caller) {
	void *a;
	int i;
	a = realloc(mem, size);
	if(NULL == a)
		return NULL;
	i = 0;
	while((i < MAXMEM) && (memlist[i].ptr != mem)) {
		i++;
	}
	if(i >= MAXMEM) {
		fprintf(stderr, _("Aiii, did not find memory block at 0x%X to enlarge: %s"), (unsigned int) mem, caller);
		fprintf(stderr, "\n");
	}
	memlist[i].ptr = a;
	summem -= memlist[i].size;
	summem += size;
	memlist[i].size = size;
	free(memlist[i].caller);
	memlist[i].caller = strdup(caller);

	return(a);
}

PSLIB_API void PSLIB_CALL
PS_mp_free(PSDoc *p, void *mem) {
	int i;
	if(NULL == mem) {
		fprintf(stderr, _("Aiii, you cannot free a NULL pointer."));
		fprintf(stderr, "\n");
		return;
	}
	i = 0;
	while((i < MAXMEM) && (memlist[i].ptr != mem)) {
		i++;
	}
	if(i >= MAXMEM) {
		fprintf(stderr, _("Aiii, did not find memory block at 0x%X to free."), (unsigned int) mem);
		fprintf(stderr, "\n");
	} else {
		memlist[i].ptr = NULL;
		summem -= memlist[i].size;
		memlist[i].size = 0;
		free(memlist[i].caller);
	}
	free(mem);
}

PSLIB_API void PSLIB_CALL
PS_mp_list_unfreed() {
	int i, j, l;
	const char *ptr;
	i = j = 0;
	while(i < MAXMEM) {
		if(memlist[i].ptr) {
			fprintf(stderr, _("%d. Memory at address 0x%X (%d) not freed: '%s'."), j, (unsigned int) memlist[i].ptr, memlist[i].size, memlist[i].caller);
			ptr = memlist[i].ptr;
			for(l=0; l<memlist[i].size; l++, ptr++) {
				fputc(*ptr, stderr);
			}
			fprintf(stderr, "\n");
			j++;
		}
		i++;
	}
	fprintf(stderr, _("Remaining unfreed memory: %d Bytes."), summem);
	fprintf(stderr, "\n");
	fprintf(stderr, _("Max. amount of memory used: %d Bytes."), peakmem);
	fprintf(stderr, "\n");
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
