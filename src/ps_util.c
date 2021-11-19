/*
 *    (c) Copyright 2001  Uwe Steinmann.
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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "ps_intern.h"
#include "ps_memory.h"
#include "ps_fontenc.h"
#include "ps_inputenc.h"
#include "ps_error.h"

/* ps_write() {{{
 * Output data using the current writeprocedure
 */
void
ps_write(PSDoc *p, const void *data, size_t size) {
	(void) p->writeproc(p, (void *) data, size);
}
/* }}} */

/* ps_putc() {{{
 * Output char
 */
void
ps_putc(PSDoc *p, char c)
{
	ps_write(p, (void *) &c, (size_t) 1);
}
/* }}} */

/* ps_puts() {{{
 * Output string
 */
void
ps_puts(PSDoc *p, const char *s)
{
	ps_write(p, (void *) s, strlen(s));
}
/* }}} */

/* ps_printf() {{{
 * Output formated string
 */
void
ps_printf(PSDoc *p, const char *fmt, ...)
{
	char buf[LINEBUFLEN];  /* formatting buffer */
	va_list ap;

	va_start(ap, fmt);

	setlocale(LC_NUMERIC, "C");
	(void) vsnprintf(buf, LINEBUFLEN, fmt, ap);
	setlocale(LC_NUMERIC, "");
	ps_puts(p, buf);

	va_end(ap);
}
/* }}} */

/* ps_ght_malloc() {{{
 * 
 * malloc routine which is passed to the hash table functions.
 */
void* ps_ght_malloc(size_t size, void *data) {
	PSDoc *psdoc = data;
	return(psdoc->malloc(psdoc, size, _("Allocate memory for hash table of char metrics")));
}
/* }}} */

/* ps_ght_free() {{{
 * 
 * free routine which is passed to the hash table functions.
 */
void ps_ght_free(void *ptr, void *data) {
	PSDoc *psdoc = data;
	psdoc->free(psdoc, ptr);
}
/* }}} */

/* ps_enter_scope() {{{
 */
void ps_enter_scope(PSDoc *psdoc, int scope) {
	if(psdoc->scopecount == (MAX_SCOPES-1)) {
		ps_error(psdoc, PS_RuntimeError, _("Maximum number of scopes reached."));
	} else {
		psdoc->scopecount++;
		psdoc->scopes[psdoc->scopecount] = scope;
	}
}
/* }}} */

/* ps_leave_scope() {{{
 */
void ps_leave_scope(PSDoc *psdoc, int scope) {
	if(scope != psdoc->scopes[psdoc->scopecount])
		ps_error(psdoc, PS_RuntimeError, _("Trying to leave scope %d but you are in %d."), scope, psdoc->scopes[psdoc->scopecount]);
	else
		psdoc->scopecount--;
}
/* }}} */

/* ps_current_scope() {{{
 */
int ps_current_scope(PSDoc *psdoc) {
	return(psdoc->scopes[psdoc->scopecount]);
}
/* }}} */

/* ps_check_scope() {{{
 */
ps_bool ps_check_scope(PSDoc *psdoc, int scope) {
	return(scope & psdoc->scopes[psdoc->scopecount]);
}
/* }}} */

/* ps_get_resources() {{{
 * Returns an array of pointers to all resources of a category.
 */
PS_RESOURCE **ps_get_resources(PSDoc *psdoc, const char *category, int *count) {
	PS_CATEGORY *cat;
	PS_RESOURCE *res;
	PS_RESOURCE **ress = NULL;
	int i;

	*count = 0;
	for(cat = dlst_first(psdoc->categories); cat != NULL; cat = dlst_next(cat)) {
		if (!strcmp(cat->name, category)) {
			ress = psdoc->malloc(psdoc, cat->resources->count * sizeof(PS_RESOURCE *), _("Allocate Memory for list of resources."));
			*count = cat->resources->count;
			for(i=0, res = dlst_first(cat->resources); res != NULL; res = dlst_next(res), i++) {
				ress[i] = res;
			}
		}
	}
	return(ress);
}
/* }}} */

