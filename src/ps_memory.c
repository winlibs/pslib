/*
 *    (c) Copyright 2001  Vilson Gärtner, Uwe Steinmann.
 *    (c) Copyright 2002-2004  Uwe Steinmann.
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

/* $Id: */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include "ps_intern.h"
#include "ps_error.h"

size_t ps_strlen(const char *str) {
	return(strlen(str));
}

char *ps_strdup(PSDoc *p, const char *str) {
	size_t len;
	char *buf;

	if (str == NULL) {
		ps_error(p, PS_Warning, "NULL string in ps_strdup");
		return(NULL);
	}
	len = ps_strlen(str);
	buf = (char *) p->malloc(p, len+1, "ps_strdup");
	if(buf != NULL) {
		memcpy(buf, str, len+1);
//		buf[len] = '\0';
	}
	return(buf);
}

/* Default memory management function {{{
 * These functions are the default functions for memory management
 * if PS_new() is called.
 */
void * _ps_malloc(PSDoc *p, size_t size, const char *caller) {
	return((void *) malloc(size));
}

void * _ps_realloc(PSDoc *p, void *mem, size_t size, const char *caller) {
	return((void *) realloc(mem, size));
}

void _ps_free(PSDoc *p, void *mem) {
	free(mem);
	mem = NULL;
}
/* }}} */

/* Convenience functions for memory management {{{
 * Call these functions instead of psdoc->malloc(), psdoc->free()
 * and psdoc->realloc()
 */
void * ps_calloc(PSDoc *p, size_t size, const char *caller) {
	void *ret;

	if(NULL == (ret = (void *) p->malloc(p, size, caller))) {
		return NULL;
	}
	memset(ret, 0, size);
	return ret;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
