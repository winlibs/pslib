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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "config.h"
#include "ps_intern.h"
#include "ps_error.h"

/* str_buffer_new() {{{
 * Create a new string buffer with the given initial size
 */
STRBUFFER *str_buffer_new(PSDoc *psdoc, size_t size) {
	STRBUFFER *sb;
	if(NULL == (sb = psdoc->malloc(psdoc, sizeof(STRBUFFER), _("Allocate memory for string buffer"))))
		return NULL;
	if(size > 0) {
		if(NULL == (sb->buffer = psdoc->malloc(psdoc, size, _("Allocate memory for string buffer")))) {
			psdoc->free(psdoc, sb);
			return NULL;
		}
		sb->buffer[0] = '\0';
	} else {
		sb->buffer = NULL;
	}
	sb->size = size;
	sb->cur = 0;
	return(sb);
}
/* }}} */

/* str_buffer_delete() {{{
 * Frees the memory occupied by the string buffer include the struct itself
 */
void str_buffer_delete(PSDoc *psdoc, STRBUFFER *sb) {
	if(sb->buffer)
		psdoc->free(psdoc, sb->buffer);
	psdoc->free(psdoc, sb);
}
/* }}} */

#define MSG_BUFSIZE 256
/* str_buffer_print() {{{
 * print a string at the end of a buffer
 */
size_t str_buffer_print(PSDoc *psdoc, STRBUFFER *sb, const char *fmt, ...) {
	char msg[MSG_BUFSIZE];
	va_list ap;
	int written;

	va_start(ap, fmt);
	written = vsnprintf(msg, MSG_BUFSIZE, fmt, ap);
	if(written >= MSG_BUFSIZE) {
		ps_error(psdoc, PS_IOError, _("Format string in string buffer is to short"));
		return -1;
	}

	/* Enlarge memory for buffer
	 * Enlarging it MSG_BUFSIZE ensure that the space is in any case
	 * sufficient to add the new string.
	 */
	if((sb->cur + written + 1) > sb->size) {
		sb->buffer = psdoc->realloc(psdoc, sb->buffer, sb->size+MSG_BUFSIZE, _("Get more memory for string buffer."));
		sb->size += MSG_BUFSIZE;
	}
	strcpy(&(sb->buffer[sb->cur]), msg);
	sb->cur += written;

	va_end(ap);
	return(written);
}
/* }}} */
#undef MSG_BUFSIZE

/* str_buffer_write() {{{
 * write a string at the end of a buffer
 */
size_t str_buffer_write(PSDoc *psdoc, STRBUFFER *sb, const char *data, size_t size) {
	/* Enlarge memory for buffer
	 * Enlarging it MSG_BUFSIZE ensure that the space is in any case
	 * sufficient to add the new string.
	 */
	if((sb->cur + size + 1) > sb->size) {
		sb->buffer = psdoc->realloc(psdoc, sb->buffer, sb->size+max(size, 2000), _("Get more memory for string buffer."));
		sb->size += max(size, 2000);
	}
	memcpy(&(sb->buffer[sb->cur]), data, size);
	sb->cur += size;
	sb->buffer[sb->cur+1] = '\0';
	return(size);
}
/* }}} */

/* str_buffer_get() {{{
 * Returns a pointer to current buffer
 */
const char *str_buffer_get(PSDoc *psdoc, STRBUFFER *sb) {
	return(sb->buffer);
}
/* }}} */

/* str_buffer_len() {{{
 * Returns len of string buffer
 */
size_t str_buffer_len(PSDoc *psdoc, STRBUFFER *sb) {
	return(sb->cur);
}
/* }}} */

/* str_buffer_clear() {{{
 * Clears the string buffer but will not free the memory
 */
void str_buffer_clear(PSDoc *psdoc, STRBUFFER *sb) {
	sb->cur = 0;
}
/* }}} */