/* ps_find_resource() {{{
 */
char *ps_find_resource(PSDoc *psdoc, const char *category, const char *resource) {
	PS_CATEGORY *cat;
	PS_RESOURCE *res;

	for(cat = dlst_first(psdoc->categories); cat != NULL; cat = dlst_next(cat)) {
		if (!strcmp(cat->name, category)) {
			for(res = dlst_first(cat->resources); res != NULL; res = dlst_next(res)) {
				if (!strcmp(res->name, resource))
					return(res->value);
			}
		}
	}
	return(NULL);
}
/* }}} */

/* ps_add_resource() {{{
 */
PS_RESOURCE * ps_add_resource(PSDoc *psdoc, const char *category, const char *resource, const char *filename, const char *prefix) {
	PS_CATEGORY *cat;
	PS_RESOURCE *res;

	/* Right and LeftMarginKerning are set in PS_set_paramter. They manipulate
	 * the ADOBEINFO struct of the affected char directly.
	 */
	if (strcmp("SearchPath", category) &&
	    strcmp("FontAFM", category) &&
			strcmp("FontEncoding", category) &&
			strcmp("FontProtusion", category) &&
			strcmp("FontOutline", category)) {
		return(NULL);
	}

	/* Search for the category */
	for(cat = dlst_first(psdoc->categories); cat != NULL; cat = dlst_next(cat)) {
		if(!strcmp(cat->name, category))
			break;
	}

	/* Create a new category if it does not exist */
	if(cat == NULL) {
		if(NULL == (cat = (PS_CATEGORY *) dlst_newnode(psdoc->categories, sizeof(PS_CATEGORY)))) {
			return(NULL);
		}
		cat->name = ps_strdup(psdoc, category);
		cat->resources = dlst_init(psdoc->malloc, psdoc->realloc, psdoc->free);
		dlst_insertafter(psdoc->categories, cat, PS_DLST_HEAD(psdoc->categories));
	}

	/* Check if resource already available */
	if(resource) {
		for(res = dlst_first(cat->resources); res != NULL; res = dlst_next(res)) {
			if(!strcmp(res->name, resource))
				break;
		}
	} else {
		res = NULL;
	}
	if(res) {
		psdoc->free(psdoc, res->name);
		res->name = ps_strdup(psdoc, resource);
		psdoc->free(psdoc, res->value);
		res->value = ps_strdup(psdoc, filename);
	} else {
		if(NULL == (res = (PS_RESOURCE *) dlst_newnode(cat->resources, sizeof(PS_RESOURCE)))) {
			return(NULL);
		}
		if(resource)
			res->name = ps_strdup(psdoc, resource);
		else
			res->name = NULL;
		res->value = ps_strdup(psdoc, filename);
		dlst_insertafter(cat->resources, res, PS_DLST_HEAD(cat->resources));
	}

	return(res);
}
/* }}} */

/* ps_del_resources() {{{
 */
void ps_del_resources(PSDoc *psdoc) {
	PS_CATEGORY *cat, *nextcat;
	PS_RESOURCE *res, *nextres;
	if(NULL == psdoc->categories)
		return;
	cat = dlst_first(psdoc->categories);
	while(cat != NULL) {
		nextcat = dlst_next(cat);
		psdoc->free(psdoc, cat->name);
		res = dlst_first(cat->resources);
		while(res != NULL) {
			nextres = dlst_next(res);
			if(res->name)
				psdoc->free(psdoc, res->name);
			if(res->value)
				psdoc->free(psdoc, res->value);
			res = nextres;
		}
		dlst_kill(cat->resources, dlst_freenode);
		cat = nextcat;
	}
	dlst_kill(psdoc->categories, dlst_freenode);
	psdoc->categories = NULL;
}
/* }}} */

/* ps_del_parameters() {{{
 */
void ps_del_parameters(PSDoc *psdoc) {
	PS_PARAMETER *param, *nextparam;
	if(NULL == psdoc->parameters)
		return;
	param = dlst_first(psdoc->parameters);
	while(param != NULL) {
		nextparam = dlst_next(param);
		psdoc->free(psdoc, param->name);
		psdoc->free(psdoc, param->value);
		param = nextparam;
	}
	dlst_kill(psdoc->parameters, dlst_freenode);
	psdoc->parameters = NULL;
}
/* }}} */

/* ps_del_values() {{{
 */
void ps_del_values(PSDoc *psdoc) {
	PS_VALUE *param, *nextparam;
	if(NULL == psdoc->values)
		return;
	param = dlst_first(psdoc->values);
	while(param != NULL) {
		nextparam = dlst_next(param);
		psdoc->free(psdoc, param->name);
		param = nextparam;
	}
	dlst_kill(psdoc->values, dlst_freenode);
	psdoc->values = NULL;
}
/* }}} */

/* ps_del_bookmarks() {{{
 */
void ps_del_bookmarks(PSDoc *psdoc, DLIST *bookmarks) {
	PS_BOOKMARK *bm, *nextbm;
	if(NULL == bookmarks)
		return;
	bm = dlst_first(bookmarks);
	while(bm != NULL) {
		nextbm = dlst_next(bm);
		psdoc->free(psdoc, bm->text);
		ps_del_bookmarks(psdoc, bm->children);
		bm = nextbm;
	}
	dlst_kill(bookmarks, dlst_freenode);
}
/* }}} */

/* ps_add_word_spacing() {{{
 */
void ps_add_word_spacing(PSDoc *psdoc, PSFont *psfont, float value) {
	psfont->wordspace += value * 1000.0 / psfont->size;
}
/* }}} */

/* ps_set_word_spacing() {{{
 */
void ps_set_word_spacing(PSDoc *psdoc, PSFont *psfont, float value) {
	ADOBEINFO *ai;

	if(value != 0.0) {
		psfont->wordspace = value * 1000.0 / psfont->size;
	} else {
		ai = gfindadobe(psfont->metrics->gadobechars, "space");
		if(ai)
			psfont->wordspace = ai->width;
		else
			psfont->wordspace = 500.0;
	}
}
/* }}} */

/* ps_get_word_spacing() {{{
 */
float ps_get_word_spacing(PSDoc *psdoc, PSFont *psfont) {
	float ws;
	ws = ((float) psfont->wordspace) * psfont->size / 1000.0;
	return(ws);
}
/* }}} */

/* ps_check_for_lig() {{{
 * Checks if the characters in text form a ligature with the character
 * in ai. If that is the case the number of characters consumed in text
 * will be returned in offset and the name of the ligature will be returned
 * in newadobename.
 * Ligatures can be broken off by a ligdischar. In such a case 1 will be
 * returned
 * and offset is set to 1 in order to skip the ligdischar. newadobename is set
 * to the name of the passed character.
 */
int ps_check_for_lig(PSDoc *psdoc, ADOBEFONTMETRIC *metrics, ADOBEINFO *ai, const char *text, char ligdischar, char **newadobename, int *offset) {
	ADOBEINFO *nextai;
	LIG *ligs;
	if((NULL == ai) || (NULL == ai->ligs))
		return 0;
	if(NULL == text || text[0] == '\0')
		return 0;

	ligs = ai->ligs;
	/* ligdischar breaks ligatures. The idea is, that every char followed
	 * by a ligdischar
	 * forms a ligature which is the char itself. Setting offset to
	 * 1 will skip the ligdischar */
	if(ligs && text[0] == ligdischar) {
		*offset += 1;
		*newadobename = ai->adobename;
		return 1;
	}
	if(ligs) {
		int _offset = 0;
		char *nextadobename;
		nextai = gfindadobe(metrics->gadobechars, psdoc->inputenc->vec[(unsigned char) (text[0])]);
		if(NULL == nextai)
			return 0;

		/* check if nextai and the following char form a ligature
		 * If that is the case then there is a change that we have a 3 char
		 * ligature. First check if current char and ligature of 2nd and 3th
		 * char for a ligature.
		 */
		if(ps_check_for_lig(psdoc, metrics, nextai, &text[1], ligdischar, &nextadobename, &_offset)) {
			while(ligs) {
				if(0 == strcmp(ligs->succ, nextadobename)) {
					*offset += 1+_offset;
					*newadobename = ligs->sub;
					return 1;
				}
				ligs = ligs->next;
			}
		}

		/* If we haven't found a three char ligature at this point then check
		 * for a 2 char ligatur.
		 */
		ligs = ai->ligs;
		while(ligs) {
	//		printf("Ligature for %s is %s\n", ai->adobename, ligs->succ);
			if(!strcmp(ligs->succ, nextai->adobename)) {
				ADOBEINFO *ligai;
				*offset += 1;
				/* check if the ligature made of the first two chars form a ligature
				 * which the third char
				 */
				ligai = gfindadobe(metrics->gadobechars, ligs->sub);
				if(ligai) {
					if(ps_check_for_lig(psdoc, metrics, ligai, &text[1], ligdischar, &nextadobename, offset)) {
						*newadobename = nextadobename;
						return 1;

					}
				}
				*newadobename = ligs->sub;
	//			printf("Found %s followed by %s replaced with %s\n", ai->adobename, nextai->adobename, ligs->sub);
				return 1;
			}
			ligs = ligs->next;
		}
	}
	return 0;
}
/* }}} */

/* ps_build_enc_hash() {{{
 * Builds the hash table of an encoding vector.
 * The glyhname is the the key, the position in the vector is the data.
 * The position of the glyph in the vector starts at 1 because 0 is reserved
 * by the hash table to indicate the end of the list.
 * If a glyphname exists more than once, only the first one will be
 * stored in the hash table. This would be the case for "" as glyphname,
 * but empty strings are not stored at all.
 */
ght_hash_table_t *ps_build_enc_hash(PSDoc *psdoc, ENCODING *enc) {
	int i;
	ght_hash_table_t *hashvec;
	hashvec = ght_create(512);
	if(hashvec) {
		ght_set_alloc(hashvec, ps_ght_malloc, ps_ght_free, psdoc);
		for(i=0; i<256; i++) {
			if(strlen(enc->vec[i]) > 0)
				ght_insert(hashvec, (char *) i+1, strlen(enc->vec[i])+1, enc->vec[i]);
		}
	}
	return(hashvec);
}
/* }}} */

/* ps_build_enc_from_font() {{{
 * Builds the hash table with a font encoding from the font itself.
 * This will only include those glyphs which have a number > 0.
 */
ght_hash_table_t *ps_build_enc_from_font(PSDoc *psdoc, ADOBEFONTMETRIC *metrics) {
	ght_hash_table_t *hashvec;

	hashvec = ght_create(512);
	if(hashvec) {
		ght_iterator_t iterator;
		char *p_key;
		ADOBEINFO *p_e;

		ght_set_alloc(hashvec, ps_ght_malloc, ps_ght_free, psdoc);
		for(p_e = ght_first(metrics->gadobechars, &iterator, (void **) &p_key); p_e; p_e = ght_next(metrics->gadobechars, &iterator, (void **) &p_key)) {
			/* Do not add glyphs with a number <= 0 */
			if(p_e->adobenum > 0) {
				if(0 > ght_insert(hashvec, (char *) p_e->adobenum+1, strlen(p_e->adobename)+1, p_e->adobename))
				ps_error(psdoc, PS_Warning, _("Could not insert entry %d->%s into font encoding hash table."), p_e->adobenum, p_e->adobename);
			}
		}
	}
	return(hashvec);
}
/* }}} */

/* ps_build_enc_vector() {{{
 * Builds the encoding vector from the hash table
 */
ENCODING *ps_build_enc_vector(PSDoc *psdoc, ght_hash_table_t *hashvec) {
	if(hashvec) {
		ght_iterator_t iterator;
		char *glyphname;
		int i;
		ENCODING *enc;

		enc = (ENCODING *) psdoc->malloc(psdoc, sizeof(ENCODING), _("Allocate memory for new encoding vector.")) ;
		if(NULL == enc) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for encoding vector."));
			return NULL;
		}
		memset(enc, 0, sizeof(ENCODING));

		for(i = (int) ght_first(hashvec, &iterator, (void **) &glyphname); i; i = (int) ght_next(hashvec, &iterator, (void **) &glyphname)) {
//			printf("got %s = %d\n", glyphname, i-1);
			enc->vec[i-1] = ps_strdup(psdoc, glyphname);
		}

		return(enc);
	} else {
		return NULL;
	}
}
/* }}} */

/* ps_list_fontenc() {{{
 * 
 */
void ps_list_fontenc(PSDoc *psdoc, ght_hash_table_t *hashvec) {
	if(hashvec) {
		ght_iterator_t iterator;
		char *glyphname;
		int i;

		fprintf(stderr, "---- Font encoding vector -----\n");
		fprintf(stderr, "Has %d entries.\n", ght_size(hashvec));
		for(i = (int) ght_first(hashvec, &iterator, (void **) &glyphname); i; i = (int) ght_next(hashvec, &iterator, (void **) &glyphname)) {
			fprintf(stderr, "%s = %d\n", glyphname, i-1);
		}
	}
}
/* }}} */

/* ps_free_enc_vector() {{{
 * Frees all memory allocated for an encoding vector including the vector
 * itself.
 */
void ps_free_enc_vector(PSDoc *psdoc, ENCODING *enc) {
	int i;
	if(!enc)
		return;
	if(enc->name)
		psdoc->free(psdoc, enc->name);
	for(i=0; i<256; i++) {
		if(enc->vec[i])
			psdoc->free(psdoc, enc->vec[i]);
	}
	psdoc->free(psdoc, enc);
}
/* }}} */

/* ps_fontenc_code() {{{
 * Returns the index in the fontencoding vector
 */
unsigned char ps_fontenc_code(PSDoc *psdoc, ght_hash_table_t *fontenc, char *adobename) {
	if(fontenc) {
		int code;
		code = (int) ght_get(fontenc, strlen(adobename)+1, (void *) adobename);
		if(code)
			return((unsigned char) code -1);
		else {
			ps_error(psdoc, PS_Warning, _("The font encoding vector does not contain the glyph '%s'. Using '?' instead."), adobename);
			return '?';
		}
	} else {
		return '?';
	}
}
/* }}} */

/* ps_fontenc_has_glpyh() {{{
 * checks of a glyph is in the fontencoding vector
 */
int ps_fontenc_has_glyph(PSDoc *psdoc, ght_hash_table_t *fontenc, char *adobename) {
	if(fontenc) {
		int code;
		code = (int) ght_get(fontenc, strlen(adobename)+1, (void *) adobename);
		if(code)
			return ps_true;
		else {
			return ps_false;
		}
	} else {
		return ps_false;
	}
}
/* }}} */

/* ps_inputenc_name() {{{
 * Returns the adobe name for a char in the input encoding vector
 */
char *ps_inputenc_name(PSDoc *psdoc, unsigned char c) {
	if(psdoc->inputenc) {
		return(psdoc->inputenc->vec[c]);
	} else {
		return NULL;
	}
}
/* }}} */

/* ps_get_bool_parameter() {{{
 */
int ps_get_bool_parameter(PSDoc *psdoc, const char *name, int deflt) {
	const char *onoffstr;
	onoffstr = PS_get_parameter(psdoc, name, 0);
	if(NULL == onoffstr) {
		return(deflt);
	} else {
		if(!strcmp(onoffstr, "true"))
			return(1);
		else
			return(0);
	}
}
/* }}} */

/* ps_open_file_in_path() {{{
 */
FILE *ps_open_file_in_path(PSDoc *psdoc, const char *filename) {
	FILE *fp;
	PS_RESOURCE **pathres;
	int count;
	int i;
	char buffer[255];

	if(fp = fopen(filename, "rb"))
		return fp;

	if(NULL != (pathres = ps_get_resources(psdoc, "SearchPath", &count))) {
		for(i=count-1; i>=0; i--) {
#ifdef HAVE_SNPRINTF
			snprintf(buffer, 255, "%s/%s", pathres[i]->value, filename);
#else
			sprintf(buffer, "%s/%s", pathres[i]->value, filename);
#endif
//			fprintf(stderr, "Searching for %s\n", buffer);
			if(fp = fopen(buffer, "rb")) {
//				fprintf(stderr, "found %s in %s\n", filename, pathres[i]->value);
				break;
			}
		}
		psdoc->free(psdoc, pathres);
		if(fp)
			return(fp);
	}

#ifdef PACKAGE_DATA_DIR
#ifdef HAVE_SNPRINTF
	snprintf(buffer, 255, "%s/%s", PACKAGE_DATA_DIR, filename);
#else
	sprintf(buffer, "%s/%s", PACKAGE_DATA_DIR, filename);
#endif
	if(fp = fopen(buffer, "rb"))
		return fp;
#endif

	return NULL;
}
/* }}} */

int pow85[5] = {1, 85, 85*85, 85*85*85, 85*85*85*85};
	
/* ps_ascii85_encode() {{{
 */
void ps_ascii85_encode(PSDoc *psdoc, char *data, size_t len) {
	unsigned long buffer;
	unsigned char c;
	int i, count, cc;

	buffer = 0;
	count = 0;
	cc = 0;
	while(count <= len-4) {
//		buffer = ((long) data[count] << 24) + ((long) data[count+1] << 16) + ((long) data[count+2] << 8) + (long) data[count+3];
		buffer |= ((unsigned char) data[count] << 24);
		buffer |= ((unsigned char) data[count+1] << 16);
		buffer |= ((unsigned char) data[count+2] << 8);
		buffer |= ((unsigned char) data[count+3]);

		if(buffer == 0) {
			ps_putc(psdoc, 'z');
			cc++;
		} else {
			for(i=4; i>=0; i--) {
				c = (unsigned int) buffer / pow85[i];
				ps_putc(psdoc, c+'!');
				buffer = (unsigned int) buffer % pow85[i];
			}
			cc += 4;
		}
		count += 4;
		if(cc > 55) {
			ps_putc(psdoc, '\n');
			cc = 0;
		}
	}
	if(len-count) {
		buffer = 0;
		for(i=0; i<len-count; i++)
			buffer = (buffer << 8) + (long) data[count+i];
		buffer = buffer << ((4-len+count)*8);
		for(i=4; i>=4-len+count; i--) {
			c = (unsigned int) buffer / pow85[i];
			ps_putc(psdoc, c+'!');
			buffer = (unsigned int) buffer % pow85[i];
		}
	}
	ps_putc(psdoc, '~');
	ps_putc(psdoc, '>');
}
/* }}} */

/* ps_asciihex_encode() {{{
 */
void ps_asciihex_encode(PSDoc *psdoc, char *data, size_t len) {
	int i, cc=0;
	unsigned char *dataptr;

	dataptr = data;
	for(i=0; i<len; i++) {
		ps_printf(psdoc, "%02x", *dataptr);
		dataptr ++;
		cc++;
		if(cc > 35 && i < (len-1)) {
			ps_printf(psdoc, "\n");
			cc = 0;
		}
	}
	ps_putc(psdoc, '\n');
	ps_putc(psdoc, '>');
}
/* }}} */

/* ps_parse_optlist() {{{
 */
ght_hash_table_t *ps_parse_optlist(PSDoc *psdoc, const char *optstr) {
	int i, isname;
	char name[100], value[100], delim;
	const char *optstrptr;
	ght_hash_table_t *opthash;

	if(optstr == NULL || optstr[0] == '\0')
		return(NULL);

	opthash = ght_create(30);
	if(opthash) {
		ght_set_alloc(opthash, ps_ght_malloc, ps_ght_free, psdoc);

		isname = 1;
		optstrptr = optstr;
		name[0] = '\0';
		value[0] = '\0';

		/* Skip leading spaces */
		while(*optstrptr == ' ')
			optstrptr++;

		while(*optstrptr != '\0') {
			if(isname) {
				i = 0;
				while(*optstrptr != '\0' && *optstrptr != ' ') {
					name[i] = *optstrptr;
					i++;
					optstrptr++;
				}
				name[i] = '\0';
				isname = 0;
				optstrptr++;
			} else {
				if(*optstrptr == '{') {
					delim = '}';
					optstrptr++;
				} else {
					delim = ' ';
				}
				i = 0;
				while(*optstrptr != '\0' && *optstrptr != delim) {
					value[i] = *optstrptr;
					i++;
					optstrptr++;
				}
				value[i] = '\0';
				isname = 1;
				optstrptr++;

				/* Value and Name is read, insert it into hash */
				if(name[0]) {
					ght_insert(opthash, (char *) ps_strdup(psdoc, value), strlen(name)+1, name);
					name[0] = '\0';
					value[0] = '\0';
				}
			}
			while(*optstrptr != '\0' && *optstrptr == ' ')
				optstrptr++;
		}
	}
	return(opthash);
}
/* }}} */

/* ps_free_optlist() {{{
 */
void ps_free_optlist(PSDoc *psdoc, ght_hash_table_t *opthash) {
	ght_iterator_t iterator;
	char *strval, *key;

//		fprintf(stderr, "Freeing optlist\n");
	for(strval = ght_first(opthash, &iterator, (void **) &key); strval; strval = ght_next(opthash, &iterator, (void **) &key)) {
//		fprintf(stderr, "Freeing %s\n", strval);
		psdoc->free(psdoc, strval);
	}
	ght_finalize(opthash);
}
/* }}} */

/* get_optlist_element_as_float() {{{
 * Retrieves an element from the hash and converts its value into float
 * Modifies *value only if the conversion was successful.
 * Returns -1 if the parameter is not available and -2 if it is available
 * but the value cannot be retrieved.
 */
int get_optlist_element_as_float(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, float *value) {
	float tmp;
	char *strval;
	char *endval;

	if(NULL == opthash) {
		return(-1);
	}
	if(NULL == (strval = ght_get(opthash, strlen(name)+1, (void *) name))) {
		return(-1);
	}
	tmp = (float) strtod(strval, &endval);
	if(endval == strval) {
		return(-2);
	}
	*value = tmp;
	return(0);
} 
/* }}} */

/* get_optlist_element_as_int() {{{
 * Retrieves an element from the hash and converts its value into integer
 * Modifies *value only if the conversion was successful.
 * Returns -1 if the parameter is not available and -2 if it is available
 * but the value cannot be retrieved.
 */
int get_optlist_element_as_int(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, int *value) {
	long tmp;
	char *strval;
	char *endval;

	if(NULL == opthash) {
		return(-1);
	}
	if(NULL == (strval = ght_get(opthash, strlen(name)+1, (void *) name))) {
		return(-1);
	}
	tmp = strtol(strval, &endval, 10);
	if(endval == strval) {
		return(-2);
	}
	*value = tmp;
	return(0);
} 
/* }}} */

/* get_optlist_element_as_bool() {{{
 * Retrieves an element from the hash and converts its value into boolean
 * Returns -1 if the parameter is not available and -2 if it is available
 * but the value cannot be retrieved.
 */
int get_optlist_element_as_bool(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, ps_bool *value) {
	char *strval;

	if(NULL == opthash) {
		return(-1);
	}
	if(NULL == (strval = ght_get(opthash, strlen(name)+1, (void *) name))) {
		return(-1);
	}
	if(strcmp(strval, "false") == 0) {
		*value = ps_false;
	} else if(strcmp(strval, "true") == 0) {
		*value = ps_true;
	} else {
		return(-2);
	}
	return(0);
} 
/* }}} */

/* get_optlist_element_as_string() {{{
 * Retrieves an element from the hash and converts its value into string
 * Returns -1 if the parameter is not available
 */
int get_optlist_element_as_string(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, char **value) {
	char *strval;

	if(NULL == opthash) {
		return(-1);
	}
	if(NULL == (strval = ght_get(opthash, strlen(name)+1, (void *) name))) {
		return(-1);
	}

	*value = strval;

	return(0);
} 
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=2 ts=2 fdm=marker
 * vim<600: sw=2 ts=2
 */
