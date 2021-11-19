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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>
#endif
#include <ctype.h>

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#endif

#include "ps_intern.h"
#include "ps_memory.h"
#include "ps_error.h"
#include "ps_fontenc.h"
#include "ps_inputenc.h"

#ifdef HAVE_LIBPNG
#	include "png.h"
#endif

#ifdef HAVE_LIBJPEG
#	include "jpeglib.h"
#endif

#ifdef HAVE_LIBGIF
#	include "gif_lib.h"
#endif

#ifdef HAVE_LIBTIFF
#	include "tiffio.h"
#endif

#ifndef DISABLE_BMP
#	include "bmp.h"
#endif

/* ps_writeproc_file() {{{
 * Default output function
 */
static size_t
ps_writeproc_file(PSDoc *p, void *data, size_t size)
{
	return fwrite(data, 1, size, p->fp);
}
/* }}} */

/* ps_writeproc_buffer() {{{
 * Default output function for memory
 */
static size_t
ps_writeproc_buffer(PSDoc *p, void *data, size_t size)
{
	return str_buffer_write(p, p->sb, data, size);
}
/* }}} */

/* _ps_delete_font() {{{
 */
void _ps_delete_font(PSDoc *psdoc, PSFont *psfont) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(psfont == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("PSFont is null."));
		return;
	}
	if(psfont->psdoc != psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("This PSFont was created for a different document."));
		return;
	}

	/* Free hash table with char info */
	if(psfont->metrics->gadobechars) {
		ADOBEINFO *ai;
		char *p ;
		ght_iterator_t iterator;
		for(ai = ght_first(psfont->metrics->gadobechars, &iterator, (void **) &p); ai != NULL; ai = ght_next(psfont->metrics->gadobechars, &iterator, (void **) &p)) {
			LIG *lig, *ligtmp;
			KERN *k, *ktmp;
			PCC *np, *nptmp;
			lig = ai->ligs;
			while(lig != NULL) {
				if(lig->succ)
					psdoc->free(psdoc, lig->succ);
				if(lig->sub)
					psdoc->free(psdoc, lig->sub);
				ligtmp = lig->next;
				psdoc->free(psdoc, lig);
				lig = ligtmp;
			}
			/*
			for (lig=ai->ligs; lig; lig = ligtmp) {
				if(lig->succ)
					psdoc->free(psdoc, lig->succ);
				if(lig->sub)
					psdoc->free(psdoc, lig->sub);
				ligtmp = lig->next;
				psdoc->free(psdoc, lig);
			}
			*/
			k = ai->kerns;
			while(k != NULL) {
				if(k->succ)
					psdoc->free(psdoc, k->succ);
				ktmp = k->next;
				psdoc->free(psdoc, k);
				k = ktmp;
			}
			for (np=ai->pccs; np; np = nptmp) {
				if(np->partname)
					psdoc->free(psdoc, np->partname);
				nptmp = np->next;
				psdoc->free(psdoc, np);
			}
			if(ai->kern_equivs)
				psdoc->free(psdoc, ai->kern_equivs);
			psdoc->free(psdoc, ai->adobename);
			psdoc->free(psdoc, ai);
		}
		ght_finalize(psfont->metrics->gadobechars);
	}

	if(psfont->metrics->fontenc) {
		ght_finalize(psfont->metrics->fontenc);
	}

	if(psfont->metrics->fontname) {
		psdoc->free(psdoc, psfont->metrics->fontname);
	}

	if(psfont->metrics->codingscheme) {
		psdoc->free(psdoc, psfont->metrics->codingscheme);
	}

	if(psfont->metrics) {
		psdoc->free(psdoc, psfont->metrics);
	}

	if(psfont->encoding) {
		psdoc->free(psdoc, psfont->encoding);
	}

	/* FIXME: There is much more to free (e.g. the font metrics) than
	 * just the font structure.
	 */
	psdoc->free(psdoc, psfont);
}
/* }}} */

/* _ps_delete_image() {{{
 */
void _ps_delete_image(PSDoc *psdoc, PSImage *psimage) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(NULL == psimage) {
		ps_error(psdoc, PS_RuntimeError, _("PSImage is null."));
		return;
	}

	if(psimage->type)
		psdoc->free(psdoc, psimage->type);
	if(psimage->data)
		psdoc->free(psdoc, psimage->data);
	if(psimage->name)
		psdoc->free(psdoc, psimage->name);
	if(psimage->palette)
		psdoc->free(psdoc, psimage->palette);
	psdoc->free(psdoc, psimage);
}
/* }}} */

/* _ps_delete_pattern() {{{
 */
void _ps_delete_pattern(PSDoc *psdoc, PSPattern *pspattern) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(NULL == pspattern) {
		ps_error(psdoc, PS_RuntimeError, _("PSPattern is null."));
		return;
	}

	if(pspattern->name)
		psdoc->free(psdoc, pspattern->name);
	psdoc->free(psdoc, pspattern);
}
/* }}} */

/* _ps_delete_spotcolor() {{{
 */
void _ps_delete_spotcolor(PSDoc *psdoc, PSSpotColor *spotcolor) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(NULL == spotcolor) {
		ps_error(psdoc, PS_RuntimeError, _("PSSpotColor is null."));
		return;
	}

	if(spotcolor->name)
		psdoc->free(psdoc, spotcolor->name);
	psdoc->free(psdoc, spotcolor);
}
/* }}} */

/* _ps_delete_shading() {{{
 */
void _ps_delete_shading(PSDoc *psdoc, PSShading *psshading) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(NULL == psshading) {
		ps_error(psdoc, PS_RuntimeError, _("PSShading is null."));
		return;
	}

	if(psshading->name)
		psdoc->free(psdoc, psshading->name);
	psdoc->free(psdoc, psshading);
}
/* }}} */

/* _ps_delete_gstate() {{{
 */
void _ps_delete_gstate(PSDoc *psdoc, PSGState *gstate) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(NULL == gstate) {
		ps_error(psdoc, PS_RuntimeError, _("PSGState is null."));
		return;
	}

	if(gstate->name)
		psdoc->free(psdoc, gstate->name);
	psdoc->free(psdoc, gstate);
}
/* }}} */

/* _ps_register_font|image|pattern() {{{
 * Register font, image with PS document
 * Returns the font, image id which is the index within the
 * internal font, image
 * array + 1. Returns 0 in case of an error.
 */
#define PS_REGISTER_RESOURCE(resname, restype) \
static int \
_ps_register_##resname(PSDoc *p, restype *resname) \
{ \
	int i; \
 \
	i = 0; \
	while(i < p->resname##cnt && p->resname##s[i] != NULL) { \
		i++; \
	} \
	if(i >= p->resname##cnt) { \
		if(NULL == (p->resname##s = p->realloc(p, p->resname##s, (p->resname##cnt+5) * sizeof(restype *), _("Could not enlarge memory for internal resource array.")))) { \
			return 0; \
		} \
		memset((void *)&p->resname##s[p->resname##cnt], 0, 5*sizeof(restype *)); \
		p->resname##cnt += 5; \
	} \
	p->resname##s[i] = resname; \
	return(i+1); \
}

#define PS_UNREGISTER_RESOURCE(resname, restype) \
static void \
_ps_unregister_##resname(PSDoc *p, int id) \
{ \
	if(id > p->resname##cnt || id < 1) { \
		ps_error(p, PS_Warning, _("Trying to unregister a resource which does not exist.")); \
		return; \
	} \
	if(p->resname##s[id-1] != NULL) { \
		_ps_delete_##resname(p, p->resname##s[id-1]); \
		p->resname##s[id-1] = NULL; \
	} else { \
		ps_error(p, PS_Warning, _("Trying to unregister a resource which does not exist.")); \
	} \
}

#define PS_FIND_RESOURCE(resname, restype) \
static int \
_ps_find_##resname(PSDoc *p, restype *ptr) \
{ \
	int i; \
 \
	i = 0; \
	while(i < p->resname##cnt) { \
		if(p->resname##s[i] == ptr) \
			return(i+1); \
		i++; \
	} \
	return(0); \
}

#define PS_FIND_RESOURCE_BY_NAME(resname, restype) \
static int \
_ps_find_##resname##_by_name(PSDoc *p, const char *name) \
{ \
	int i; \
 \
	i = 0; \
	while(i < p->resname##cnt) { \
		if(p->resname##s[i] && (0 == strcmp(p->resname##s[i]->name, name))) \
			return(i+1); \
		i++; \
	} \
	return(0); \
}

#define PS_GET_RESOURCE(resname, restype) \
static restype * \
_ps_get_##resname(PSDoc *p, int id) \
{ \
	if(id > p->resname##cnt || id < 1) { \
		ps_error(p, PS_Warning, _("Trying to get a resource which does not exist.")); \
		return NULL; \
	} \
	return(p->resname##s[id-1]); \
}

PS_REGISTER_RESOURCE(font, PSFont)
PS_UNREGISTER_RESOURCE(font, PSFont)
PS_FIND_RESOURCE(font, PSFont)
PS_FIND_RESOURCE_BY_NAME(font, PSFont)
PS_GET_RESOURCE(font, PSFont)
PS_REGISTER_RESOURCE(image, PSImage)
PS_UNREGISTER_RESOURCE(image, PSImage)
PS_FIND_RESOURCE(image, PSImage)
PS_FIND_RESOURCE_BY_NAME(image, PSImage)
PS_GET_RESOURCE(image, PSImage)
PS_REGISTER_RESOURCE(pattern, PSPattern)
PS_UNREGISTER_RESOURCE(pattern, PSPattern)
PS_FIND_RESOURCE(pattern, PSPattern)
PS_FIND_RESOURCE_BY_NAME(pattern, PSPattern)
PS_GET_RESOURCE(pattern, PSPattern)
PS_REGISTER_RESOURCE(shading, PSShading)
PS_UNREGISTER_RESOURCE(shading, PSShading)
PS_FIND_RESOURCE(shading, PSShading)
PS_FIND_RESOURCE_BY_NAME(shading, PSShading)
PS_GET_RESOURCE(shading, PSShading)
PS_REGISTER_RESOURCE(spotcolor, PSSpotColor)
PS_UNREGISTER_RESOURCE(spotcolor, PSSpotColor)
PS_FIND_RESOURCE(spotcolor, PSSpotColor)
PS_FIND_RESOURCE_BY_NAME(spotcolor, PSSpotColor)
PS_GET_RESOURCE(spotcolor, PSSpotColor)
PS_REGISTER_RESOURCE(gstate, PSGState)
PS_UNREGISTER_RESOURCE(gstate, PSGState)
PS_FIND_RESOURCE(gstate, PSGState)
PS_FIND_RESOURCE_BY_NAME(gstate, PSGState)
PS_GET_RESOURCE(gstate, PSGState)
/* }}} */

/* ps_output_shading_dict() {{{
 * Outputs a shading dictionary.
 */
static void ps_output_shading_dict(PSDoc *psdoc, PSShading *psshading) {
	ps_printf(psdoc, "<<\n");
	ps_printf(psdoc, " /ShadingType %d\n", psshading->type);
	if(psshading->type == 3)
		ps_printf(psdoc, " /Coords [%.2f %.2f %.2f %.2f %.2f %.2f]\n", psshading->x0, psshading->y0, psshading->r0, psshading->x1, psshading->y1, psshading->r1);
	else
		ps_printf(psdoc, " /Coords [%.2f %.2f %.2f %.2f]\n", psshading->x0, psshading->y0, psshading->x1, psshading->y1);
	ps_printf(psdoc, " /Extend [ %s %s ]\n", psshading->extend0 ? "true" : "false", psshading->extend1 ? "true" : "false");
	ps_printf(psdoc, " /AntiAlias %s\n", psshading->antialias ? "true" : "false");
	switch(psshading->startcolor.colorspace) {
		case PS_COLORSPACE_GRAY:
			ps_printf(psdoc, " /ColorSpace /DeviceGray\n");
			ps_printf(psdoc, " /Function\n");
			ps_printf(psdoc, " <<\n");
			ps_printf(psdoc, "  /FunctionType 2 /Domain [ 0 1 ]\n");
			ps_printf(psdoc, "  /C0 [ %.4f ]\n", psshading->startcolor.c1);
			ps_printf(psdoc, "  /C1 [ %.4f ]\n", psshading->endcolor.c1);
			ps_printf(psdoc, "  /N %.4f\n", psshading->N);
			ps_printf(psdoc, " >>\n");
			break;
		case PS_COLORSPACE_RGB:
			ps_printf(psdoc, " /ColorSpace /DeviceRGB\n");
			ps_printf(psdoc, " /Function\n");
			ps_printf(psdoc, " <<\n");
			ps_printf(psdoc, "  /FunctionType 2 /Domain [ 0 1 ]\n");
			ps_printf(psdoc, "  /C0 [ %.4f %.4f %.4f ]\n", psshading->startcolor.c1, psshading->startcolor.c2, psshading->startcolor.c3);
			ps_printf(psdoc, "  /C1 [ %.4f %.4f %.4f ]\n", psshading->endcolor.c1, psshading->endcolor.c2, psshading->endcolor.c3);
			ps_printf(psdoc, "  /N %.4f\n", psshading->N);
			ps_printf(psdoc, " >>\n");
			break;
		case PS_COLORSPACE_CMYK:
			ps_printf(psdoc, " /ColorSpace /DeviceCMYK\n");
			ps_printf(psdoc, " /Function\n");
			ps_printf(psdoc, " <<\n");
			ps_printf(psdoc, "  /FunctionType 2 /Domain [ 0 1 ]\n");
			ps_printf(psdoc, "  /C0 [ %.4f %.4f %.4f %.4f ]\n", psshading->startcolor.c1, psshading->startcolor.c2, psshading->startcolor.c3, psshading->startcolor.c4);
			ps_printf(psdoc, "  /C1 [ %.4f %.4f %.4f %.4f ]\n", psshading->endcolor.c1, psshading->endcolor.c2, psshading->endcolor.c3, psshading->endcolor.c4);
			ps_printf(psdoc, "  /N %.4f\n", psshading->N);
			ps_printf(psdoc, " >>\n");
			break;
		case PS_COLORSPACE_SPOT: {
			PSSpotColor *spotcolor;
			spotcolor = _ps_get_spotcolor(psdoc, (int) psshading->startcolor.c1);
			if(!spotcolor) {
				ps_error(psdoc, PS_RuntimeError, _("Could not find spot color."));
				return;
			}
			ps_printf(psdoc, " /ColorSpace ");
			ps_printf(psdoc, "[ /Separation (%s)\n", spotcolor->name);
			switch(spotcolor->colorspace) {
				case PS_COLORSPACE_GRAY:
					ps_printf(psdoc, "  /DeviceGray { 1 %f sub mul 1 exch sub }\n", spotcolor->c1);
					break;
				case PS_COLORSPACE_RGB: {
					float max;
					max = (spotcolor->c1 > spotcolor->c2) ? spotcolor->c1 : spotcolor->c2;
					max = (max > spotcolor->c3) ? max : spotcolor->c3;
					ps_printf(psdoc, "  /DeviceRGB { 1 exch sub dup dup %f exch sub %f mul add exch dup dup %f exch sub %f mul add exch dup %f exch sub %f mul add }\n", max, spotcolor->c1, max, spotcolor->c2, max, spotcolor->c3);
					break;
				}
				case PS_COLORSPACE_CMYK:	
					ps_printf(psdoc, "  /DeviceCMYK { dup %f mul exch dup %f mul exch dup %f mul exch %f mul }\n", spotcolor->c1, spotcolor->c2, spotcolor->c3, spotcolor->c4);
					break;
			}
			ps_printf(psdoc, "   ]\n");
			ps_printf(psdoc, " /Function\n");
			ps_printf(psdoc, " <<\n");
			ps_printf(psdoc, "  /FunctionType 2 /Domain [ 0 1 ]\n");
			ps_printf(psdoc, "  /C0 [ %.4f ]\n", psshading->startcolor.c2);
			ps_printf(psdoc, "  /C1 [ %.4f ]\n", psshading->endcolor.c2);
			ps_printf(psdoc, "  /N %.4f\n", psshading->N);
			ps_printf(psdoc, " >>\n");
			break;
		}
	}
	ps_printf(psdoc, ">>\n");
}
/* }}} */

/* Start of API functions */

/* PS_get_majorversion() {{{
 * Get major version of library
 */
PSLIB_API int PSLIB_CALL
PS_get_majorversion(void) {
	return(LIBPS_MAJOR_VERSION);
}
/* }}} */

/* PS_get_minorversion() {{{
 * Get minor version of library
 */
PSLIB_API int PSLIB_CALL
PS_get_minorversion(void) {
	return(LIBPS_MINOR_VERSION);
}
/* }}} */

/* PS_get_subminorversion() {{{
 * Get minor version of library
 */
PSLIB_API int PSLIB_CALL
PS_get_subminorversion(void) {
	return(LIBPS_MICRO_VERSION);
}
/* }}} */

/* PS_boot() {{{
 * Will set text domain, once it works
 */
PSLIB_API void PSLIB_CALL
PS_boot(void) {
#ifdef ENABLE_NLS
	/* do not call textdomain in a library
	textdomain (GETTEXT_PACKAGE);
	*/
//	setlocale(LC_MESSAGES, "");
//	setlocale(LC_TIME, "");
//	setlocale(LC_CTYPE, "");
//	setlocale(LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#endif
}
/* }}} */

/* PS_shutdown() {{{
 * Counter part to PS_boot()
 */
PSLIB_API void PSLIB_CALL
PS_shutdown(void) {
}
/* }}} */

/* PS_set_info() {{{
 * Sets several document information will show up as comments in the Header
 * of the PostScript document
 */
PSLIB_API void PSLIB_CALL
PS_set_info(PSDoc *p, const char *key, const char *val) {
	char *val_buf;
	char *key_buf;

	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_OBJECT|PS_SCOPE_DOCUMENT)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'object', or 'document' scope."), __FUNCTION__);
		return;
	}

	if (key == NULL || key[0] == '\0' || val == NULL || val[0] == '\0') {
		ps_error(p, PS_Warning, _("Empty key or value in PS_set_info()."));
		return;
	}
	if(p->headerwritten == ps_true) {
		ps_error(p, PS_Warning, _("Calling PS_set_info() has no effect because PostScript header is already written."));
	}

	val_buf = ps_strdup(p, val);
	key_buf = ps_strdup(p, key);
	if (0 == strcmp(key_buf, "Keywords")) {
		p->Keywords = val_buf;
	} else if (0 == strcmp(key_buf, "Subject")) {
		p->Subject = val_buf;
	} else if (0 == strcmp(key_buf, "Title")) {
		p->Title = val_buf;
	} else if (0 == strcmp(key_buf, "Creator")) {
		p->Creator = val_buf;
	} else if (0 == strcmp(key_buf, "Author")) {
	  p->Author = val_buf;
	} else if (0 == strcmp(key_buf, "BoundingBox")) {
		if(p->BoundingBox)
			p->free(p, p->BoundingBox);
	  p->BoundingBox = val_buf;
	} else if (0 == strcmp(key_buf, "Orientation")) {
	  p->Orientation = val_buf;
	}
	p->free(p, key_buf);
}
/* }}} */

/* PS_new2() {{{
 * Create a new PostScript document. Allows to set memory management
 * functions and error handler.
 */
PSLIB_API PSDoc * PSLIB_CALL
PS_new2(void  (*errorhandler)(PSDoc *p, int type, const char *msg, void *data),
	void* (*allocproc)(PSDoc *p, size_t size, const char *caller),
	void* (*reallocproc)(PSDoc *p, void *mem, size_t size, const char *caller),
	void  (*freeproc)(PSDoc *p, void *mem),
	void   *opaque) {
	PSDoc *p;

	if(allocproc == NULL) {
		allocproc = _ps_malloc;
		reallocproc = _ps_realloc;
		freeproc  = _ps_free;
	}
	if (errorhandler == NULL)
		errorhandler = _ps_errorhandler;

	if(NULL == (p = (PSDoc *) (* allocproc) (NULL, sizeof(PSDoc), "PS new"))) {
		(*errorhandler)(NULL, PS_MemoryError, _("Could not allocate memory for new PS document."), opaque);
		return(NULL);
	}
	memset((void *)p, 0, (size_t) sizeof(PSDoc));

	p->errorhandler = errorhandler;
	p->user_data = opaque;
	p->malloc = allocproc;
	p->realloc = reallocproc;
	p->free = freeproc;
	p->fp = NULL;
	p->sb = NULL;
	p->copies = 1;
	p->warnings = ps_true;
	p->inputenc = ps_get_inputencoding("ISO-8859-1"); //&inputencoding;
	p->hdict = NULL;
	p->hdictfilename = NULL;
	p->categories = dlst_init(allocproc, reallocproc, freeproc);
	p->parameters = dlst_init(allocproc, reallocproc, freeproc);
	p->values = dlst_init(allocproc, reallocproc, freeproc);
	p->bookmarks = dlst_init(allocproc, reallocproc, freeproc);
	p->lastbookmarkid = 0;
	p->bookmarkdict = NULL;
	p->bookmarkcnt = 0;
	p->CreationDate = NULL;

	/* Initialize list of fonts */
	p->fontcnt = 5;
	if(NULL == (p->fonts = p->malloc(p, p->fontcnt*sizeof(PSFont *), _("Allocate memory for internal Font list of document.")))) {
		return(NULL);
	}
	memset((void *)p->fonts, 0, p->fontcnt*sizeof(PSFont *));

	/* Initialize list of images */
	p->imagecnt = 5;
	if(NULL == (p->images = p->malloc(p, p->imagecnt*sizeof(PSImage *), _("Allocate memory for internal Image list of document.")))) {
		return(NULL);
	}
	memset((void *)p->images, 0, p->imagecnt*sizeof(PSImage *));

	/* Initialize list of patterns */
	p->patterncnt = 5;
	if(NULL == (p->patterns = p->malloc(p, p->patterncnt*sizeof(PSPattern *), _("Allocate memory for internal Pattern list of document.")))) {
		return(NULL);
	}
	memset((void *)p->patterns, 0, p->patterncnt*sizeof(PSPattern *));

	/* Initialize list of spot colors */
	p->spotcolorcnt = 5;
	if(NULL == (p->spotcolors = p->malloc(p, p->spotcolorcnt*sizeof(PSSpotColor *), _("Allocate memory for internal spot color list of document.")))) {
		return(NULL);
	}
	memset((void *)p->spotcolors, 0, p->spotcolorcnt*sizeof(PSSpotColor *));

	/* Initialize list of shadings */
	p->shadingcnt = 5;
	if(NULL == (p->shadings = p->malloc(p, p->shadingcnt*sizeof(PSShading *), _("Allocate memory for internal Shading list of document.")))) {
		return(NULL);
	}
	memset((void *)p->shadings, 0, p->shadingcnt*sizeof(PSShading *));

	/* Initialize list of graphic states*/
	p->gstatecnt = 5;
	if(NULL == (p->gstates = p->malloc(p, p->gstatecnt*sizeof(PSGState *), _("Allocate memory for internal graphic state list of document.")))) {
		return(NULL);
	}
	memset((void *)p->gstates, 0, p->gstatecnt*sizeof(PSGState *));

	p->agstate = 0;
	p->agstates[0].x = 0.0;
	p->agstates[0].y = 0.0;
	p->agstates[0].strokecolor.colorspace = PS_COLORSPACE_GRAY;
	p->agstates[0].strokecolor.c1 = 0.0;
	p->agstates[0].strokecolorinvalid = ps_false;
	p->agstates[0].fillcolor.colorspace = PS_COLORSPACE_GRAY;
	p->agstates[0].fillcolor.c1 = 0.0;
	p->agstates[0].fillcolorinvalid = ps_false;
	p->tstate = 0;
	p->tstates[0].tx = 0.0;
	p->tstates[0].ty = 0.0;
	p->tstates[0].cx = 0.0;
	p->tstates[0].cy = 0.0;
	p->closefp = ps_false;
	p->page_open = ps_false;
	p->doc_open = ps_false;
	p->scopecount = 0;
	p->scopes[0] = PS_SCOPE_OBJECT;
	p->headerwritten = ps_false;
	p->commentswritten = ps_false;
	p->beginprologwritten = ps_false;
	p->endprologwritten = ps_false;
	p->setupwritten = ps_false;
	p->border_width = 1.0;
	p->border_style = PS_BORDER_SOLID;
	p->border_black = 3.0;
	p->border_white = 3.0;
	p->border_red = 0.0;
	p->border_green = 0.0;
	p->border_blue = 1.0;
	p->textrendering = -1;

	return(p);
}
/* }}} */

/* PS_new() {{{
 * Create a new PostScript Document. Use the internal memory management
 * functions and error handler.
 */
PSLIB_API PSDoc * PSLIB_CALL
PS_new(void) {
	return(PS_new2(NULL, NULL, NULL, NULL, NULL));
}
/* }}} */

/* PS_get_opaque() {{{
 * Returns the pointer on the user data as it is passed to each call
 * of the errorhandler.
 */
PSLIB_API void * PSLIB_CALL
PS_get_opaque(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(NULL);
	}
	return(psdoc->user_data);
}
/* }}}  */

/* ps_write_ps_comments() {{{ */
static void ps_write_ps_comments(PSDoc *psdoc) {
	int i;
	time_t ps_calendar_time;
	struct tm *ps_local_tm;
	if(NULL != (psdoc->CreationDate = psdoc->malloc(psdoc, LINEBUFLEN/2, _("Allocate memory for PS header field 'CreationTime'.")))) {
		ps_calendar_time = time(NULL);
		if(ps_calendar_time == (time_t)(-1)) {
			sprintf(psdoc->CreationDate,"%s","20/3/2001 3:30 AM");
		} else {
			ps_local_tm = localtime(&ps_calendar_time);
			strftime(psdoc->CreationDate, LINEBUFLEN/2, "%d/%m/%Y %I:%M %p", ps_local_tm);
		}
	} else {
		ps_error(psdoc, PS_MemoryError, _("Cannot allocate memory for PS header field 'CreationTime'."));
	}

	ps_printf(psdoc, "%%!PS-Adobe-3.0\n");
	if(psdoc->Creator)
		ps_printf(psdoc, "%%%%Creator: %s (%s)\n", psdoc->Creator, "pslib " LIBPS_DOTTED_VERSION);
	else
		ps_printf(psdoc, "%%%%Creator: %s\n", "pslib " LIBPS_DOTTED_VERSION);
	if(psdoc->CreationDate) {
		ps_printf(psdoc, "%%%%Creation-Date: %s\n", psdoc->CreationDate);
	}
	if(psdoc->Title)
		ps_printf(psdoc, "%%%%Title: %s\n", psdoc->Title);
	if(psdoc->Author)
		ps_printf(psdoc, "%%%%Author: %s\n", psdoc->Author);
	ps_printf(psdoc, "%%%%PageOrder: Ascend\n");
	ps_printf(psdoc, "%%%%Pages: (atend)\n");
	if(psdoc->BoundingBox) {
		ps_printf(psdoc, "%%%%BoundingBox: %s\n", psdoc->BoundingBox);
	} else {
		ps_printf(psdoc, "%%%%BoundingBox: (atend)\n");
	}
	if(psdoc->Orientation)
		ps_printf(psdoc, "%%%%Orientation: %s\n", psdoc->Orientation);
	else
		ps_printf(psdoc, "%%%%Orientation: (atend)\n");
	ps_printf(psdoc, "%%%%DocumentProcessColors: Black\n");
	ps_printf(psdoc, "%%%%DocumentCustomColors: \n");
	for(i=0; i<psdoc->spotcolorcnt; i++) {
		PSSpotColor *spotcolor = psdoc->spotcolors[i];
		if(NULL != spotcolor)
			ps_printf(psdoc, "%%%%+ (%s)\n", spotcolor->name);
	}
	ps_printf(psdoc, "%%%%CMYKCustomColor: \n");
	for(i=0; i<psdoc->spotcolorcnt; i++) {
		PSSpotColor *spotcolor = psdoc->spotcolors[i];
		if(NULL != spotcolor && spotcolor->colorspace == PS_COLORSPACE_CMYK)
			ps_printf(psdoc, "%%%%+ %f %f %f %f (%s)\n", spotcolor->c1, spotcolor->c2, spotcolor->c3, spotcolor->c4, spotcolor->name);
	}
	ps_printf(psdoc, "%%%%RGBCustomColor: \n");
	for(i=0; i<psdoc->spotcolorcnt; i++) {
		PSSpotColor *spotcolor = psdoc->spotcolors[i];
		if(NULL != spotcolor && spotcolor->colorspace == PS_COLORSPACE_RGB)
			ps_printf(psdoc, "%%%%+ %f %f %f (%s)\n", spotcolor->c1, spotcolor->c2, spotcolor->c3, spotcolor->name);
	}
	ps_printf(psdoc, "%%%%EndComments\n");
	psdoc->commentswritten = ps_true;
}
/* }}} */

/* ps_write_ps_beginprolog() {{{ */
static void ps_write_ps_beginprolog(PSDoc *psdoc) {
	ps_enter_scope(psdoc, PS_SCOPE_PROLOG);
	ps_printf(psdoc, "%%%%BeginProlog\n");
	ps_printf(psdoc, "%%%%BeginResource: definicoes\n");
	ps_printf(psdoc, "%%%%EndResource\n");
	ps_printf(psdoc, "%%%%BeginProcSet: standard\n");

	ps_printf(psdoc, "/PslibDict 300 dict def PslibDict begin/N{def}def/B{bind def}N\n");
	ps_printf(psdoc, "/TR{translate}N/vsize 11 72 mul N/hsize 8.5 72 mul N/isls false N\n");
	ps_printf(psdoc, "/p{show}N/w{0 rmoveto}B/a{moveto}B/l{lineto}B");
	ps_printf(psdoc, "/qs{currentpoint\n");
	ps_printf(psdoc, "currentpoint newpath moveto 3 2 roll dup true charpath stroke\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qf{currentpoint\n");
	ps_printf(psdoc, "currentpoint newpath moveto 3 2 roll dup true charpath fill\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qsf{currentpoint\n");
	ps_printf(psdoc, "currentpoint newpath moveto 3 2 roll dup true charpath gsave stroke grestore fill\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qc{currentpoint\n");
	ps_printf(psdoc, "currentpoint newpath moveto 3 2 roll dup true charpath clip\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qsc{currentpoint\n");
	ps_printf(psdoc, "currentpoint initclip newpath moveto 3 2 roll dup true charpath clip stroke\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qfc{currentpoint\n");
	ps_printf(psdoc, "currentpoint initclip newpath moveto 3 2 roll dup true charpath clip fill\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qfsc{currentpoint\n");
	ps_printf(psdoc, "currentpoint initclip newpath moveto 3 2 roll dup true charpath gsave stroke grestore clip fill\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/qi{currentpoint\n");
	ps_printf(psdoc, "3 2 roll\n");
	ps_printf(psdoc, "stringwidth pop 3 2 roll add exch moveto}B");
	ps_printf(psdoc, "/tr{currentpoint currentpoint 5 4 roll add moveto}B");
	ps_printf(psdoc, "/rt{moveto}B");
	ps_printf(psdoc, "/#copies{1}B\n");
	ps_printf(psdoc, "/PslibPageBeginHook{pop pop pop pop pop}B\n");
	ps_printf(psdoc, "/PslibPageEndHook{pop}B\n");
	ps_printf(psdoc, "\n");

	ps_printf(psdoc, "/reencdict 12 dict def /ReEncode { reencdict begin\n");
	ps_printf(psdoc, "/newcodesandnames exch def /newfontname exch def /basefontname exch def\n");
	ps_printf(psdoc, "/basefontdict basefontname findfont def /newfont basefontdict maxlength dict def\n");
	ps_printf(psdoc, "basefontdict { exch dup /FID ne { dup /Encoding eq\n");
	ps_printf(psdoc, "{ exch dup length array copy newfont 3 1 roll put }\n");
	ps_printf(psdoc, "{ exch newfont 3 1 roll put } ifelse } { pop pop } ifelse } forall\n");
	ps_printf(psdoc, "newfont /FontName newfontname put newcodesandnames aload pop\n");
	ps_printf(psdoc, "128 1 255 { newfont /Encoding get exch /.notdef put } for\n");
	ps_printf(psdoc, "newcodesandnames length 2 idiv { newfont /Encoding get 3 1 roll put } repeat\n");
	ps_printf(psdoc, "newfontname newfont definefont pop end } def\n");
	ps_printf(psdoc, "end\n");
	ps_printf(psdoc, "%%%%EndProcSet\n");
	ps_printf(psdoc, "%%%%BeginProcSet: colorsep\n");
	ps_printf(psdoc, "%%!\n");
	ps_printf(psdoc, "%% Colour separation.\n");
	ps_printf(psdoc, "%% Ask dvips to do 4 pages. In bop-hook, cycle\n");
	ps_printf(psdoc, "%% round CMYK color spaces.\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% Sebastian Rahtz 30.9.93\n");
	ps_printf(psdoc, "%% checked 7.1.94\n");
	ps_printf(psdoc, "%% from Green Book, and Kunkel Graphic Design with PostScript\n");
	ps_printf(psdoc, "%% (Green Book Listing 9-5, on page 153.)\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% This work is placed in the public domain\n");
	ps_printf(psdoc, "/seppages  0  def \n");
	ps_printf(psdoc, "userdict begin\n");
	ps_printf(psdoc, "/Min {%% 3 items on stack\n");
	ps_printf(psdoc, "2 copy lt { pop }{ exch pop } ifelse\n");
	ps_printf(psdoc, "2 copy lt { pop }{ exch pop } ifelse\n");
	ps_printf(psdoc, "} def\n");
	ps_printf(psdoc, "/SetGray {\n");
	ps_printf(psdoc, " 1 exch sub systemdict begin adjustdot setgray end	\n");
	ps_printf(psdoc, "} def\n");
	ps_printf(psdoc, "/sethsbcolor {systemdict begin\n");
	ps_printf(psdoc, "  sethsbcolor currentrgbcolor end\n");
	ps_printf(psdoc, "  userdict begin setrgbcolor end}def \n");
	ps_printf(psdoc, "\n");
	ps_printf(psdoc, "/ToCMYK\n");
	ps_printf(psdoc, "%% Red book p 305\n");
	ps_printf(psdoc, "  {\n");
	ps_printf(psdoc, "%% subtract each colour from 1\n");
	ps_printf(psdoc, "  3 { 1 exch sub 3 1 roll } repeat\n");
	ps_printf(psdoc, "%% define percent of black undercolor\n");
	ps_printf(psdoc, "%% find minimum (k)\n");
	ps_printf(psdoc, "  3 copy  Min \n");
	ps_printf(psdoc, "%% remove undercolor\n");
	ps_printf(psdoc, "  blackUCR sub\n");
	ps_printf(psdoc, "  dup 0 lt {pop 0} if \n");
	ps_printf(psdoc, "  /percent_UCR exch def \n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% subtract that from each colour\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "  3 { percent_UCR sub 3 1 roll } repeat \n");
	ps_printf(psdoc, "%% work out black itself\n");
	ps_printf(psdoc, "  percent_UCR 1.25 mul %% 1 exch sub\n");
	ps_printf(psdoc, "%% stack should now have C M Y K\n");
	ps_printf(psdoc, "} def \n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% crop marks\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "/cX 18 def \n");
	ps_printf(psdoc, "/CM{gsave TR 0 cX neg moveto 0 cX lineto stroke\n");
	ps_printf(psdoc, "cX neg 0 moveto cX 0 lineto stroke grestore}def \n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "/bop-hook{cX dup TR\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% which page are we producing\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "   seppages 1 add \n");
	ps_printf(psdoc, "    /seppages exch def\n");
	ps_printf(psdoc, "     seppages 5 eq { /seppages  1  def } if\n");
	ps_printf(psdoc, "     seppages 1 eq { \n");
	ps_printf(psdoc, "      /ColourName (CYAN) def \n");
	ps_printf(psdoc, "      CYAN setupcolor    \n");
	ps_printf(psdoc, "      /WhichColour 3 def } if \n");
	ps_printf(psdoc, "   seppages 2 eq { \n");
	ps_printf(psdoc, "      /ColourName (MAGENTA) def \n");
	ps_printf(psdoc, "      MAGENTA setupcolor \n");
	ps_printf(psdoc, "     /WhichColour 2 def } if\n");
	ps_printf(psdoc, "   seppages 3 eq { \n");
	ps_printf(psdoc, "      /ColourName (YELLOW) def\n");
	ps_printf(psdoc, "      YELLOW setupcolor  \n");
	ps_printf(psdoc, "      /WhichColour 1 def } if \n");
	ps_printf(psdoc, "   seppages 4 eq { \n");
	ps_printf(psdoc, "      /ColourName (BLACK) def \n");
	ps_printf(psdoc, "      BLACK setupcolor   \n");
	ps_printf(psdoc, "      /WhichColour 0 def } if \n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% crop marks\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "gsave .3 setlinewidth \n");
	ps_printf(psdoc, "3 -7 moveto\n");
	ps_printf(psdoc, "/Helvetica findfont 6 scalefont setfont\n");
	ps_printf(psdoc, "ColourName show\n");
	ps_printf(psdoc, "0 0 CM \n");
	ps_printf(psdoc, "vsize cX 2 mul sub dup hsize cX 2 mul sub dup isls{4 2 roll}if 0 CM \n");
	ps_printf(psdoc, "exch CM 0 \n");
	ps_printf(psdoc, "exch CM \n");
	ps_printf(psdoc, "grestore 0 cX -2 mul TR isls\n");
	ps_printf(psdoc, "{cX -2 mul 0 TR}if\n");
	ps_printf(psdoc, "	  } def end\n");
	ps_printf(psdoc, "%% \n");
	ps_printf(psdoc, "/separations 48 dict def\n");
	ps_printf(psdoc, "separations begin\n");
	ps_printf(psdoc, "   /cmykprocs [ %%def\n");
	ps_printf(psdoc, "       %% cyan\n");
	ps_printf(psdoc, "    { pop pop  pop SetGray  }\n");
	ps_printf(psdoc, "       %% magenta\n");
	ps_printf(psdoc, "    { pop pop exch pop SetGray  }\n");
	ps_printf(psdoc, "       %% yellow\n");
	ps_printf(psdoc, "    { pop 3 1 roll pop pop SetGray  }\n");
	ps_printf(psdoc, "       %% black\n");
	ps_printf(psdoc, "    { 4 1 roll pop pop pop SetGray  }\n");
	ps_printf(psdoc, "   ] def\n");
	ps_printf(psdoc, "   /rgbprocs [ %%def\n");
	ps_printf(psdoc, "       %% cyan\n");
	ps_printf(psdoc, "    { ToCMYK pop pop pop SetGray }\n");
	ps_printf(psdoc, "       %% magenta\n");
	ps_printf(psdoc, "    { ToCMYK pop pop exch pop SetGray }\n");
	ps_printf(psdoc, "       %% yellow\n");
	ps_printf(psdoc, "    { ToCMYK pop 3 1 roll pop pop SetGray }\n");
	ps_printf(psdoc, "       %% black\n");
	ps_printf(psdoc, "    { ToCMYK 4 1 roll pop pop pop SetGray  }\n");
	ps_printf(psdoc, "   ] def\n");
	ps_printf(psdoc, "   /testprocs [ %%def\n");
	ps_printf(psdoc, "       %% cyan\n");
	ps_printf(psdoc, "    { ToCMYK pop pop pop  }\n");
	ps_printf(psdoc, "       %% magenta\n");
	ps_printf(psdoc, "    { ToCMYK pop pop exch pop  }\n");
	ps_printf(psdoc, "       %% yellow\n");
	ps_printf(psdoc, "    { ToCMYK pop 3 1 roll pop pop  }\n");
	ps_printf(psdoc, "       %% black\n");
	ps_printf(psdoc, "    { ToCMYK 4 1 roll pop pop pop   }\n");
	ps_printf(psdoc, "   ] def\n");
	ps_printf(psdoc, "   /screenangles [ %%def\n");
	ps_printf(psdoc, "       105  %% cyan\n");
	ps_printf(psdoc, "       75    %% magenta\n");
	ps_printf(psdoc, "       0      %% yellow\n");
	ps_printf(psdoc, "       45    %% black\n");
	ps_printf(psdoc, "   ] def\n");
	ps_printf(psdoc, "end  %% separations\n");
	ps_printf(psdoc, "\n");
	ps_printf(psdoc, "%% setupcolortakes 0, 1, 2, or 3 as its argument,\n");
	ps_printf(psdoc, "%% for cyan, magenta, yellow, and black.\n");
	ps_printf(psdoc, "/CYAN 0 def           /MAGENTA 1 def\n");
	ps_printf(psdoc, "/YELLOW 2 def         /BLACK 3 def\n");
	ps_printf(psdoc, "/setupcolor{ %%def\n");
	ps_printf(psdoc, "   userdict begin\n");
	ps_printf(psdoc, "       dup separations /cmykprocs get exch get\n");
	ps_printf(psdoc, "       /setcmykcolor exch def\n");
	ps_printf(psdoc, "       dup separations /rgbprocs get exch get\n");
	ps_printf(psdoc, "       /setrgbcolor exch def\n");
	ps_printf(psdoc, "       dup separations /testprocs get exch get\n");
	ps_printf(psdoc, "       /testrgbcolor exch def\n");
	ps_printf(psdoc, "       separations /screenangles get exch get\n");
	ps_printf(psdoc, "       currentscreen\n");
	ps_printf(psdoc, "           exch pop 3 -1 roll exch\n");
	ps_printf(psdoc, "       setscreen\n");
	ps_printf(psdoc, "       /setscreen { pop pop pop } def\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% redefine setgray so that it only shows on the black separation\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "      /setgray {\n");
	ps_printf(psdoc, "       WhichColour 0 eq\n");
	ps_printf(psdoc, "       {systemdict begin adjustdot setgray end} \n");
	ps_printf(psdoc, "       {pop systemdict begin 1 setgray end}\n");
	ps_printf(psdoc, "       ifelse}def \n");
	ps_printf(psdoc, "   end\n");
	ps_printf(psdoc, "} bind def\n");
	ps_printf(psdoc, "\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% from Kunkel\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "/adjustdot { dup 0 eq { } { dup 1 exch sub .1 mul add} ifelse } def\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% redefine existing operators\n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% Percent of undercolor removal\n");
	ps_printf(psdoc, "/magentaUCR .3 def  \n");
	ps_printf(psdoc, "/yellowUCR .07 def  \n");
	ps_printf(psdoc, "/blackUCR .4 def \n");
	ps_printf(psdoc, "%%\n");
	ps_printf(psdoc, "%% Correct yellow and magenta\n");
	ps_printf(psdoc, "/correctMY {rgb2cym\n");
	ps_printf(psdoc, "  1 index yellowUCR mul sub 3 1 roll\n");
	ps_printf(psdoc, "  1 index magentaUCR mul sub 3 1 roll\n");
	ps_printf(psdoc, "  3 1 roll rgb2cym}def\n");
	ps_printf(psdoc, "%% \n");
	ps_printf(psdoc, "%%(bluely green ) =\n");
	ps_printf(psdoc, "%%CYAN setupcolor\n");
	ps_printf(psdoc, "%%.1 .4 .5  testrgbcolor =\n");
	ps_printf(psdoc, "%%MAGENTA setupcolor\n");
	ps_printf(psdoc, "%%.1 .4 .5  testrgbcolor =\n");
	ps_printf(psdoc, "%%YELLOW setupcolor\n");
	ps_printf(psdoc, "%%.1 .4 .5  testrgbcolor =\n");
	ps_printf(psdoc, "%%BLACK setupcolor\n");
	ps_printf(psdoc, "%%.1 .4 .5  testrgbcolor =\n");
	ps_printf(psdoc, "%%quit\n");
	ps_printf(psdoc, "%%%%EndProcSet\n");

	/* The fontenc vector is placed outside the PslibDict dictionary */
	{
	int i, j;
	ENCODING *fontenc;
	fontenc = &fontencoding[0];
	ps_printf(psdoc, "/fontenc-%s [\n", fontenc->name);
	for(i=0; i<32; i++) {
		for(j=0; j<8; j++) {
			if((fontenc->vec[i*8+j] != NULL) && (*(fontenc->vec[i*8+j]) != '\0'))
				ps_printf(psdoc, "8#%03o /%s ", i*8+j, fontenc->vec[i*8+j]);
		}
		ps_printf(psdoc, "\n");
	}
	ps_printf(psdoc, "] def\n");
	}

	ps_printf(psdoc, "/pdfmark where {pop} {userdict /pdfmark /cleartomark load put} ifelse\n");
	if(psdoc->Creator)
		ps_printf(psdoc, "[ /Creator (%s \\(%s\\))\n", psdoc->Creator, "pslib " LIBPS_DOTTED_VERSION);
	else
		ps_printf(psdoc, "[ /Creator (%s)\n", "pslib " LIBPS_DOTTED_VERSION);
	if(psdoc->CreationDate) {
		ps_printf(psdoc, "  /Creation-Date (%s)\n", psdoc->CreationDate);
	}
	if(psdoc->Title)
		ps_printf(psdoc, "  /Title (%s)\n", psdoc->Title);
	if(psdoc->Author)
		ps_printf(psdoc, "  /Author (%s)\n", psdoc->Author);
	if(psdoc->Keywords)
		ps_printf(psdoc, "  /Keywords (%s)\n", psdoc->Keywords);
	if(psdoc->Subject)
		ps_printf(psdoc, "  /Subject (%s)\n", psdoc->Subject);
	ps_printf(psdoc, "/DOCINFO pdfmark\n");
	psdoc->beginprologwritten = ps_true;
}
/* }}} */

/* ps_write_ps_endprolog() {{{ */
static void ps_write_ps_endprolog(PSDoc *psdoc) {
	ps_printf(psdoc, "%%%%EndProlog\n");
	ps_leave_scope(psdoc, PS_SCOPE_PROLOG);
	psdoc->endprologwritten = ps_true;
}
/* }}} */

/* ps_write_ps_setup() {{{ */
static void ps_write_ps_setup(PSDoc *psdoc) {
	ps_printf(psdoc, "%%%%BeginSetup\n");
	ps_printf(psdoc, "PslibDict begin\n");
	if(psdoc->copies > 1)
		ps_printf(psdoc, "/#copies %d def\n", psdoc->copies);
	ps_printf(psdoc, "%%%%EndSetup\n");
	psdoc->setupwritten = ps_true;
}
/* }}} */

/* ps_write_ps_header() {{{
 * Write the PostScript header
 */
static void ps_write_ps_header(PSDoc *psdoc) {
	if(psdoc->headerwritten == ps_true)
		return;
	if(psdoc->commentswritten == ps_false)
		ps_write_ps_comments(psdoc);
	if(psdoc->beginprologwritten == ps_false)
		ps_write_ps_beginprolog(psdoc);
	if(psdoc->endprologwritten == ps_false)
		ps_write_ps_endprolog(psdoc);
	if(psdoc->setupwritten == ps_false)
		ps_write_ps_setup(psdoc);
	psdoc->headerwritten = ps_true;
}
/* }}} */

/* ps_setcolor() {{{
 * Outputs PostScript commands to set the color, but checks before
 * if it was already set. whichcolor is either PS_COLORTYPE_FILL
 * or PS_COLORTYPE_STROKE. The function only outputs something if
 * [fill|stroke]colorinvalid is set. [fill|stroke]colorinvalid is set
 * when the color is set by PS_setcolor() or this function has set
 * the fill or stroke color.
 */
static void ps_setcolor(PSDoc *psdoc, int whichcolor) {
	PSColor *currentcolor = NULL;

	if(ps_check_scope(psdoc, PS_SCOPE_PATTERN)) {
		if(psdoc->pattern->painttype == 2) {
			ps_error(psdoc, PS_Warning, _("Setting color inside a pattern of PaintType 2 is not allowed."), __FUNCTION__);
			return;
		}
	}

	switch(whichcolor) {
		case PS_COLORTYPE_FILL:
			if(psdoc->agstates[psdoc->agstate].fillcolorinvalid) {
				psdoc->agstates[psdoc->agstate].strokecolorinvalid = ps_true;
				psdoc->agstates[psdoc->agstate].fillcolorinvalid = ps_false;
				currentcolor = &(psdoc->agstates[psdoc->agstate].fillcolor);
			}
			break;
		case PS_COLORTYPE_STROKE:
			if(psdoc->agstates[psdoc->agstate].strokecolorinvalid) {
				psdoc->agstates[psdoc->agstate].fillcolorinvalid = ps_true;
				psdoc->agstates[psdoc->agstate].strokecolorinvalid = ps_false;
				currentcolor = &(psdoc->agstates[psdoc->agstate].strokecolor);
			}
			break;
	}
	if(currentcolor) {
		switch(currentcolor->colorspace) {
			case PS_COLORSPACE_GRAY:
				ps_printf(psdoc, "%f setgray\n", currentcolor->c1);
				break;
			case PS_COLORSPACE_RGB:
				ps_printf(psdoc, "%.4f %.4f %.4f setrgbcolor\n", currentcolor->c1, currentcolor->c2, currentcolor->c3);
				break;
			case PS_COLORSPACE_CMYK:
				ps_printf(psdoc, "%.4f %.4f %.4f %.4f setcmykcolor\n", currentcolor->c1, currentcolor->c2, currentcolor->c3, currentcolor->c4);
				break;
			case PS_COLORSPACE_PATTERN: {
				PSPattern *pspattern = _ps_get_pattern(psdoc, (int) currentcolor->pattern);
				if(NULL == pspattern) {
					ps_error(psdoc, PS_RuntimeError, _("PSPattern is null."));
					return;
				}
				if(pspattern->painttype == 1) {
					ps_printf(psdoc, "%s setpattern\n", pspattern->name);
				} else {
					ps_printf(psdoc, "[/Pattern [/");
					switch(currentcolor->prevcolorspace) {
						case PS_COLORSPACE_GRAY:
							ps_printf(psdoc, "DeviceGray]] setcolorspace\n");
							ps_printf(psdoc, "%.4f %s setcolor\n", currentcolor->c1, pspattern->name);
							break;
						case PS_COLORSPACE_CMYK:
							ps_printf(psdoc, "DeviceCMYK]] setcolorspace\n");
							ps_printf(psdoc, "%.4f %.4f %.4f %.4f %s setcolor\n", currentcolor->c1, currentcolor->c2, currentcolor->c3, currentcolor->c4, pspattern->name);
							break;
						case PS_COLORSPACE_RGB:
							ps_printf(psdoc, "DeviceRGB]] setcolorspace\n");
							ps_printf(psdoc, "%.4f %.4f %.4f %s setcolor\n", currentcolor->c1, currentcolor->c2, currentcolor->c3, pspattern->name);
							break;
						case PS_COLORSPACE_SPOT: {
							PSSpotColor *spotcolor;
							spotcolor = _ps_get_spotcolor(psdoc, (int) currentcolor->c1);
							if(!spotcolor) {
								ps_error(psdoc, PS_RuntimeError, _("Could not find spot color."));
								return;
							}
							ps_printf(psdoc, "Separation (%s)\n", spotcolor->name);
							switch(spotcolor->colorspace) {
								case PS_COLORSPACE_GRAY:
									ps_printf(psdoc, "  /DeviceGray { 1 %f sub mul 1 exch sub }\n", spotcolor->c1);
									break;
								case PS_COLORSPACE_RGB: {
									float max;
									max = (spotcolor->c1 > spotcolor->c2) ? spotcolor->c1 : spotcolor->c2;
									max = (max > spotcolor->c3) ? max : spotcolor->c3;
									ps_printf(psdoc, "  /DeviceRGB { 1 exch sub dup dup %f exch sub %f mul add exch dup dup %f exch sub %f mul add exch dup %f exch sub %f mul add }\n", max, spotcolor->c1, max, spotcolor->c2, max, spotcolor->c3);
									break;
								}
								case PS_COLORSPACE_CMYK:	
									ps_printf(psdoc, "  /DeviceCMYK { dup %f mul exch dup %f mul exch dup %f mul exch %f mul }\n", spotcolor->c1, spotcolor->c2, spotcolor->c3, spotcolor->c4);
									break;
							}
							ps_printf(psdoc, "]] setcolorspace\n");
							break;
						}
						default:
							ps_error(psdoc, PS_Warning, _("Current stroke/fill color is not RGB, CMYK, Gray or spot while setting a pattern of paint type 1."));
					}
				}
				break;
			}
			case PS_COLORSPACE_SPOT: {
				PSSpotColor *spotcolor;
				spotcolor = _ps_get_spotcolor(psdoc, (int) currentcolor->c1);
				if(!spotcolor) {
					ps_error(psdoc, PS_RuntimeError, _("Could not find spot color."));
					return;
				}
				ps_printf(psdoc, "[ /Separation (%s)\n", spotcolor->name);
				switch(spotcolor->colorspace) {
					case PS_COLORSPACE_GRAY:
						ps_printf(psdoc, "  /DeviceGray { 1 %f sub mul 1 exch sub }\n", spotcolor->c1);
						break;
					case PS_COLORSPACE_RGB: {
						float max;
						max = (spotcolor->c1 > spotcolor->c2) ? spotcolor->c1 : spotcolor->c2;
						max = (max > spotcolor->c3) ? max : spotcolor->c3;
						ps_printf(psdoc, "  /DeviceRGB { 1 exch sub dup dup %f exch sub %f mul add exch dup dup %f exch sub %f mul add exch dup %f exch sub %f mul add }\n", max, spotcolor->c1, max, spotcolor->c2, max, spotcolor->c3);
						break;
					}
					case PS_COLORSPACE_CMYK:	
						ps_printf(psdoc, "  /DeviceCMYK { dup %f mul exch dup %f mul exch dup %f mul exch %f mul }\n", spotcolor->c1, spotcolor->c2, spotcolor->c3, spotcolor->c4);
						break;
				}
				ps_printf(psdoc, "] setcolorspace\n");
				ps_printf(psdoc, "%f setcolor\n", currentcolor->c2);
				break;
			}
		}
	}
}
/* }}} */

/* PS_open_fp() {{{
 * Associates an already open file with the PostScript document created
 * with PS_new().
 */
PSLIB_API int PSLIB_CALL
PS_open_fp(PSDoc *psdoc, FILE *fp) {
	if(NULL == fp) {
		ps_error(psdoc, PS_Warning, _("File pointer is NULL. Use PS_open_mem() to create file in memory."));
		return(-1);
	}
	psdoc->fp = fp;
	psdoc->closefp = ps_false;
	psdoc->writeproc = ps_writeproc_file;
	psdoc->page = 0;
	psdoc->doc_open = ps_true;
	ps_enter_scope(psdoc, PS_SCOPE_DOCUMENT);

	return(0);
}
/* }}} */

/* PS_open_file() {{{
 * Associates a file to the PostScript document created with PS_new().
 */
PSLIB_API int PSLIB_CALL
PS_open_file(PSDoc *psdoc, const char *filename) {
	FILE *fp;

	if(filename == NULL || filename[0] == '\0' || (filename[0] == '-' && filename[1] == '\0')) {
		PS_open_mem(psdoc, NULL);
		return 0;
	} else {
		fp = fopen(filename, "w");
		if(NULL == fp) {
			ps_error(psdoc, PS_IOError, _("Could not open file '%s'."), filename);
			return -1;
		}

		if(0 > PS_open_fp(psdoc, fp)) {
			fclose(fp);
			return(-1);
		}
		psdoc->closefp = ps_true;
	}
	return 0;
}
/* }}} */

/* PS_open_mem() {{{
 * Create document in memory. Actually you are just passing a write procedure
 * which is used instead of the internal procedure.
 */
PSLIB_API int PSLIB_CALL
PS_open_mem(PSDoc *p, size_t (*writeproc)(PSDoc *p, void *data, size_t size)) {
	if (writeproc == NULL) {
		p->sb = str_buffer_new(p, 1000);
		p->writeproc = ps_writeproc_buffer;
	} else {
		p->writeproc = writeproc;
	}
	p->fp = NULL;
	p->page = 0;
	p->doc_open = ps_true;
	ps_enter_scope(p, PS_SCOPE_DOCUMENT);

	return 0;
}
/* }}} */

/* _output_bookmarks() {{{
 *
 */
void _output_bookmarks(PSDoc *psdoc, DLIST *bookmarks) {
	PS_BOOKMARK *bm;
	for(bm = dlst_last(bookmarks); bm != NULL; bm = dlst_prev(bm)) {
		char *str;
		ps_printf(psdoc, "[ /Page %d /Title (", bm->page);
		str = bm->text;
		while(*str != '\0') {
			unsigned char index = (unsigned char) *str;
			/* Everything below 32, above 127 and '(' and ')' should be output
			 * as octal values.
			 */
			if(index < 32 || index > 127 || index == '(' || index == ')' || index == '\\') {
				ps_printf(psdoc, "\\%03o", index);
			} else {
				ps_putc(psdoc, *str);
			}
			str++;
		}
		ps_printf(psdoc, ") /Count %d /OUT pdfmark\n", (bm->open == 0) ? bm->children->count : -bm->children->count);
		if(bm->children->count)
			_output_bookmarks(psdoc, bm->children);
	}
}
/* }}} */

/* PS_close() {{{
 * Closes a PostScript document. It closes the actual file only if the
 * document was opened with PS_open_file(). This function does not
 * free resources. Use PS_delete() to do that.
 */
PSLIB_API void PSLIB_CALL
PS_close(PSDoc *psdoc) {
	/* End page if it is still open */
	if(psdoc->page_open == ps_true) {
		ps_error(psdoc, PS_Warning, _("Ending last page of document."));
		PS_end_page(psdoc);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' scope."), __FUNCTION__);
		return;
	}

	/* Write the trailer if the document was not close before */
	if(psdoc->doc_open == ps_true) {
		ps_printf(psdoc, "%%%%Trailer\n");
		ps_printf(psdoc, "end\n");
		if(psdoc->bookmarks->count > 0) {
			_output_bookmarks(psdoc, psdoc->bookmarks);
		}
		ps_printf(psdoc, "%%%%Pages: %d\n", psdoc->page);
		ps_printf(psdoc, "%%%%BoundingBox: %s\n", psdoc->BoundingBox);
		ps_printf(psdoc, "%%%%Orientation: %s\n", psdoc->Orientation);
		ps_printf(psdoc, "%%%%EOF");
		ps_leave_scope(psdoc, PS_SCOPE_DOCUMENT);
	}

	/* FIXME: Need to free the linked lists parameters, categories and values */
	if((psdoc->closefp == ps_true) && (NULL != psdoc->fp)) {
		fclose(psdoc->fp);
		psdoc->fp = NULL;
	}

	psdoc->doc_open = ps_false;
}
/* }}} */

/* PS_delete() {{{
 * Frees all resources of a document. If the document was not closed before,
 * it will also be closed.
 */
PSLIB_API void PSLIB_CALL
PS_delete(PSDoc *psdoc) {
	int i;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	/* Make sure the document is closed */
	if(psdoc->doc_open == ps_true) {
		PS_close(psdoc);
	}

	if(psdoc->sb)
		str_buffer_delete(psdoc, psdoc->sb);

	/* Free the memory */
	ps_del_resources(psdoc);
	ps_del_parameters(psdoc);
	ps_del_values(psdoc);
	ps_del_bookmarks(psdoc, psdoc->bookmarks);
	psdoc->bookmarks = NULL;

	if(psdoc->Author) {
		psdoc->free(psdoc, psdoc->Author);
		psdoc->Author = NULL;
	}
	if(psdoc->Keywords) {
		psdoc->free(psdoc, psdoc->Keywords);
		psdoc->Keywords = NULL;
	}
	if(psdoc->Subject) {
		psdoc->free(psdoc, psdoc->Subject);
		psdoc->Subject = NULL;
	}
	if(psdoc->Title) {
		psdoc->free(psdoc, psdoc->Title);
		psdoc->Title = NULL;
	}
	if(psdoc->Creator) {
		psdoc->free(psdoc, psdoc->Creator);
		psdoc->Creator = NULL;
	}
	if(psdoc->BoundingBox) {
		psdoc->free(psdoc, psdoc->BoundingBox);
		psdoc->BoundingBox = NULL;
	}
	if(psdoc->Orientation) {
		psdoc->free(psdoc, psdoc->Orientation);
		psdoc->Orientation = NULL;
	}
	if(psdoc->CreationDate) {
		psdoc->free(psdoc, psdoc->CreationDate);
		psdoc->CreationDate = NULL;
	}

	/* Freeing font resources */
	i = 0;
	while(i < psdoc->fontcnt) {
		if(psdoc->fonts[i]) {
			_ps_delete_font(psdoc, psdoc->fonts[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->fonts);

	/* Freeing image resources */
	i = 0;
	while(i < psdoc->imagecnt) {
		if(psdoc->images[i]) {
			_ps_delete_image(psdoc, psdoc->images[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->images);

	/* Freeing pattern resources */
	i = 0;
	while(i < psdoc->patterncnt) {
		if(psdoc->patterns[i]) {
			_ps_delete_pattern(psdoc, psdoc->patterns[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->patterns);

	/* Freeing spotcolor resources */
	i = 0;
	while(i < psdoc->spotcolorcnt) {
		if(psdoc->spotcolors[i]) {
			_ps_delete_spotcolor(psdoc, psdoc->spotcolors[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->spotcolors);

	/* Freeing shading resources */
	i = 0;
	while(i < psdoc->shadingcnt) {
		if(psdoc->shadings[i]) {
			_ps_delete_shading(psdoc, psdoc->shadings[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->shadings);

	/* Freeing graphic state resources */
	i = 0;
	while(i < psdoc->gstatecnt) {
		if(psdoc->gstates[i]) {
			_ps_delete_gstate(psdoc, psdoc->gstates[i]);
		}
		i++;
	}
	psdoc->free(psdoc, psdoc->gstates);

	if(psdoc->hdict)
		hnj_hyphen_free(psdoc->hdict);
	if(psdoc->hdictfilename)
		psdoc->free(psdoc, psdoc->hdictfilename);
	psdoc->free(psdoc, psdoc);
}
/* }}} */

/* PS_set_parameter() {{{
 * Sets all kind of parameters. Parameters are string values as opposed
 * to 'values' set by PS_set_value() which are of type float.
 * Some parameters change internal variables while other are just
 * stored in double linked list. If a value itself is a name value pair,
 * then this is called a resource.
 */
PSLIB_API void PSLIB_CALL
PS_set_parameter(PSDoc *psdoc, const char *name, const char *value) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if((strcmp(name, "FontAFM") == 0) ||
		 (strcmp(name, "FontOutline") == 0) ||
		 (strcmp(name, "FontProtusion") == 0) ||
		 (strcmp(name, "FontEncoding") == 0) ||
		 (strcmp(name, "RightMarginKerning") == 0) ||
		 (strcmp(name, "LeftMarginKerning") == 0)) {
		char *res = ps_strdup(psdoc, value);
		char *filename;

		if ((filename = strchr(res, '=')) == NULL) {
			psdoc->free(psdoc, res);
			ps_error(psdoc, PS_Warning, _("Invalid resource"));
			return;
		}
		*filename++ = '\0';
		if (*filename == '=') {
			filename++;
		}
		if(strcmp(name, "RightMarginKerning") == 0) {
			ADOBEINFO *ai;
			if(!psdoc->font || !psdoc->font->metrics) {
				ps_error(psdoc, PS_RuntimeError, _("RightMarginKerning cannot be set without setting a font before."));
				return;
			}
			ai = gfindadobe(psdoc->font->metrics->gadobechars, res);
			if(ai)
				ai->rkern = atoi(filename);
		} else if(strcmp(name, "LeftMarginKerning") == 0) {
			ADOBEINFO *ai;
			if(!psdoc->font || !psdoc->font->metrics) {
				ps_error(psdoc, PS_RuntimeError, _("LeftMarginKerning cannot be set without setting a font before."));
				return;
			}
			ai = gfindadobe(psdoc->font->metrics->gadobechars, res);
			if(ai)
				ai->lkern = atoi(filename);
		} else {
			if(NULL == ps_add_resource(psdoc, name, res, filename, NULL)) {
				ps_error(psdoc, PS_RuntimeError, _("Resource '%s' in category '%s' could not be registered."), res, name);
			}
		}
		psdoc->free(psdoc, res);
	} else if(strcmp(name, "SearchPath") == 0) {
		if(NULL == ps_add_resource(psdoc, name, NULL, value, NULL)) {
			ps_error(psdoc, PS_RuntimeError, _("Resource in category '%s' could not be registered."), name);
		}
	} else if(strcmp(name, "underline") == 0) {
		if(strcmp(value, "true") == 0) {
			psdoc->underline = ps_true;
		} else {
			psdoc->underline = ps_false;
		}
	} else if(strcmp(name, "overline") == 0) {
		if(strcmp(value, "true") == 0) {
			psdoc->overline = ps_true;
		} else {
			psdoc->overline = ps_false;
		}
	} else if(strcmp(name, "strikeout") == 0) {
		if(strcmp(value, "true") == 0) {
			psdoc->strikeout = ps_true;
		} else {
			psdoc->strikeout = ps_false;
		}
	} else if(strcmp(name, "warning") == 0) {
		if(strcmp(value, "true") == 0) {
			psdoc->warnings = ps_true;
		} else {
			psdoc->warnings = ps_false;
		}
	} else if(strcmp(name, "hyphendict") == 0) {
		if((psdoc->hdict != NULL) && strcmp(value, psdoc->hdictfilename)) {
			hnj_hyphen_free(psdoc->hdict);
			psdoc->free(psdoc, psdoc->hdictfilename);
		}
		psdoc->hdict = hnj_hyphen_load(value);
		if(!psdoc->hdict) {
			ps_error(psdoc, PS_RuntimeError, _("Could not load hyphenation table '%s', turning hyphenation off."), value);
			return;
		} 
		if(psdoc->hdictfilename)
			psdoc->free(psdoc, psdoc->hdictfilename);
		psdoc->hdictfilename = ps_strdup(psdoc, value);
	} else if(strcmp(name, "inputencoding") == 0) {
		ENCODING *enc;
		if(NULL != (enc = ps_get_inputencoding(value))) {
			psdoc->inputenc = enc;
		} else {
			ps_error(psdoc, PS_Warning, _("Input encoding '%s' could not be set."), value);
		}
		if(strcmp(value, "true") == 0) {
			psdoc->warnings = ps_true;
		} else {
			psdoc->warnings = ps_false;
		}
	} else {
		PS_PARAMETER *parameter;

		/* Check if parameter already exists. If yes, reset it */
		for(parameter = dlst_first(psdoc->parameters); parameter != NULL; parameter = dlst_next(parameter)) {
			if (0 == strcmp(parameter->name, name)) {
				psdoc->free(psdoc, parameter->value);
				parameter->value = ps_strdup(psdoc, value);
				return;
			}
		}

		/* Add an new parameter */
		if(NULL == (parameter = (PS_PARAMETER *) dlst_newnode(psdoc->parameters, (int) sizeof(PS_PARAMETER)))) {
			return;
		}
		parameter->name = ps_strdup(psdoc, name);
		parameter->value = ps_strdup(psdoc, value);
		dlst_insertafter(psdoc->parameters, parameter, PS_DLST_HEAD(psdoc->parameters));
	}
}
/* }}} */

/* PS_get_parameter() {{{
 * Returns a parameter. This function cannot return resources set by
 * PS_set_parameter().
 */
PSLIB_API const char * PSLIB_CALL
PS_get_parameter(PSDoc *psdoc, const char *name, float modifier) {
	PS_PARAMETER *param;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(NULL);
	}

	if(name == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("Do not know which parameter to get since the passed name is NULL."));
		return(NULL);
	}

	if(strcmp(name, "fontname") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(NULL);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(NULL);
		}
		return(psfont->metrics->fontname);
	} else if(strcmp(name, "fontencoding") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(NULL);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(NULL);
		}
		return(psfont->metrics->codingscheme);
	} else if(strcmp(name, "dottedversion") == 0) {
		return(LIBPS_DOTTED_VERSION);
	} else if(strcmp(name, "scope") == 0) {
		switch(ps_current_scope(psdoc)) {
			case PS_SCOPE_OBJECT:
				return("object");
			case PS_SCOPE_DOCUMENT:
				return("document");
			case PS_SCOPE_NONE:
				return("null");
			case PS_SCOPE_PAGE:
				return("page");
			case PS_SCOPE_PATTERN:
				return("pattern");
			case PS_SCOPE_PATH:
				return("path");
			case PS_SCOPE_TEMPLATE:
				return("template");
			case PS_SCOPE_PROLOG:
				return("prolog");
			case PS_SCOPE_FONT:
				return("font");
			case PS_SCOPE_GLYPH:
				return("glyph");
		}
	} else {
		for(param = dlst_first(psdoc->parameters); param != NULL; param = dlst_next(param)) {
			if (0 == strcmp(param->name, name)) {
				return(param->value);
			}
		}
	}
	return(NULL);
}
/* }}} */

/* PS_set_value() {{{
 * Sets a float value.
 */
PSLIB_API void PSLIB_CALL
PS_set_value(PSDoc *psdoc, const char *name, float value) {
	PS_VALUE *parameter;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	if(strcmp(name, "wordspacing") == 0) {
		ADOBEINFO *ai = NULL;
		if(psdoc->font && psdoc->font->metrics != NULL)
			ai = gfindadobe(psdoc->font->metrics->gadobechars, "space");
		if(ai)
			psdoc->font->wordspace = (int) (value * ai->width);
	} else if(strcmp(name, "textx") == 0) {
		psdoc->tstates[psdoc->tstate].tx = value;
		psdoc->tstates[psdoc->tstate].cx = value;
	} else if(strcmp(name, "texty") == 0) {
		psdoc->tstates[psdoc->tstate].ty = value;
		psdoc->tstates[psdoc->tstate].cy = value;
	} else if(strcmp(name, "textrendering") == 0) {
		psdoc->textrendering = (int) value;
	} else {
		/* Check if value exists */
		for(parameter = dlst_first(psdoc->values); parameter != NULL; parameter = dlst_next(parameter)) {
			if(0 == strcmp(parameter->name, name)) {
				parameter->value = value;
				return;
			}
		}

		/* Doesn't exist, so create a new one */
		if(NULL == (parameter = (PS_VALUE *) dlst_newnode(psdoc->values, (int) sizeof(PS_VALUE)))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for new node in value list."));
			return;
		}
		parameter->name = ps_strdup(psdoc, name);
		parameter->value = value;
		dlst_insertafter(psdoc->values, parameter, PS_DLST_HEAD(psdoc->values));
	}

}
/* }}} */

/* PS_get_value() {{{
 * Returns a value set with PS_set_value()
 * modifier specifies a for which object this value shall be retrieved.
 * objects are fonts, images, ...
 */
PSLIB_API float PSLIB_CALL
PS_get_value(PSDoc *psdoc, const char *name, float modifier) {
	PS_VALUE *value;

	if(name == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("Do not know which value to get since the passed name is NULL."));
		return(0.0);
	}
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0.0);
	}

	if(strcmp(name, "fontsize") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->size);
	} else if(strcmp(name, "font") == 0) {
		return((float) _ps_find_font(psdoc, psdoc->font));
	} else if(strcmp(name, "imagewidth") == 0) {
		PSImage *psimage = _ps_get_image(psdoc, (int) modifier);
		if(!psimage)
			return(0.0);
		return((float) psimage->width);
	} else if(strcmp(name, "imageheight") == 0) {
		PSImage *psimage = _ps_get_image(psdoc, (int) modifier);
		if(!psimage)
			return(0.0);
		return((float) psimage->height);
	} else if(strcmp(name, "capheight") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->capheight);
	} else if(strcmp(name, "ascender") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->ascender);
	} else if(strcmp(name, "descender") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->descender);
	} else if(strcmp(name, "italicangle") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->italicangle);
	} else if(strcmp(name, "underlineposition") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->underlineposition);
	} else if(strcmp(name, "underlinethickness") == 0) {
		PSFont *psfont;
		if(0 == (int) modifier) {
			psfont = psdoc->font;
		} else {
			if(NULL == (psfont = _ps_get_font(psdoc, (int) modifier)))
				return(0.0);
		}
		if(NULL == psfont || NULL == psfont->metrics) {
			ps_error(psdoc, PS_RuntimeError, _("No font set."));
			return(0.0);
		}
		return(psfont->metrics->underlinethickness);
	} else if(strcmp(name, "textx") == 0) {
		return(psdoc->tstates[psdoc->tstate].tx);
	} else if(strcmp(name, "texty") == 0) {
		return(psdoc->tstates[psdoc->tstate].ty);
	} else if(strcmp(name, "textrendering") == 0) {
		return((float) psdoc->textrendering);
	} else if(strcmp(name, "wordspacing") == 0) {
		ADOBEINFO *ai = NULL;
		if(psdoc->font != NULL && psdoc->font->metrics != NULL)
			ai = gfindadobe(psdoc->font->metrics->gadobechars, "space");
		if(ai)
			return((float) psdoc->font->wordspace / ai->width);
		else
			return(0.0);
	} else if(strcmp(name, "major") == 0) {
		return((float) LIBPS_MAJOR_VERSION);
	} else if(strcmp(name, "minor") == 0) {
		return((float) LIBPS_MINOR_VERSION);
	} else if(strcmp(name, "subminor") == 0) {
		return((float) LIBPS_MICRO_VERSION);
	} else if(strcmp(name, "revision") == 0) {
		return((float) LIBPS_MICRO_VERSION);
	} else {
		for(value = dlst_first(psdoc->values); value != NULL; value = dlst_next(value)) {
			if (0 == strcmp(value->name, name)) {
				return(value->value);
			}
		}
	}
	return(0.0);
}
/* }}} */

/* PS_list_values() {{{
 * Outputs a list of all values
 */
PSLIB_API void PSLIB_CALL
PS_list_values(PSDoc *psdoc) {
	PS_VALUE *value;

	if(psdoc == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	printf("List of Values\n-----------------------------------\n");
	for(value = dlst_first(psdoc->values); value != NULL; value = dlst_next(value)) {
		printf("%s = %f\n", value->name, value->value);
	}
	printf("-------------------------------------\n");
}
/* }}} */

/* PS_list_parameters() {{{
 * Outputs a list of all parameters
 */
PSLIB_API void PSLIB_CALL
PS_list_parameters(PSDoc *psdoc) {
	PS_PARAMETER *parameter;

	if(psdoc == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	printf("List of Parameters\n-----------------------------------\n");
	for(parameter = dlst_first(psdoc->parameters); parameter != NULL; parameter = dlst_next(parameter)) {
		printf("%s = %s\n", parameter->name, parameter->value);
	}
	printf("-------------------------------------\n");
}
/* }}} */

/* PS_list_resources() {{{
 * Outputs a list of all resources
 */
PSLIB_API void PSLIB_CALL
PS_list_resources(PSDoc *psdoc) {
	PS_CATEGORY *cat;
	PS_RESOURCE *res;

	if(psdoc == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	printf("List of Resources\n-----------------------------------\n");
	for(cat = dlst_first(psdoc->categories); cat != NULL; cat = dlst_next(cat)) {
		for(res = dlst_first(cat->resources); res != NULL; res = dlst_next(res)) {
			printf("%s : %s = %s\n", cat->name, res->name, res->value);
		}
	}
	printf("-------------------------------------\n");
}
/* }}} */

/* PS_begin_page() {{{
 * starts a new (the next) page
 */
PSLIB_API void PSLIB_CALL
PS_begin_page(PSDoc *psdoc, float width, float height) {
	int sepcolor;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
//	if(psdoc->commentswritten == ps_false) {
	if(psdoc->page == 0) {
		/* Do not overwrite the BoundingBox if it has been set before */
		if(width != 0.0 && height != 0.0) {
			if(psdoc->BoundingBox == NULL) {
				char tmp[30];
#ifdef HAVE_SNPRINTF
				snprintf(tmp, 30, "0 0 %.0f %.0f", width, height);
#else
				sprintf(tmp, "0 0 %.0f %.0f", width, height);
#endif
				psdoc->BoundingBox = ps_strdup(psdoc, tmp);
			}
			if(psdoc->Orientation == NULL) {
				if(width > height)
					psdoc->Orientation = ps_strdup(psdoc, "Landscape");
				else
					psdoc->Orientation = ps_strdup(psdoc, "Portrait");
			}
		}
	}
	/* Make sure the rest of the header is written */
	ps_write_ps_header(psdoc);
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' scope."), __FUNCTION__);
		return;
	}

	(psdoc->page)++;
	ps_printf(psdoc, "\n%%%%Page: %i %i\n", psdoc->page, psdoc->page);
	ps_printf(psdoc, "%%%%PageBoundingBox: 0 0 %d %d\n", (int) width, (int) height);
	ps_printf(psdoc, "%%%%BeginPageSetup\n");
	ps_printf(psdoc, "[ /CropBox [0 0 %.2f %.2f] /PAGE pdfmark\n", width, height);
	sepcolor = (int) PS_get_value(psdoc, "separationcolor", 0.0);
	if(sepcolor > 0) {
		ps_printf(psdoc, "userdict 0 %d bop-hook\n", sepcolor-1);
		ps_printf(psdoc, "PslibDict begin ");
		ps_printf(psdoc, "/vsize %.2f def ", height);
		ps_printf(psdoc, "/hsize %.2f def ", width);
		ps_printf(psdoc, "end\n");
	}
	ps_printf(psdoc, "%%%%EndPageSetup\n");
	ps_printf(psdoc, "save\n");
	ps_printf(psdoc, "0 0 %.2f %.2f ", width, height);
	ps_printf(psdoc, "%i PslibPageBeginHook\n", psdoc->page);
	ps_printf(psdoc, "restore\n");
	ps_printf(psdoc, "save\n");
	psdoc->tstates[psdoc->tstate].tx = 100.0;
	psdoc->tstates[psdoc->tstate].ty = 100.0;
	psdoc->tstates[psdoc->tstate].cx = 100.0;
	psdoc->tstates[psdoc->tstate].cy = 100.0;
	psdoc->page_open = ps_true;
	ps_enter_scope(psdoc, PS_SCOPE_PAGE);
}
/* }}} */

/* PS_end_page() {{{
 * ends the page and increments the pagecount by 1
 */
PSLIB_API void PSLIB_CALL
PS_end_page(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}
	if(psdoc->agstate > 0) {
		ps_error(psdoc, PS_RuntimeError, _("PS_save() has been called more often than PS_restore()."));
		return;
	}
//	ps_printf(psdoc, "end\n");
	ps_printf(psdoc, "restore\n");
	ps_printf(psdoc, "save\n");
	ps_printf(psdoc, "%i PslibPageEndHook\n", psdoc->page);
	ps_printf(psdoc, "restore\n");
	ps_printf(psdoc, "showpage\n");
	psdoc->page_open = ps_false;
	/* Set current font to NULL in order to enforce calling PS_set_font().
	 * PS_set_font() is needed because each page in encapsulated in save/restore
	 * any setting of a font within page will not be valid after the page.
	 */
	psdoc->font = NULL;
	ps_leave_scope(psdoc, PS_SCOPE_PAGE);
}
/* }}} */

/* ps_render_text() {{{
 * Prints text on page depending on current text rendering.
 * The text passed to this function may not containing any
 * kerning pairs, since it relies on the PostScript stringwith
 * function which cannot handle kerning (unless you don't want
 * kerning to be taken into account).
 * The text must be font encoded as it is unmodified written into
 * the PostScript file.
 */
static void ps_render_text(PSDoc *psdoc, const char *text) {
	char *str = (char *) text;
	float textrise;
	if(text == NULL)
		return;
	textrise = PS_get_value(psdoc, "textrise", 0.0);
	if(textrise != 0.0) {
		ps_printf(psdoc, "%f tr ", textrise);
	}
	switch(psdoc->textrendering) {
		case -1: /* normal */
		case 1: /* stroke */
		case 5: /* stroke and clip*/
			ps_setcolor(psdoc, PS_COLORTYPE_STROKE);
			break;
		case 0: /* fill */
		case 2: /* stroke and fill */
			// FIXME: need to set fill and stroke color
		case 4: /* fill and clip*/
		case 6: /* fill, stroke and clip*/
			ps_setcolor(psdoc, PS_COLORTYPE_FILL);
			break;
		default:
			ps_setcolor(psdoc, PS_COLORTYPE_STROKE);
	}
	ps_putc(psdoc, '(');
	while(*str != '\0') {
		unsigned char index = (unsigned char) *str;
		/* Everything below 32, above 127 and '(' and ')' should be output
		 * as octal values.
		 */
		if(index < 32 || index > 127 || index == '(' || index == ')' || index == '\\') {
			ps_printf(psdoc, "\\%03o", index);
		} else {
			ps_putc(psdoc, *str);
		}
		str++;
	}
	ps_putc(psdoc, ')');
	switch(psdoc->textrendering) {
		case -1: /* normal */
			ps_puts(psdoc, "p ");
			break;
		case 0: /* fill */
			ps_puts(psdoc, "qf ");
			break;
		case 1: /* stroke */
			ps_puts(psdoc, "qs ");
			break;
		case 2: /* stroke and fill */
			ps_puts(psdoc, "qsf ");
			break;
		case 3: /* invisible text */
			ps_puts(psdoc, "qi ");
			break;
		case 4: /* fill and clip*/
			ps_puts(psdoc, "qfc ");
			break;
		case 5: /* stroke and clip*/
			ps_puts(psdoc, "qsc ");
			break;
		case 6: /* fill, stroke and clip*/
			// FIXME: need to set fill and stroke color
			ps_puts(psdoc, "qfsc ");
			break;
		case 7: /* clip*/
			ps_puts(psdoc, "qc ");
			break;
		default:
			ps_puts(psdoc, "p ");
			break;
	}
	if(textrise) {
		ps_puts(psdoc, "rt ");
	}
}
/* }}} */

/* PS_show2() {{{
 * Output text at current position. Do not print more the xlen characters.
 */
PSLIB_API void PSLIB_CALL
PS_show2(PSDoc *psdoc, const char *text, int xlen) {
	int kernonoff;
	int ligonoff;
	char ligdischar;
	float charspacing;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(NULL == text) {
		ps_error(psdoc, PS_RuntimeError, _("Text to display is NULL."));
		return;
	}

	if(NULL == psdoc->font) {
		ps_error(psdoc, PS_RuntimeError, _("No font set."));
		return;
	}

	/* Set starting point for text */
	ps_printf(psdoc, "%.2f %.2f a\n", psdoc->tstates[psdoc->tstate].tx, psdoc->tstates[psdoc->tstate].ty);

	charspacing = PS_get_value(psdoc, "charspacing", 0) * 1000.0 / psdoc->font->size;
	kernonoff = ps_get_bool_parameter(psdoc, "kerning", 1);
	ligonoff = ps_get_bool_parameter(psdoc, "ligatures", 1);
	if(ligonoff) {
		const char *tmp = PS_get_parameter(psdoc, "ligaturedisolvechar", 0.0);
		if(tmp && tmp[0])
			ligdischar = tmp[0];
		else
			ligdischar = '¦';
	}

	/* Take kerning into account if adobe font metrics has been loaded. */
	if(psdoc->font->metrics) {
		int i, len, k=0;
		float x, y, yy;
		ADOBEINFO *prevai = NULL;
		char *strbuf;
		char *textcpy;
		float overallwidth = 0;
		float ascender = 0;
		float descender = 0;

		textcpy = ps_strdup(psdoc, text);
		len = strlen(text);
		if(xlen != 0)
			len = xlen < len ? xlen : len;
		strbuf = (char *) psdoc->malloc(psdoc, len+1, _("Allocate temporay space for output string."));
		for(i=0; i<len; i++) {
			unsigned char c;
			char *adobename;
			float kern;

			c = (unsigned char) textcpy[i];
			adobename = ps_inputenc_name(psdoc, c);
			if(adobename && adobename[0] != '\0') {
				ADOBEINFO *ai;
				ai = gfindadobe(psdoc->font->metrics->gadobechars, adobename);
				if(ai) {
					/* Check if space has ended a string */
					if(strcmp(adobename, "space") == 0) {
						if((kernonoff == 1) && (NULL != prevai))
							kern = (float) calculatekern(prevai, ai);
						else
							kern = 0.0;
						overallwidth += (float) psdoc->font->wordspace + charspacing + kern;
						/* first output collected text */
						if(k > 0) {
							strbuf[k] = '\0';
							ps_render_text(psdoc, strbuf);
							k = 0;
						}
						ps_printf(psdoc, "%.2f w ", (psdoc->font->wordspace+charspacing+kern)*psdoc->font->size/1000.0+0.005);
					} else {
						char *newadobename;
						int offset = 0;
						/* Check if the current and the next character form a ligature */
						if(ligonoff == 1 &&
						   charspacing == 0.0 &&
						   ps_check_for_lig(psdoc, psdoc->font->metrics, ai, &textcpy[i+1], ligdischar, &newadobename, &offset)) {
							if(ps_fontenc_has_glyph(psdoc, psdoc->font->metrics->fontenc, newadobename)) {
								ADOBEINFO *nai = gfindadobe(psdoc->font->metrics->gadobechars, newadobename);
								if(nai) {
									ai = nai;
									i += offset;
								} else {
									ps_error(psdoc, PS_Warning, _("Font '%s' has no ligature '%s', disolving it."), psdoc->font->metrics->fontname, newadobename);
								}
							} else {
								ps_error(psdoc, PS_Warning, _("Font encoding vector of font '%s' has no ligature '%s', disolving it."), psdoc->font->metrics->fontname, newadobename);
							}
						}
						/* At this point either ai is ligature or the current char */
						overallwidth += (float) (ai->width);
						descender = min((float) ai->lly, descender);
						ascender = max((float) ai->ury, ascender);
						if((kernonoff == 1) && (NULL != prevai)) {
							kern = (float) calculatekern(prevai, ai);
							overallwidth += kern;
						} else {
							kern = 0.0;
						}
						if(i < (len-1))
							overallwidth += charspacing;
	//					printf("kern = %f\n", kern);
						/* Any space between last and current character? If Yes output the
						 * collected text first, put the space after it and start a new
						 * string up to the next space (space is either kerning or extra
						 * charspace).
						 */
						if((kern != 0.0 || charspacing != 0.0) && (i > 0)) {
							if(k > 0) {
								strbuf[k] = '\0';
								ps_render_text(psdoc, strbuf);
								k = 0;
							}
							ps_printf(psdoc, "%.2f w ", (kern+charspacing)*psdoc->font->size/1000.0+0.005);
						}
						if(psdoc->font->metrics->fontenc)
							strbuf[k++] = ps_fontenc_code(psdoc, psdoc->font->metrics->fontenc, ai->adobename);
						else
							strbuf[k++] = ai->adobenum;
					}
				} else { /* glyph not found */
					ps_error(psdoc, PS_Warning, _("Glyph '%s' not found in metric file."), adobename);
				}
				prevai = ai;
			} else {
				ps_error(psdoc, PS_Warning, _("Character %d not in input encoding vector."), c);
			}
		}
		psdoc->free(psdoc, textcpy);
		/* Output rest of line if there is some left */
		if(k > 0) {
			strbuf[k] = '\0';
			ps_render_text(psdoc, strbuf);
			k = 0;
		}
		psdoc->free(psdoc, strbuf);
		ps_printf(psdoc, "\n");
		x = psdoc->tstates[psdoc->tstate].tx;
		yy = psdoc->tstates[psdoc->tstate].ty;
		psdoc->tstates[psdoc->tstate].tx += overallwidth*psdoc->font->size/1000.0;
		if(psdoc->underline == ps_true) {
//			y = yy + (psdoc->font->metrics->descender-2*psdoc->font->metrics->underlinethickness)*psdoc->font->size/1000.0;
			y = yy + (descender-2*psdoc->font->metrics->underlinethickness)*psdoc->font->size/1000.0;
			PS_save(psdoc);
			PS_setdash(psdoc, 0, 0);
			PS_setlinewidth(psdoc, psdoc->font->metrics->underlinethickness*psdoc->font->size/1000.0);
			PS_moveto(psdoc, x, y);
			PS_lineto(psdoc, x + overallwidth*psdoc->font->size/1000.0, y);
			PS_stroke(psdoc);
			PS_restore(psdoc);
		}
		if(psdoc->overline == ps_true) {
			y = yy + (psdoc->font->metrics->ascender+2*psdoc->font->metrics->underlinethickness)*psdoc->font->size/1000.0;
			PS_save(psdoc);
			PS_setdash(psdoc, 0, 0);
			PS_setlinewidth(psdoc, psdoc->font->metrics->underlinethickness*psdoc->font->size/1000.0);
			PS_moveto(psdoc, x, y);
			PS_lineto(psdoc, x + overallwidth*psdoc->font->size/1000.0, y);
			PS_stroke(psdoc);
			PS_restore(psdoc);
		}
		if(psdoc->strikeout == ps_true) {
			y = yy + (psdoc->font->metrics->ascender)/2*psdoc->font->size/1000.0;
			PS_save(psdoc);
			PS_setdash(psdoc, 0, 0);
			PS_setlinewidth(psdoc, psdoc->font->metrics->underlinethickness*psdoc->font->size/1000.0);
			PS_moveto(psdoc, x, y);
			PS_lineto(psdoc, x + overallwidth*psdoc->font->size/1000.0, y);
			PS_stroke(psdoc);
			PS_restore(psdoc);
		}
	} else {
		/* FIXME: ps_render_text() expects the text in fontenc */
		ps_render_text(psdoc, text);
	}
}
/* }}} */

/* PS_show() {{{
 * Output null terminated string
 */
PSLIB_API void PSLIB_CALL
PS_show(PSDoc *psdoc, const char *text) {
	PS_show2(psdoc, text, 0);
}
/* }}} */

/* PS_continue_text() {{{
 * Output text one line after the last line outputed. The line spacing
 * is taken from the value 'leading'.
 */
PSLIB_API void PSLIB_CALL
PS_continue_text(PSDoc *psdoc, const char *text) {
	PS_continue_text2(psdoc, text, 0);
}
/* }}} */

/* PS_continue_text2() {{{
 * Output text one line after the last line outputed. The line spacing
 * is taken from the value 'leading'. Do not output more than len characters.
 */
PSLIB_API void PSLIB_CALL
PS_continue_text2(PSDoc *psdoc, const char *text, int len) {
	int y, x;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	/* Save the current text position and set it to the text position
	 * for continued text.
	 */
	y = psdoc->tstates[psdoc->tstate].ty;
	x = psdoc->tstates[psdoc->tstate].tx;
	psdoc->tstates[psdoc->tstate].cy -= PS_get_value(psdoc, "leading", 0.0);
	psdoc->tstates[psdoc->tstate].ty = psdoc->tstates[psdoc->tstate].cy;
	psdoc->tstates[psdoc->tstate].tx = psdoc->tstates[psdoc->tstate].cx;
	PS_show2(psdoc, text, len);
	/* Restore the old text position */
	psdoc->tstates[psdoc->tstate].ty = y;
	psdoc->tstates[psdoc->tstate].tx = x;
}
/* }}} */

//#define MAX_CHARS_IN_LINE 1024

/* PS_show_boxed() {{{
 * Outputs text in a box with given dimensions. The text is justified
 * as specified in hmode. This function uses several parameters and values
 * to format the output. The return value is the number of characters that
 * could not be written.
 */
PSLIB_API int PSLIB_CALL
PS_show_boxed(PSDoc *psdoc, const char *text, float left, float bottom, float width, float height, const char *hmode, const char *feature) {
	char *str = NULL;
	char *textcopy = NULL;
	char *linebuf = NULL;
	char prevchar = '\0';
	int curpos, lastbreak, lastdelim, firstword, spaces, morechars, mode;
	int lineend, parend, boxlinecounter, parlinecounter, *linecounter;
	int parcounter, parindentskip;
	int numindentlines = 0;
	int doindent;
	const char *linenumbermode;
	float hlen, vlen, xpos, ypos, oldypos, leading, old_word_spacing;
	float oldhlen = 0.0;
	float parindent, parskip, linewidth, linenumberspace;
	float linenumbersep = 5.0;
	int hyphenationonoff;
	int treatcrasspace = 0;
	int treatcraspar = 1;
	int hyphenminchars = 3;
	int blind = 0;
	int fontid;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return 0;
	}

	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return 0;
	}

	if(NULL == text)
		return 0;

	if(NULL == hmode || '\0' == *hmode) {
		ps_error(psdoc, PS_RuntimeError, _("Must specify a horizontal mode for PS_show_boxed()."));
		return 0;
	}

	if(0 == strcmp(hmode, "fulljustify")) {
		mode = PS_TEXT_HMODE_FULLJUSTIFY;
	} else if(0 == strcmp(hmode, "justify")) {
		mode = PS_TEXT_HMODE_JUSTIFY;
	} else if(0 == strcmp(hmode, "right")) {
		mode = PS_TEXT_HMODE_RIGHT;
	} else if(0 == strcmp(hmode, "center")) {
		mode = PS_TEXT_HMODE_CENTER;
	} else if(0 == strcmp(hmode, "left")) {
		mode = PS_TEXT_HMODE_LEFT;
	} else {
		ps_error(psdoc, PS_Warning, _("There is no such horizontal mode like '%s'. Using 'left' instead."), hmode);
		mode = PS_TEXT_HMODE_LEFT;
	}

	if(feature != NULL && 0 == strcmp(feature, "blind")) {
		blind = 1;
	}

	if(NULL == psdoc->font) {
		ps_error(psdoc, PS_RuntimeError, _("Cannot output text before setting a font."));
		return 0;
	}

	if(NULL == psdoc->font->metrics) {
		ps_error(psdoc, PS_RuntimeError, _("No font metrics loaded."));
		return 0;
	}
	if(!(fontid = _ps_find_font(psdoc, psdoc->font))) {
		ps_error(psdoc, PS_RuntimeError, _("Could not find font resource."));
		return 0;
	}

	hyphenationonoff = ps_get_bool_parameter(psdoc, "hyphenation", 0);
	if(hyphenationonoff) {
		if(!psdoc->hdict) {
			ps_error(psdoc, PS_Warning, _("No hyphenation table set, turning hyphenation off."));
			hyphenationonoff = ps_false;
		} else {
			if(0 == (hyphenminchars = (int) PS_get_value(psdoc, "hyphenminchars", 0)))
				hyphenminchars = 3;
		}
	}

	/* If CR is treated as line break, it may not be treated as
	 * parbreak as well.
	 */
	treatcrasspace = !ps_get_bool_parameter(psdoc, "linebreak", 0);
	if(treatcrasspace)
		treatcraspar = ps_get_bool_parameter(psdoc, "parbreak", 1);
	else
		treatcraspar = 0;

	/* Calculate baseline of first line in box */
	ypos = bottom + height - psdoc->font->metrics->ascender*psdoc->font->size/1000.0;
	textcopy = ps_strdup(psdoc, text);
	str = textcopy;
	curpos = 0;
	lastbreak = 0;
	lastdelim = 0;
	vlen = 0;
	firstword = 1;
	boxlinecounter = 0;
	parlinecounter = 0;
	parcounter = 0;
	if((leading = PS_get_value(psdoc, "leading", 0)) <= 0.0)
		leading = (psdoc->font->metrics->ascender - psdoc->font->metrics->descender)*psdoc->font->size*1.2/1000.0;
	if((parindent = PS_get_value(psdoc, "parindent", 0)) <= 0.0)
		parindent = 0.0;
	else {
		if((numindentlines = (int) PS_get_value(psdoc, "numindentlines", 0.0)) <= 0)
			numindentlines = 1;
	}
	if((parskip = PS_get_value(psdoc, "parskip", 0)) <= 0.0)
		parskip = 0.0;
	if((parindentskip = (int) PS_get_value(psdoc, "parindentskip", 0.0)) <= 0)
		parindentskip = 0;
	linecounter = NULL;
	if((linenumbermode = PS_get_parameter(psdoc, "linenumbermode", 0.0)) != NULL) {
		if(0 == strcmp(linenumbermode, "paragraph"))
			linecounter = &parlinecounter;
		else if(0 == strcmp(linenumbermode, "box"))
			linecounter = &boxlinecounter;
		else
			ps_error(psdoc, PS_Warning, _("Unknown line number mode '%s'. Turning line numbering off."), linenumbermode);
		if(linecounter) {
			if((linenumberspace = PS_get_value(psdoc, "linenumberspace", 0.0)) <= 0.0)
				linenumberspace = 20;
			if((linenumbersep = PS_get_value(psdoc, "linenumbersep", 0.0)) <= 0.0)
				linenumbersep = 5;
			width -= (linenumberspace+linenumbersep);
			left += (linenumberspace+linenumbersep);

		}
	}
	old_word_spacing = ps_get_word_spacing(psdoc, psdoc->font);
	oldypos = ypos;
	/* Output text until all shown or the box is full */
	while(*str != '\0' && (ypos >= bottom || height == 0.0)) {
		/* reset length of line */
		hlen = 0.0;
		/* Set normal word spacing for the current font */
		ps_set_word_spacing(psdoc, psdoc->font, 0.0);
		spaces = 0;
		firstword = 1;
		lineend = ps_false;
		parend = ps_false;
		linewidth = width;
		/* Indent only the first n lines of paragraph, but only starting
		 * with th m'th paragraph
		 */
		doindent = parlinecounter < numindentlines && parcounter >= parindentskip;
		if(doindent) {
			linewidth -= parindent;
		}
		/* Collect text until line is full, line has been ended or no more chars */
		while(hlen < linewidth && *str != '\0' && lineend == ps_false && parend == ps_false) {
			/* Search for next word boundry (delimiter) */
			while(*str != ' ' && *str != '-' && *str != '\n' && *str != '\r' && *str != '­' && *str != '\0') {
				prevchar = *str;
				str++;
				curpos++;
			}

			/* Treat '\n' and '\r' as space */
			if(*str == '\n' || *str == '\r') {
				if(!treatcrasspace && *str == '\n') {
					lineend = ps_true;
				}
				if(treatcraspar && prevchar == '\n' && *str == '\n') {
					parend = ps_true;
				}
			} else if(*str == '\t') {
				*str = ' ';
			}

			/* a space at the end of the line has to be skipped, but a '-' or hyphen
			 * must be displayed and taken into account for stringwidth.
			 * It could be that there are other words in this line and the
			 * current char will be the last in the line. For that reason morechars
			 * is set 0 for each new word boundry which isn't a '-' or a hyphen. */
			if(parend) {
				morechars = -1;
			} else if(*str == '-') {
				morechars = 1;
			} else {
				morechars = 0;
			}
			/* Calculate the width of the complete line from the last break to
			 * the current position. Save the old len before, just in case
			 * the line is too long and we need the old value.
			 */
			oldhlen = hlen;
			hlen = PS_stringwidth2(psdoc, &textcopy[lastbreak], curpos-lastbreak+morechars, fontid, psdoc->font->size);
			//printf("'%s' (%d) ist %f lang\n", &textcopy[lastbreak], curpos-lastbreak+morechars, hlen);
			/* Is the line length still smaller or is this the first word which
			 * always is displayed even if it ecxeeds the line width.
			 */
			if(hlen < linewidth || firstword == 1) {
//				printf("%f ist noch kleiner als %f\n", hlen, width);
				lastdelim = curpos;
				prevchar = *str;

				/* CR in the middle of the line will be made to spaces */
				if(*str == '\n')
					*str = ' ';

				/* Count space for later calculation of spacing between words. Will
				 * be corrected if this is the last space of a line.
				 */
				if(*str == ' ')
					spaces++;

				/* We have passed the first word */
				firstword = 0;

				/* If you are not at the end of the string then move to
				 * the next char and skip the delimiter. We need to do
				 * that only if the line isn't full yet, otherwise
				 * we will drop out of the while loop anyway. */
				if(*str != '\0') {
					str++;
					curpos++;
				}
			}
		}
		/* We have dropped the while loop because the text ended before the
		 * line was full, the line was ended by cr or the line is overfilled.
		 * curpos and str now point to the next delimiter. The line does not
		 * contain cr anymore. They has been replaced by spaces. */

		/* Do not take the last space in a line into account */
		if(textcopy[lastdelim] == ' ')
			spaces--;

		/* morechars may not have the right value, because it has been set for
		 * the last character read, which may not be the character at index
		 * lastdelim. This happens if a word does not fit into the line anymore.
		 */
		if(parend) {
			morechars = -1;
		} else if(textcopy[lastdelim] != ' ' && textcopy[lastdelim] != '\0') {
			morechars = 1;
		} else {
			morechars = 0;
		}
//		printf("lastdelim-1 = '%c', curpos-1 = '%c', morechars = %d\n", textcopy[lastdelim-1], textcopy[curpos-1], morechars);
//		printf("output line '%s' (%d) has %d spaces\n", &textcopy[lastbreak], lastdelim-lastbreak+morechars, spaces);
//
		/* The word between lastdelim and curpos should now be hyphenated.
		 * No need for hyphenation if there is no more text or the line
		 * has been ended for some reason (e.g. a cr which was not treated
		 * as space).
		 */
		if(!lineend && !parend && textcopy[lastdelim] != '\0' && hyphenationonoff) {
			char *hyphenword = (char *) ps_calloc(psdoc, (curpos-lastdelim+3)*sizeof(char), _("Could not allocated memory for hyphenated word."));
//			printf("Remaining space in line is %.2f\n", linewidth-oldhlen);
			if(NULL != hyphenword) {
				int i, k, lasthp;
				float lenhp = 0.0;

				/* Just consider the last word in the line. It starts with
				 * the last delimiter which can be ' ' or '-' or a hyphen and
				 * ends it the end of the word. Since curpos points to the next
				 * delimiter we are set.
				 */
				/* Same situation as with morechars before. If the delimiter is
				 * not a space or '\0' we have to take it into account.
				 * Actually it doesn't matter if we take a hyphen into account
				 * or not, but maybe there is a difference between 'word' and
				 * 'word-' if it is hyphenated.
				 */
				if(textcopy[curpos] != ' ' && textcopy[curpos] != '\n' && textcopy[curpos] != '\0') {
					strncpy(hyphenword, &textcopy[lastdelim+1], curpos-lastdelim-morechars+1);
				} else {
					strncpy(hyphenword, &textcopy[lastdelim+1], curpos-lastdelim-morechars);
				}

				lasthp = 0;
				/* hyphenate only words with at least 2*hyphenminchars chars
				 * All chars at the beginning of the word which are not alpha
				 * will be counted and later taken into account.
				 */
				k = 0;
				while(hyphenword[k] && !isalpha(hyphenword[k]))
					k++;
				if((strlen(hyphenword)-k) > 2*hyphenminchars) {
					char *buffer;
					char buf1[100];
					int l;
					/* buf1 will be the partial hyphenated word including all not
					 * alpha chars at the beginning of the word. This string will
					 * be used to calculate the len of the line.
					 */
					buffer = (char*) psdoc->malloc(psdoc, sizeof(char) * (strlen(hyphenword)+3), _("Could not allocate memory for hyphenation buffer."));
					hnj_hyphen_hyphenate(psdoc->hdict, &hyphenword[k], strlen(&hyphenword[k]), buffer);
//					printf("Word to hyphenate: '%s'\n", &hyphenword[k]);
//					printf("                    %s\n", buffer);
					buf1[0] = textcopy[lastdelim];
					k++;
					for(l=1; l<k; l++) {
						buf1[l] = textcopy[lastdelim+1];
					}
					for(i=hyphenminchars-1, hlen=0; (i < strlen(&hyphenword[k-1])-hyphenminchars) && (hlen < (linewidth-oldhlen)); i++) {
	//					printf("%c", hyphenword[i]);
						if(buffer[i] & 1) {
							strncpy(&buf1[k], &hyphenword[k-1], i+1);
							buf1[i+k+1] = '­';
							buf1[i+k+2] = '\0';
//							printf("buf1 = %s\n", buf1);
							hlen = PS_stringwidth2(psdoc, buf1, -1, fontid, psdoc->font->size);
							if(hlen < (linewidth-oldhlen)) {
								lasthp = i+k;
								lenhp = hlen;
							}
//							printf("Len of %s (%d) is %.2f (%.2f)\n", buf1, i+1, hlen, linewidth-oldhlen);
	//						printf("-");
						}
					}
//					printf("Hyphenation at char %d\n", lasthp+1);
					if(lasthp > 0) {
						if(textcopy[lastdelim] == ' ') {
							spaces++;
						}
						oldhlen += lenhp;
						lastdelim += lasthp;
						morechars = 1;
					}
					psdoc->free(psdoc, buffer);
				}
				/* At this point we know a possible hyphenation postition. The
				 * next step would be to take the line concat it with the hyphenated
				 * word, determine its length, correct curpos, lastdelim, oldhlen
				 * and str
				 */

				psdoc->free(psdoc, hyphenword);

				if(NULL != (linebuf = psdoc->malloc(psdoc, lastdelim-lastbreak+morechars+2, _("Could not allocate memory for line buffer.")))) {
					strncpy(linebuf, &textcopy[lastbreak], lastdelim-lastbreak+morechars);
					if(lasthp > 0)
						linebuf[lastdelim-lastbreak+morechars] = '­';
					else
						linebuf[lastdelim-lastbreak+morechars] = '\0';
					linebuf[lastdelim-lastbreak+morechars+1] = '\0';
				} else {
					return 0;
				}
			}
		} else {
			if(NULL != (linebuf = psdoc->malloc(psdoc, lastdelim-lastbreak+morechars+1, _("Could not allocate memory for line buffer.")))) {
				strncpy(linebuf, &textcopy[lastbreak], lastdelim-lastbreak+morechars);
				linebuf[lastdelim-lastbreak+morechars] = '\0';
			} else {
				return 0;
			}
		}

//		printf("Complete line: '%s'\n", linebuf);
//		printf("Line has %d spaces\n", spaces);
//		printf("Line is %f (%f) long\n\n", PS_stringwidth2(psdoc, linebuf, strlen(linebuf), psdoc->font, psdoc->font->size), oldhlen);

		if(!blind) {
			/* Recalculate the length of the line, althought oldhlen should contain
			 * the proper length already, it differs from the len calculated by
			 * the following line FIXME: must be investigated
			 */
			oldhlen = PS_stringwidth2(psdoc, linebuf, -1, fontid, psdoc->font->size);

			xpos = 0.0;
			switch(mode) {
				case PS_TEXT_HMODE_FULLJUSTIFY:
				case PS_TEXT_HMODE_JUSTIFY: {
					/* If the paragraph was ended e.g. by a cr, we will not care about
					 * PS_TEXT_HMODE_JUSTIFY. */
					if(parend) {
						xpos = left;
					} else {
						float extraleftspace = 0.0;
						float extrarightspace = 0.0;
						if(spaces != 0 && *str != '\0' && textcopy[lastdelim] != '\n') {
							ADOBEINFO *ai;
							unsigned char singlechar;

							/* check if the last char of the line may protrude into the right
							 * margin. */
							singlechar = (unsigned char) linebuf[strlen(linebuf)-1];
		//					printf("Last char of line is %c\n", singlechar);
							if(NULL != (ai = gfindadobe(psdoc->font->metrics->gadobechars, ps_inputenc_name(psdoc, singlechar)))) {
			//					printf("RightMarginKerning for '%s' is %d\n",  ps_inputenc_name(psdoc, singlechar), ai->rkern);
								extrarightspace = ai->width*psdoc->font->size*ai->rkern/1000000.0;
							}

							/* check if the first char of the line may protrude into the left
							 * margin. */
							singlechar = (unsigned char) linebuf[0];
		//					printf("First char of line is %c\n", singlechar);
							if(NULL != (ai = gfindadobe(psdoc->font->metrics->gadobechars, ps_inputenc_name(psdoc, singlechar)))) {
			//					printf("RightMarginKerning for '%s' is %d\n",  ps_inputenc_name(psdoc, singlechar), ai->rkern);
								extraleftspace = ai->width*psdoc->font->size*ai->lkern/1000000.0;
							}
		//					printf("Set word spacing to %f\n", (linewidth-oldhlen)/spaces);
							if((mode == PS_TEXT_HMODE_FULLJUSTIFY) || (ypos-leading >= bottom))
								ps_add_word_spacing(psdoc, psdoc->font, (linewidth+extraleftspace+extrarightspace-oldhlen)/(float)spaces);
						}
						/* set oldhlen to ensure texty is set properly */
						oldhlen = linewidth;
						xpos = left-extraleftspace;
					}
					if(doindent) {
						xpos += parindent;
					}
					break;
				}
				case PS_TEXT_HMODE_LEFT:
					/* FIXME: consider left margin kerning, see extraleftspace
					 * as used above */
					xpos = left;
					if(doindent) {
						xpos += parindent;
					}
					break;
				case PS_TEXT_HMODE_RIGHT:
					xpos = left + (linewidth-oldhlen);
					break;
				case PS_TEXT_HMODE_CENTER:
					xpos = left + (linewidth-oldhlen)/2;
					break;
			}

			PS_show_xy2(psdoc, linebuf, strlen(linebuf), xpos, ypos);
			if(linecounter) {
				char linenumstr[11];
#ifdef HAVE_SNPRINTF
				snprintf(linenumstr, 10, "%d", *linecounter+1);
#else
				sprintf(linenumstr, "%d", *linecounter+1);
#endif
				PS_show_xy2(psdoc, linenumstr, strlen(linenumstr), left-linenumbersep-PS_stringwidth2(psdoc, linenumstr, -1, fontid, psdoc->font->size), ypos);
			}
		}
		psdoc->free(psdoc, linebuf);

//		printf("lastdelim = %c (%d), lastbreak = %d\n", textcopy[lastdelim], lastdelim, lastbreak);
		/* Set the string position to 1 char after the last delimiter */
		if(textcopy[lastdelim] != '\0')
			curpos = lastdelim+1;
		str = &textcopy[curpos];
		lastbreak = lastdelim+1;
		/* forward one line by leading */
		oldypos = ypos;
		ypos = ypos - leading;
		boxlinecounter++;
		if(parend) {
			ypos -= parskip;
			parlinecounter = 0;
			parcounter++;
		} else {
			parlinecounter++;
		}
	}
	psdoc->free(psdoc, textcopy);

	/* Set new text position. textx may not be set, because it has been set by
	 * PS_show_xy2() already.
	 */
	if(!blind)
		psdoc->tstates[psdoc->tstate].ty = oldypos;
	PS_set_value(psdoc, "boxheight", (bottom + height) - (oldypos - leading + psdoc->font->metrics->ascender*psdoc->font->size/1000.0));
//	printf("Reset word spacing to %f\n", old_word_spacing);
	ps_set_word_spacing(psdoc, psdoc->font, old_word_spacing);
	if(text[curpos] == '\0')
		return 0;
	else
		return(ps_strlen(text) - curpos);
}
/* }}} */

/* PS_moveto() {{{
 * Moves the current position (cursor) to the passed position
 * This function starts a new path if it is called in page-scope,
 * otherwise it will set the current point and continues with the
 * current path.
 */
PSLIB_API void PSLIB_CALL
PS_moveto(PSDoc *psdoc, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH | PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN | PS_SCOPE_GLYPH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path', 'template', 'pattern', 'glyph' or 'page' scope."), __FUNCTION__);
		return;
	}
	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	/* Only set the scope to PS_SCOPE_PATH if we are not already in that
	 * scope.
	 */
	if(ps_current_scope(psdoc) != PS_SCOPE_PATH) {
		ps_enter_scope(psdoc, PS_SCOPE_PATH);
		ps_printf(psdoc, "newpath\n");
	}
	ps_printf(psdoc, "%.2f %.2f a\n", x, y);
}
/* }}} */

/* PS_show_xy() {{{
 * Calls PS_moveto and PS_show to print the text on the selected position
 */
PSLIB_API void PSLIB_CALL
PS_show_xy(PSDoc *psdoc, const char *text, float x, float y) {
	PS_show_xy2(psdoc, text, 0, x, y);
}
/* }}} */

/* PS_show_xy2() {{{
 * Calls PS_moveto and PS_show2 to print the text on the selected position
 */
PSLIB_API void PSLIB_CALL
PS_show_xy2(PSDoc *psdoc, const char *text, int xlen, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	psdoc->tstates[psdoc->tstate].tx = x;
	psdoc->tstates[psdoc->tstate].cx = x;
	psdoc->tstates[psdoc->tstate].ty = y;
	psdoc->tstates[psdoc->tstate].cy = y;
	PS_show2(psdoc, text, xlen);
}
/* }}} */

/* PS_lineto() {{{
 * Draws a line from the current point to the passed point
 */
PSLIB_API void PSLIB_CALL
PS_lineto(PSDoc *psdoc, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	ps_printf(psdoc, "%.2f %.2f l\n", x, y);
}
/* }}} */

/* PS_set_text_pos() {{{
 * Set the coordinates for the next text output.
 */
PSLIB_API void PSLIB_CALL
PS_set_text_pos(PSDoc *psdoc, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	psdoc->tstates[psdoc->tstate].tx = x;
	psdoc->tstates[psdoc->tstate].ty = y;
	psdoc->tstates[psdoc->tstate].cx = x;
	psdoc->tstates[psdoc->tstate].cy = y;
}
/* }}} */

/* PS_setlinewidth() {{{
 * Set the width of a line for all drawing operations.
 */
PSLIB_API void PSLIB_CALL
PS_setlinewidth(PSDoc *p, float width) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN | PS_SCOPE_GLYPH)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'template', 'glyph', or 'pattern' scope."), __FUNCTION__);
		return;
	}
	ps_printf(p, "%f setlinewidth\n", width);
}
/* }}} */

/* PS_setlinecap() {{{
 * Sets the appearance of the line ends.
 */
PSLIB_API void PSLIB_CALL
PS_setlinecap(PSDoc *p, int type) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if((type > PS_LINECAP_SQUARED) || (type < PS_LINECAP_BUTT)) {
		ps_error(p, PS_Warning, _("Type of linecap is out of range."));
		return;
	}
	ps_printf(p, "%d setlinecap\n", type);
}
/* }}} */

/* PS_setlinejoin() {{{
 * Sets how lines are joined in a polygon.
 */
PSLIB_API void PSLIB_CALL
PS_setlinejoin(PSDoc *p, int type) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if((type > PS_LINEJOIN_BEVEL) || (type < PS_LINEJOIN_MITER)) {
		ps_error(p, PS_Warning, _("Type of linejoin is out of range."));
		return;
	}
	ps_printf(p, "%d setlinejoin\n", type);
}
/* }}} */

/* PS_setmiterlimit() {{{
 */
PSLIB_API void PSLIB_CALL
PS_setmiterlimit(PSDoc *p, float value) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(value < 1) {
		ps_error(p, PS_Warning, _("Miter limit is less than 1."));
		return;
	}
	ps_printf(p, "%f setmiterlimit\n", value);
}
/* }}} */

/* PS_setflat() {{{
 */
PSLIB_API void PSLIB_CALL
PS_setflat(PSDoc *p, float value) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(value < 0.2 || value > 100.0) {
		ps_error(p, PS_Warning, _("Flat value is less than 0.2 or bigger than 100.0"));
		return;
	}
	ps_printf(p, "%f setflat\n", value);
}
/* }}} */

/* PS_setdash() {{{
 * Sets the length of the black and white portions of a dashed line.
 */
PSLIB_API void PSLIB_CALL
PS_setdash(PSDoc *p, float on, float off) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(on == 0.0 && off == 0.0) {
		ps_printf(p, "[] 0 setdash\n");
	} else {
		ps_printf(p, "[%f %f] 0 setdash\n", on, off);
	}
}
/* }}} */

/* PS_setpolydash() {{{
 * Sets a more complicated dash line with. arr is list of black and white
 * portions.
 */
PSLIB_API void PSLIB_CALL
PS_setpolydash(PSDoc *p, float *arr, int length) {
	float *dash;
	int i;

	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(NULL == arr) {
		ps_error(p, PS_RuntimeError, _("Array for dashes is NULL."));
		return;
	}

	ps_printf(p, "[");
	dash = arr;
	for(i=0; i<length; i++, dash++) {
		ps_printf(p, "%f ", *dash);
	}
	ps_printf(p, "] 0 setdash\n");
}
/* }}} */

/* PS_setmatrix() {{{
 * FIXME: Not implemented
 */
PSLIB_API void PSLIB_CALL
PS_setmatrix(PSDoc *p, float a, float b, float c, float d, float e, float f) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
}
/* }}} */

/* PS_setoverprintmode() {{{
 * Sets overprint flag true or false.
 */
PSLIB_API void PSLIB_CALL
PS_setoverprintmode(PSDoc *p, int mode) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if((mode > 1) || (mode < 0)) {
		ps_error(p, PS_Warning, _("Mode for overprint must be either 0 or 1."));
		return;
	}
	ps_printf(p, "%s setoverprint\n", mode == 0 ? "false" : "true");
}
/* }}} */

/* PS_setsmoothness() {{{
 * Sets smoothness.
 */
PSLIB_API void PSLIB_CALL
PS_setsmoothness(PSDoc *p, float smoothness) {
	if(NULL == p) {
		ps_error(p, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(p, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(p, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if((smoothness > 1) || (smoothness < 0)) {
		ps_error(p, PS_Warning, _("Smoothness value must be between 0 and 1."));
		return;
	}
	ps_printf(p, "%.4f setsmoothness\n", smoothness);
}
/* }}} */

/* PS_rect() {{{
 * Draws a rectangle. The rectancle is either added to the current path
 * or starts a new one.
 */
PSLIB_API void PSLIB_CALL
PS_rect(PSDoc *psdoc, float x, float y, float width, float height) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH | PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN | PS_SCOPE_GLYPH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path', 'template', 'pattern', 'glyph' or 'page' scope."), __FUNCTION__);
		return;
	}
	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	if(ps_current_scope(psdoc) != PS_SCOPE_PATH) {
		ps_enter_scope(psdoc, PS_SCOPE_PATH);
		ps_printf(psdoc, "newpath\n");
	}
	ps_printf(psdoc, "%.4f %.4f a\n", x, y);
	ps_printf(psdoc, "%.4f %.4f l\n", x+width, y);
	ps_printf(psdoc, "%.4f %.4f l\n", x+width, y+height);
	ps_printf(psdoc, "%.4f %.4f l\n", x, y+height);
	ps_printf(psdoc, "closepath\n");
}
/* }}} */

/* PS_circle() {{{
 * Draws a circle
 */
PSLIB_API void PSLIB_CALL
PS_circle(PSDoc *psdoc, float x, float y, float radius) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH | PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN | PS_SCOPE_GLYPH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path', 'template', 'pattern', 'glyph' or 'page' scope."), __FUNCTION__);
		return;
	}
	if(radius < 0.0) {
		ps_error(psdoc, PS_RuntimeError, _("Radius for circle is less than 0.0."));
		return;
	}

	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	if(ps_current_scope(psdoc) != PS_SCOPE_PATH) {
		ps_enter_scope(psdoc, PS_SCOPE_PATH);
		ps_printf(psdoc, "newpath\n");
	}
	ps_printf(psdoc, "%.4f %.4f a\n", x+radius, y);
	ps_printf(psdoc, "%.4f %.4f %.4f 0 360 arc\n", x, y, radius);
}
/* }}} */

/* PS_arc() {{{
 * Draws an arc counterclockwise
 */
PSLIB_API void PSLIB_CALL
PS_arc(PSDoc *psdoc, float x, float y, float radius, float alpha, float beta) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH | PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path', 'template', 'pattern' or 'page' scope."), __FUNCTION__);
		return;
	}
	if(radius < 0.0) {
		ps_error(psdoc, PS_RuntimeError, _("Radius for arc is less than 0.0."));
		return;
	}

	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	if(ps_current_scope(psdoc) != PS_SCOPE_PATH) {
		ps_enter_scope(psdoc, PS_SCOPE_PATH);
		ps_printf(psdoc, "newpath\n");
	}
//	ps_printf(psdoc, "%.4f %.4f a\n", x+radius*cos(alpha/180*M_PI), y+radius*sin(alpha/180*M_PI));
	ps_printf(psdoc, "%.4f %.4f %.4f %.4f %.4f arc\n", x, y, radius, alpha, beta);
}
/* }}} */

/* PS_arcn() {{{
 * Draws an arc clockwise
 */
PSLIB_API void PSLIB_CALL
PS_arcn(PSDoc *psdoc, float x, float y, float radius, float alpha, float beta) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH | PS_SCOPE_PAGE | PS_SCOPE_TEMPLATE | PS_SCOPE_PATTERN)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path', 'template', 'pattern' or 'page' scope."), __FUNCTION__);
		return;
	}
	if(radius < 0.0) {
		ps_error(psdoc, PS_RuntimeError, _("Radius for arc is less than 0.0."));
		return;
	}

	psdoc->agstates[psdoc->agstate].x = x;
	psdoc->agstates[psdoc->agstate].y = y;
	if(ps_current_scope(psdoc) != PS_SCOPE_PATH) {
		ps_enter_scope(psdoc, PS_SCOPE_PATH);
		ps_printf(psdoc, "newpath\n");
	}
//	ps_printf(psdoc, "%.4f %.4f a\n", x+radius*cos(beta/180*M_PI), y+radius*sin(beta/180*M_PI));
	ps_printf(psdoc, "%.4f %.4f %.4f %.4f %.4f arcn\n", x, y, radius, alpha, beta);
}
/* }}} */

/* PS_curveto() {{{
 * Draws a curve
 */
PSLIB_API void PSLIB_CALL
PS_curveto(PSDoc *psdoc, float x1, float y1, float x2, float y2, float x3, float y3) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	psdoc->agstates[psdoc->agstate].x = x3;
	psdoc->agstates[psdoc->agstate].y = y3;
	ps_printf(psdoc, "%f %f %f %f %f %f curveto\n", x1, y1, x2, y2, x3, y3);
}
/* }}} */

/* PS_setgray() {{{
 * Sets gray value for following line drawing. 0 means black and 1 means white.
 */
PSLIB_API void PSLIB_CALL
PS_setgray(PSDoc *psdoc, float gray) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "%f setgray\n", gray);
}
/* }}} */

/* PS_clip() {{{
 * Clips drawing to previously created path.
 */
PSLIB_API void PSLIB_CALL
PS_clip(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "clip\n");
	ps_leave_scope(psdoc, PS_SCOPE_PATH);
}
/* }}} */

/* PS_closepath() {{{
 * Closes a path. Connects the last point with the first point.
 */
PSLIB_API void PSLIB_CALL
PS_closepath(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "closepath\n");
}
/* }}} */

/* PS_closepath_stroke() {{{
 * Closes path and draws it.
 */
PSLIB_API void PSLIB_CALL
PS_closepath_stroke(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "closepath\n");
	ps_setcolor(psdoc, PS_COLORTYPE_STROKE);
	ps_printf(psdoc, "stroke\n");
	ps_leave_scope(psdoc, PS_SCOPE_PATH);
}
/* }}} */

/* PS_fill_stroke() {{{
 * Fills and draws a path
 */
PSLIB_API void PSLIB_CALL
PS_fill_stroke(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "gsave ");
	ps_setcolor(psdoc, PS_COLORTYPE_FILL);
	ps_printf(psdoc, "fill grestore\n");
	ps_setcolor(psdoc, PS_COLORTYPE_STROKE);
	ps_printf(psdoc, "stroke\n");
	ps_leave_scope(psdoc, PS_SCOPE_PATH);
}
/* }}} */

/* PS_stroke() {{{
 * Draws a path
 */
PSLIB_API void PSLIB_CALL
PS_stroke(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_setcolor(psdoc, PS_COLORTYPE_STROKE);
	ps_printf(psdoc, "stroke\n");
	ps_leave_scope(psdoc, PS_SCOPE_PATH);
}
/* }}} */

/* PS_fill() {{{
 * Fills a path.
 */
PSLIB_API void PSLIB_CALL
PS_fill(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'path' scope."), __FUNCTION__);
		return;
	}
	ps_setcolor(psdoc, PS_COLORTYPE_FILL);
	ps_printf(psdoc, "fill\n");
	ps_leave_scope(psdoc, PS_SCOPE_PATH);
}
/* }}} */

/* PS_shfill() {{{
 * Fills a path with a shading.
 */
PSLIB_API void PSLIB_CALL
PS_shfill(PSDoc *psdoc, int shading) {
	PSShading *psshading;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	psshading = _ps_get_shading(psdoc, shading);
	if(NULL == psshading) {
		ps_error(psdoc, PS_RuntimeError, _("PSShading is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern' or 'template' scope."), __FUNCTION__);
		return;
	}

	ps_output_shading_dict(psdoc, psshading);
	ps_printf(psdoc, "shfill\n");
}
/* }}} */

/* PS_save() {{{
 * Saves the current context.
 */
PSLIB_API void PSLIB_CALL
PS_save(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(psdoc->agstate >= PS_MAX_GSTATE_LEVELS-1) {
		ps_error(psdoc, PS_Warning, _("No more graphic states available."));
		return;
	}
	/* Take over the old graphics state */
	psdoc->agstate++;
	memcpy(&(psdoc->agstates[psdoc->agstate]), &(psdoc->agstates[psdoc->agstate-1]), sizeof(PSGState));
	psdoc->agstates[psdoc->agstate].x = psdoc->agstates[psdoc->agstate-1].x;
	psdoc->agstates[psdoc->agstate].y = psdoc->agstates[psdoc->agstate-1].y;

	ps_printf(psdoc, "gsave %% start level %d\n", psdoc->agstate);
}
/* }}} */

/* PS_restore() {{{
 * Restores a previously save context.
 */
PSLIB_API void PSLIB_CALL
PS_restore(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(psdoc->agstate <= 0) {
		ps_error(psdoc, PS_Warning, _("PS_restore() has been called more often than PS_save()."));
		return;
	}
	ps_printf(psdoc, "grestore %% end level %d\n", psdoc->agstate);
	psdoc->agstate--;
}
/* }}} */

/* PS_create_gstate() {{{
 * Creates a new graphic state and returns its id.
 */
PSLIB_API int PSLIB_CALL
PS_create_gstate(PSDoc *psdoc, const char *optlist) {
	ght_hash_table_t *opthash;
	PSGState *psgstate;
	int gstateid;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	if(NULL != optlist && optlist[0] != '\0') {
		ps_error(psdoc, PS_RuntimeError, _("Option list may not be empty."));
		return(0);
	}

	if(NULL == (opthash = ps_parse_optlist(psdoc, optlist))) {
		ps_error(psdoc, PS_RuntimeError, _("Error while parsing option list."));
		return(0);
	}

	if(NULL == (psgstate = (PSGState *) psdoc->malloc(psdoc, sizeof(PSGState), _("Allocate memory for graphic state.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for graphic state."));
		return(0);
	}
	memset(psgstate, 0, sizeof(PSGState));
	psgstate->opthash = opthash;
	if(0 == (gstateid = _ps_register_gstate(psdoc, psgstate))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register gstate."));
		psdoc->free(psdoc, psgstate);
		return(0);
	}

	return(gstateid);
}
/* }}} */

/* PS_set_gstate() {{{
 * Sets a graphic state.
 */
PSLIB_API void PSLIB_CALL
PS_set_gstate(PSDoc *psdoc, int gstate) {
	ght_hash_table_t *opthash;
	ght_iterator_t iterator;
	PSGState *psgstate;
	char *optvalue, *optname;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	psgstate = _ps_get_gstate(psdoc, gstate);
	if(NULL == psgstate) {
		ps_error(psdoc, PS_RuntimeError, _("PSGState is null."));
		return;
	}
	opthash = psgstate->opthash;
	for(optvalue = ght_first(opthash, &iterator, (void **) &optname); optvalue != NULL; optvalue = ght_next(opthash, &iterator, (void **) &optname)) {
		if(strcmp(optname, "setsmoothness") == 0) {
			float smoothness;
			if(0 == (smoothness = get_optlist_element_as_float(psdoc, opthash, "setsmoothness", &smoothness))) {
				PS_setsmoothness(psdoc, smoothness);
			}
		} else if(strcmp(optname, "linewidth") == 0) {
			float linewidth;
			if(0 == (linewidth = get_optlist_element_as_float(psdoc, opthash, "linewidth", &linewidth))) {
				PS_setlinewidth(psdoc, linewidth);
			}
		} else if(strcmp(optname, "linecap") == 0) {
			int linecap;
			if(0 == (linecap = get_optlist_element_as_int(psdoc, opthash, "linecap", &linecap))) {
				PS_setlinecap(psdoc, linecap);
			}
		} else if(strcmp(optname, "linejoin") == 0) {
			int linejoin;
			if(0 == (linejoin = get_optlist_element_as_int(psdoc, opthash, "linejoin", &linejoin))) {
				PS_setlinejoin(psdoc, linejoin);
			}
		} else if(strcmp(optname, "flatness") == 0) {
			float flatness;
			if(0 == (flatness = get_optlist_element_as_float(psdoc, opthash, "flatness", &flatness))) {
				PS_setflat(psdoc, flatness);
			}
		} else if(strcmp(optname, "miterlimit") == 0) {
			float miterlimit;
			if(0 == (miterlimit = get_optlist_element_as_float(psdoc, opthash, "miterlimit", &miterlimit))) {
				PS_setmiterlimit(psdoc, miterlimit);
			}
		} else if(strcmp(optname, "overprintmode") == 0) {
			int overprintmode;
			if(0 == (overprintmode = get_optlist_element_as_int(psdoc, opthash, "overprintmode", &overprintmode))) {
				PS_setoverprintmode(psdoc, overprintmode);
			}
		} else {
			ps_error(psdoc, PS_Warning, _("Graphic state contains unknown option."));
		}
	}
}
/* }}} */

/* PS_rotate() {{{
 * Rotates the coordinate system, which will effect all following drawing
 * operations.
 */
PSLIB_API void PSLIB_CALL
PS_rotate(PSDoc *psdoc, float x) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "%f rotate\n", x);		
}
/* }}} */

/* PS_translate() {{{
 * Translates the coordinate system, which will effect all following drawing
 * operations.
 */
PSLIB_API void PSLIB_CALL
PS_translate(PSDoc *psdoc, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "%.2f %.2f translate\n", x, y);		
}
/* }}} */

/* PS_skew() {{{
 * FIXME: must be implemented
 * Skew the coordinate system
 */
PSLIB_API void PSLIB_CALL
PS_skew(PSDoc *psdoc, float alpha, float beta) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
}
/* }}} */

/* PS_concat() {{{
 * FIXME: must be implemented
 * Concatenate matrix to current transformation matrix
 */
PSLIB_API void PSLIB_CALL
PS_concat(PSDoc *psdoc, float a, float b, float c, float d, float e, float f) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
}
/* }}} */

/* PS_scale() {{{
 * Scales the coordinate system, which will effect all following drawing
 * operations.
 */
PSLIB_API void PSLIB_CALL
PS_scale(PSDoc *psdoc, float x, float y) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	ps_printf(psdoc, "%f %f scale\n", x, y);		
}
/* }}} */

/* PS_setcolor() {{{
 * Sets drawing color for all following operations
 */
PSLIB_API void PSLIB_CALL
PS_setcolor(PSDoc *psdoc, const char *type, const char *colorspace, float c1, float c2, float c3, float c4) {
	int icolorspace = 0;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	/* FIXME: Maybe called in pattern only if painttype = 1 */
	if(!ps_check_scope(psdoc, PS_SCOPE_PROLOG|PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'prolog', 'document', 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	if(!type || !*type) {
		ps_error(psdoc, PS_RuntimeError, _("Missing parameter type in PS_setcolor()."));
		return;
	}
	if(0 != strcmp(type, "fill") && 0 != strcmp(type, "stroke") && 0 != strcmp(type, "both") && 0 != strcmp(type, "fillstroke")) {
		ps_error(psdoc, PS_RuntimeError, _("Type in PS_setcolor() is none of 'fill', 'stroke' or 'fillstroke'."));
		return;
	}

	if(!colorspace || !*colorspace) {
		ps_error(psdoc, PS_RuntimeError, _("Missing paramter colorspace in PS_setcolor()."));
		return;
	}

	if(0 == strcmp(colorspace, "gray")) {
		if(c1 > 1.0 || c1 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Gray value out of range 0-1."));
			return;
		}
		icolorspace = PS_COLORSPACE_GRAY;
//		ps_printf(psdoc, "/DeviceGray setcolorspace %f setcolor\n", c1);
//		ps_printf(psdoc, "%f setgray\n", c1);
	} else if(0 == strcmp(colorspace, "rgb")) {
		if(c1 > 1.0 || c1 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Red value out of range 0-1."));
			return;
		}
		if(c2 > 1.0 || c2 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Green value out of range 0-1."));
			return;
		}
		if(c3 > 1.0 || c3 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Blue value out of range 0-1."));
			return;
		}
		icolorspace = PS_COLORSPACE_RGB;
//		ps_printf(psdoc, "/DeviceRGB setcolorspace %.4f %.4f %.4f setcolor\n", c1, c2, c3);
//		ps_printf(psdoc, "%.4f %.4f %.4f setrgbcolor\n", c1, c2, c3);
	} else if(0 == strcmp(colorspace, "cmyk")) {
		if(c1 > 1.0 || c1 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Cyan value out of range 0-1."));
			return;
		}
		if(c2 > 1.0 || c2 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Magenta value out of range 0-1."));
			return;
		}
		if(c3 > 1.0 || c3 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Yellow value out of range 0-1."));
			return;
		}
		if(c4 > 1.0 || c4 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Black value out of range 0-1."));
			return;
		}
		icolorspace = PS_COLORSPACE_CMYK;
//		ps_printf(psdoc, "/DeviceCMYK setcolorspace %.4f %.4f %.4f %.4f setcolor\n", c1, c2, c3, c4);
//		ps_printf(psdoc, "%.4f %.4f %.4f %.4f setcmykcolor\n", c1, c2, c3, c4);
	} else if(0 == strcmp(colorspace, "pattern")) {
		PSPattern *pspattern = _ps_get_pattern(psdoc, (int) c1);
		if(NULL == pspattern) {
			ps_error(psdoc, PS_RuntimeError, _("PSPattern is null."));
			return;
		}
		icolorspace = PS_COLORSPACE_PATTERN;
//		ps_printf(psdoc, "/Pattern setcolorspace %s setcolor\n", pspattern->name);
//		ps_printf(psdoc, "%s setpattern\n", pspattern->name);
	} else if(0 == strcmp(colorspace, "spot")) {
		PSSpotColor *spotcolor;
		spotcolor = _ps_get_spotcolor(psdoc, (int) c1);
		if(!spotcolor) {
			ps_error(psdoc, PS_RuntimeError, _("Could not find spot color."));
			return;
		}
		if(c2 > 1.0 || c2 < 0.0) {
			ps_error(psdoc, PS_RuntimeError, _("Tint value out of range 0-1."));
			return;
		}
		icolorspace = PS_COLORSPACE_SPOT;
	} else {
		ps_error(psdoc, PS_RuntimeError, _("Colorspace in PS_setcolor() is not in 'gray', 'rgb', 'cmyk', 'spot', or 'pattern'."));
	}

	if(0 == strcmp(type, "fill") || 0 == strcmp(type, "both") || 0 == strcmp(type, "fillstroke")) {
		psdoc->agstates[psdoc->agstate].fillcolor.prevcolorspace = psdoc->agstates[psdoc->agstate].fillcolor.colorspace;
		psdoc->agstates[psdoc->agstate].fillcolor.colorspace = icolorspace;
		psdoc->agstates[psdoc->agstate].fillcolorinvalid = ps_true;
		switch(icolorspace) {
			case PS_COLORSPACE_GRAY:
				psdoc->agstates[psdoc->agstate].fillcolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].fillcolor.c2 = 0;
				psdoc->agstates[psdoc->agstate].fillcolor.c3 = 0;
				psdoc->agstates[psdoc->agstate].fillcolor.c4 = 0;
				break;
			case PS_COLORSPACE_RGB:
				psdoc->agstates[psdoc->agstate].fillcolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].fillcolor.c2 = c2;
				psdoc->agstates[psdoc->agstate].fillcolor.c3 = c3;
				psdoc->agstates[psdoc->agstate].fillcolor.c4 = 0;
				break;
			case PS_COLORSPACE_CMYK:
				psdoc->agstates[psdoc->agstate].fillcolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].fillcolor.c2 = c2;
				psdoc->agstates[psdoc->agstate].fillcolor.c3 = c3;
				psdoc->agstates[psdoc->agstate].fillcolor.c4 = c4;
				break;
			case PS_COLORSPACE_SPOT:
				psdoc->agstates[psdoc->agstate].fillcolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].fillcolor.c2 = c2;
				break;
			case PS_COLORSPACE_PATTERN:
				psdoc->agstates[psdoc->agstate].fillcolor.pattern = c1;
				break;
		}
	}
	if(0 == strcmp(type, "stroke") || 0 == strcmp(type, "both") || 0 == strcmp(type, "fillstroke")) {
		psdoc->agstates[psdoc->agstate].strokecolor.prevcolorspace = psdoc->agstates[psdoc->agstate].strokecolor.colorspace;
		psdoc->agstates[psdoc->agstate].strokecolor.colorspace = icolorspace;
		psdoc->agstates[psdoc->agstate].strokecolorinvalid = ps_true;
		switch(icolorspace) {
			case PS_COLORSPACE_GRAY:
				psdoc->agstates[psdoc->agstate].strokecolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].strokecolor.c2 = 0;
				psdoc->agstates[psdoc->agstate].strokecolor.c3 = 0;
				psdoc->agstates[psdoc->agstate].strokecolor.c4 = 0;
				break;
			case PS_COLORSPACE_RGB:
				psdoc->agstates[psdoc->agstate].strokecolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].strokecolor.c2 = c2;
				psdoc->agstates[psdoc->agstate].strokecolor.c3 = c3;
				psdoc->agstates[psdoc->agstate].strokecolor.c4 = 0;
				break;
			case PS_COLORSPACE_CMYK:
				psdoc->agstates[psdoc->agstate].strokecolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].strokecolor.c2 = c2;
				psdoc->agstates[psdoc->agstate].strokecolor.c3 = c3;
				psdoc->agstates[psdoc->agstate].strokecolor.c4 = c4;
				break;
			case PS_COLORSPACE_SPOT:
				psdoc->agstates[psdoc->agstate].strokecolor.c1 = c1;
				psdoc->agstates[psdoc->agstate].strokecolor.c2 = c2;
				break;
			case PS_COLORSPACE_PATTERN:
				psdoc->agstates[psdoc->agstate].strokecolor.pattern = c1;
				break;
		}
	}

	return;
}
/* }}} */

/* PS_makespotcolor() {{{
 * Creates a spot color from the current fill color
 */
PSLIB_API int PSLIB_CALL
PS_makespotcolor(PSDoc *psdoc, const char *name, int reserved) {
	PSSpotColor *spotcolor;
	int ispotcolor;
	int spotcolorid;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}

	if(!ps_check_scope(psdoc, PS_SCOPE_PROLOG|PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'prolog', 'document', 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return(0);
	}

	if(0 != (ispotcolor = _ps_find_spotcolor_by_name(psdoc, name))) {
		return(ispotcolor);
	}

	if(psdoc->agstates[psdoc->agstate].fillcolor.colorspace != PS_COLORSPACE_GRAY &&
	   psdoc->agstates[psdoc->agstate].fillcolor.colorspace != PS_COLORSPACE_RGB &&
		 psdoc->agstates[psdoc->agstate].fillcolor.colorspace != PS_COLORSPACE_CMYK) {
		ps_error(psdoc, PS_MemoryError, _("Cannot make a spot color from a spot color or pattern."));
		return(0);
	}

	if(NULL == (spotcolor = (PSSpotColor *) psdoc->malloc(psdoc, sizeof(PSSpotColor), _("Allocate memory for spot color.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for spot color."));
		return(0);
	}
	memset(spotcolor, 0, sizeof(PSSpotColor));
	if(0 == (spotcolorid = _ps_register_spotcolor(psdoc, spotcolor))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register spotcolor."));
		psdoc->free(psdoc, spotcolor);
		return(0);
	}

	spotcolor->name = ps_strdup(psdoc, name);
	spotcolor->colorspace = psdoc->agstates[psdoc->agstate].fillcolor.colorspace;
	spotcolor->c1 = psdoc->agstates[psdoc->agstate].fillcolor.c1;
	spotcolor->c2 = psdoc->agstates[psdoc->agstate].fillcolor.c2;
	spotcolor->c3 = psdoc->agstates[psdoc->agstate].fillcolor.c3;
	spotcolor->c4 = psdoc->agstates[psdoc->agstate].fillcolor.c4;

	return(spotcolorid);
}
/* }}} */

/* PS_findfont() {{{
 * Finds a font. Actually tries to load an afm file for the given fontname.
 * Resource of the font must be freed with PS_delete_font()
 */
PSLIB_API int PSLIB_CALL
PS_findfont(PSDoc *psdoc, const char *fontname, const char *encoding, int embed) {
	PSFont *psfont;
	ADOBEFONTMETRIC *metrics;
	char *filename;
	const char *myenc;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	/* Check if this function does any output. If it does and we are
	 * beyond the header already, then issue
	 * a warning, because if the output goes between pages it will be lost
	 * when extracting certain pages from the document
	 */
	if(psdoc->headerwritten && (embed || (encoding != NULL && encoding[0] != '\0'))) {
		/* If the header has not been written, we do not need to check the
		 * scope.
		 */
		if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
			ps_error(psdoc, PS_Warning, _("Calling %s between pages is likely to cause problems when viewing the document. Call in within a page or in the prolog."), __FUNCTION__);
		}
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PROLOG|PS_SCOPE_PAGE|PS_SCOPE_DOCUMENT|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'prolog', 'document', 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return(0);
	}

	if(NULL == fontname) {
		ps_error(psdoc, PS_RuntimeError, _("No font name give to PS_findfont()."));
		return(0);
	}

	if(NULL == (psfont = (PSFont *) psdoc->malloc(psdoc, sizeof(PSFont), _("Allocate memory for font.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for font."));
		return(0);
	}

	/* Remember the PSDoc for which the font was loaded. */
	psfont->psdoc = psdoc;

	filename = ps_find_resource(psdoc, "FontAFM", fontname);
	if(NULL == filename) {
		if(NULL == (filename = (char *) psdoc->malloc(psdoc, strlen(fontname)+5, _("Could not allocate memory for afm filename.")))) {
			return(0);
		}
		strcpy(filename, fontname);
		strcat(filename, ".afm");
		metrics = readadobe(psdoc, filename);
		psdoc->free(psdoc, filename);
	} else {
		metrics = readadobe(psdoc, filename);
	}

	if(metrics == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("Font metrics could not be loaded (%s)."), fontname);
		psdoc->free(psdoc, psfont);
		return(0);
	}

	myenc = encoding;
	if((myenc == NULL || myenc[0] == '\0') && (NULL == (myenc = ps_find_resource(psdoc, "FontEncoding", fontname)))) {
		/* The encoding vector for the default encoding is already
		 * part of the prolog and need not be output.
		 */
		readencoding(psdoc, metrics, NULL);
		psfont->encoding = ps_strdup(psdoc, "default");
	} else {
		if(!strcmp(myenc, "builtin")) {
			psfont->encoding = ps_strdup(psdoc, myenc);
			metrics->fontenc = ps_build_enc_from_font(psdoc, metrics);
			if(metrics->codingscheme)
				psdoc->free(psdoc, metrics->codingscheme);
			metrics->codingscheme = ps_strdup(psdoc, metrics->fontname);

		} else {
			if(0 > readencoding(psdoc, metrics, myenc)) {
				ps_error(psdoc, PS_RuntimeError, _("Encoding file could not be loaded (%s)."), myenc);
				psdoc->free(psdoc, psfont);
				return 0;
			}
			psfont->encoding = ps_strdup(psdoc, myenc);
		}

		/* If the header is not written, because we are before
		 * the first page, then output the header first.
		 */
		if(psdoc->beginprologwritten == ps_false) {
			ps_write_ps_comments(psdoc);
			ps_write_ps_beginprolog(psdoc);
		}

		/* Output the encoding vector */
		{
		int i, j;
		ENCODING *fontenc;
		fontenc = ps_build_enc_vector(psdoc, metrics->fontenc);
		ps_printf(psdoc, "/fontenc-%s [\n", metrics->codingscheme);
		for(i=0; i<32; i++) {
			for(j=0; j<8; j++) {
				if((fontenc->vec[i*8+j] != NULL) && (*(fontenc->vec[i*8+j]) != '\0'))
					ps_printf(psdoc, "8#%03o /%s ", i*8+j, fontenc->vec[i*8+j]);
			}
			ps_printf(psdoc, "\n");
		}
		ps_printf(psdoc, "] def\n");
		ps_free_enc_vector(psdoc, fontenc);
		}
	}

	psfont->metrics = metrics;

	if(ps_get_bool_parameter(psdoc, "kerning", 0)) {
		filename = ps_find_resource(psdoc, "FontProtusion", fontname);
		if(NULL == filename) {
			if(NULL == (filename = (char *) psdoc->malloc(psdoc, strlen(fontname)+5, _("Could not allocate memory for afm filename.")))) {
				return(0);
			}
			strcpy(filename, fontname);
			strcat(filename, ".pro");
			if(0 > readprotusion(psdoc, psfont, filename))
				ps_error(psdoc, PS_Warning, _("Could not open protusion file: %s"), filename);
			psdoc->free(psdoc, filename);
		} else {
			if(0 > readprotusion(psdoc, psfont, filename))
				ps_error(psdoc, PS_Warning, _("Could not open protusion file: %s"), filename);
		}
	}

	ps_set_word_spacing(psdoc, psfont, 0.0);
	if(embed) {
		FILE *fp;
		long fsize;
		filename = ps_find_resource(psdoc, "FontOutline", fontname);

		if(NULL == filename) {
			if(NULL == (filename = (char *) psdoc->malloc(psdoc, strlen(fontname)+5, _("Could not allocate memory for pfb filename.")))) {
				return(0);
			}
			strcpy(filename, fontname);
			strcat(filename, ".pfb");
			fp = ps_open_file_in_path(psdoc, filename);
			psdoc->free(psdoc, filename);
		} else {
			fp = ps_open_file_in_path(psdoc, filename);
		}
		if (fp) {
			unsigned char *bb;
			fseek(fp, 0, SEEK_END);
			fsize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			if(NULL != (bb = psdoc->malloc(psdoc, fsize, _("Could not allocate memory for content of font outline file.")))) {
				int ts = fread(bb, 1, fsize, fp);
				if ((bb[0] == (unsigned char) 128) && (bb[1]) == (unsigned char) 1) {
					unsigned int ulen, j;
					unsigned int posi,cxxc=0;
					char linebuf[80];

					/* If the header is not written, because we are before
					 * the first page, then output the header first.
					 */
					if(psdoc->beginprologwritten == ps_false) {
						ps_write_ps_comments(psdoc);
						ps_write_ps_beginprolog(psdoc);
					}

					ps_printf(psdoc, "%%BeginFont: %s\n", fontname);
					for (posi = 6; posi < fsize; posi++) {
						if ((bb[posi] == (unsigned char) 128) && (bb[posi+1] == (unsigned char) 2))
							break;
						if(bb[posi] != (unsigned char) '\r')
							ps_putc(psdoc, bb[posi]);
						else if(bb[posi] == (unsigned char) '\r' && bb[posi+1] != (unsigned char) '\n')
							ps_putc(psdoc, '\n');
					}
					ulen = bb[posi+2] & 0xff;
					ulen |= (bb[posi+3] << 8) & 0xff00;
					ulen |= (bb[posi+4] << 16) & 0xff0000;
					ulen |= (bb[posi+5] << 24) & 0xff000000;
          if (ulen > fsize)
						ulen = fsize-7;
					posi += 6;
					cxxc=0;
					for (j = 0; j < ulen; j++) {
						unsigned char u=bb[posi];
						linebuf[cxxc]=((u >> 4) & 15) + '0';
						if(u>0x9f) linebuf[cxxc]+='a'-':';
						++cxxc;
						u&=15; linebuf[cxxc]=u + '0';
						if(u>0x9) linebuf[cxxc]+='a'-':';
						++posi;
						++cxxc;
						if (cxxc > 72) {
							linebuf[cxxc++]='\n';
							linebuf[cxxc++]=0;
							ps_printf(psdoc, "%s", linebuf);
							cxxc = 0;
						}
					}
					linebuf[cxxc]=0;
					ps_printf(psdoc, "%s\n", linebuf);
					posi += 6;
					for (j = posi; j < fsize; j++) {
						if ((bb[j] == (unsigned char) 128) && (bb[j+1] == (unsigned char) 3))
							break;
						if(bb[j]=='\r')
							ps_printf(psdoc, "\n");
						else
							ps_printf(psdoc, "%c", bb[j]);
					}
					ps_printf(psdoc, "\n");
					cxxc = 0;
					ps_printf(psdoc, "%%EndFont\n");
				} else {
					ps_error(psdoc, PS_Warning, _("Outline of font '%s' does not start with 0x80 0x01"), fontname);
				}
				psdoc->free(psdoc, bb);
			} else {
				ps_error(psdoc, PS_Warning, _("Problems while reading font outline for '%s'."), fontname);
			}
			fclose(fp);
		} else {
			ps_error(psdoc, PS_Warning, _("Font outline could not be loaded for '%s'."), fontname);
		}
	}
	return(_ps_register_font(psdoc, psfont));
}
/* }}} */

/* PS_load_font() {{{
 * Finds a font. Actually tries to load an afm file for the given fontname.
 * Resource of the font must be freed with PS_delete_font()
 */
PSLIB_API int PSLIB_CALL
PS_load_font(PSDoc *psdoc, const char *fontname, int len, const char *encoding, const char *optlist) {
	ght_hash_table_t *opthash;
	ps_bool optlist_embedding = 0;
	/* Read the option list only, if it is non empty. */
	if(NULL != optlist && optlist[0] != '\0') {
		opthash = ps_parse_optlist(psdoc, optlist);
		if(NULL == opthash) {
			ps_error(psdoc, PS_RuntimeError, _("Error while parsing option list."));
			return(0);
		}
		/* Issue a warning only if a value is given but cannot be parsed */
		if(-2 == get_optlist_element_as_bool(psdoc, opthash, "embedding", &optlist_embedding)) {
			ps_error(psdoc, PS_Warning, _("Value of option list element 'embedding' could not be read, using default."));
		}
		ps_free_optlist(psdoc, opthash);
	}
	return(PS_findfont(psdoc, fontname, encoding, optlist_embedding));
}
/* }}} */

/* PS_string_geometry() {{{
 * Calculates the width of string using the given font.
 * If dimension is not NULL it will be filled with the width, descender and
 * ascender.
 */
PSLIB_API float PSLIB_CALL
PS_string_geometry(PSDoc *psdoc, const char *text, int xlen, int fontid, float size, float *dimension) {
	PSFont *psfont;
	float width = 0.0, ascender = 0.0, descender = 0.0, charspacing;
	int i, len;
	int ligonoff;
	int kernonoff;
	char ligdischar;
	ADOBEINFO *prevai=NULL;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0.0);
	}

	if(NULL == text)
		return(0.0);

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return(0.0);
	}

	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("No font available."));
		return(0.0);
	}

	if(NULL == psfont->metrics) {
		ps_error(psdoc, PS_RuntimeError, _("No font metrics available. Cannot calculate width of string."));
		return(-1.0);
	}

	if(0.0 == size)
		size = psfont->size;

	charspacing = PS_get_value(psdoc, "charspacing", 0) * 1000.0 / size;
	kernonoff = ps_get_bool_parameter(psdoc, "kerning", 1);
	ligonoff = ps_get_bool_parameter(psdoc, "ligatures", 1);
	if(ligonoff) {
		const char *tmp = PS_get_parameter(psdoc, "ligaturedisolvechar", 0.0);
		if(tmp && tmp[0])
			ligdischar = tmp[0];
		else
			ligdischar = '¦';
	}

//	printf("determine width of %s\n", text);
	len = strlen(text);
	if(xlen >= 0)
		len = xlen < len ? xlen : len;
	for(i=0; i<len; i++) {
		ADOBEINFO *ai;
		unsigned char c;
		char *adobename;

		c = (unsigned char) text[i];
		adobename = ps_inputenc_name(psdoc, c);
		if(adobename && adobename[0] != '\0') {
//			printf("search width for %s in 0x%X\n", adobename, psfont->metrics->adobechars);
			ai = gfindadobe(psfont->metrics->gadobechars, adobename);
			if(ai) {
				if(strcmp(adobename, "space") == 0) {
					width += (float) psfont->wordspace;
				} else {
					char *newadobename;
					int offset = 0;
					if(ligonoff == 1 &&
					   charspacing == 0.0 &&
					   ps_check_for_lig(psdoc, psfont->metrics, ai, &text[i+1], ligdischar, &newadobename, &offset)) {
						if(ps_fontenc_has_glyph(psdoc, psfont->metrics->fontenc, newadobename)) {
							ADOBEINFO *nai = gfindadobe(psfont->metrics->gadobechars, newadobename);
							if(nai) {
								ai = nai;
								i += offset;
							} else {
								ps_error(psdoc, PS_Warning, _("Font '%s' has no ligature '%s', disolving it."), psfont->metrics->fontname, newadobename);
							}
						} else {
							ps_error(psdoc, PS_Warning, _("Font encoding vector of font '%s' has no ligature '%s', disolving it."), psfont->metrics->fontname, newadobename);
						}
					}
					/* At this point either ai is ligature or the current char */
					width += (float) (ai->width);
					if(i < (len-1))
						width += charspacing;
	//			printf("new width is %f\n", width);
					if(kernonoff == 1 && NULL != prevai)
						width += (float) calculatekern(prevai, ai);
					descender = min((float) ai->lly, descender);
					ascender = max((float) ai->ury, ascender);
	//			printf("new width after adding kern is %f\n", width);
				}
			} else {
				ps_error(psdoc, PS_Warning, _("Glyph '%s' not found in metric file."), adobename);
			}
			prevai = ai;
		} else {
			ps_error(psdoc, PS_Warning, _("Character %d not in input encoding vector."), c);
		}
	}
	if(dimension != NULL) {
		dimension[0] = width*size/1000.0;
		dimension[1] = descender*size/1000.0;
		dimension[2] = ascender*size/1000.0;
	}
	return(width*size/1000.0);
}
/* }}} */

/* PS_stringwidth2() {{{
 * Calculates the width of string using the current font.
 */
PSLIB_API float PSLIB_CALL
PS_stringwidth2(PSDoc *psdoc, const char *text, int xlen, int fontid, float size) {
	return(PS_string_geometry(psdoc, text, xlen, fontid, size, NULL));
}
/* }}} */

/* PS_stringwidth() {{{
 * Calculates the width of string using the current font.
 */
PSLIB_API float PSLIB_CALL
PS_stringwidth(PSDoc *psdoc, const char *text, int fontid, float size) {
	return(PS_string_geometry(psdoc, text, -1, fontid, size, NULL));
}
/* }}} */

/* PS_deletefont() {{{
 * frees the memory allocated by a font
 */
PSLIB_API void PSLIB_CALL
PS_deletefont(PSDoc *psdoc, int fontid) {
	_ps_unregister_font(psdoc, fontid);
}
/* }}} */

/* PS_setfont() {{{
 * sets the font for all following text output operations
 */
PSLIB_API void PSLIB_CALL
PS_setfont(PSDoc *psdoc, int fontid, float size) {
	PSFont *psfont;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_PATTERN|PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page', 'pattern', or 'template' scope."), __FUNCTION__);
		return;
	}
	psfont = _ps_get_font(psdoc, fontid);
	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("PSFont is null."));
		return;
	}
	psdoc->font = psfont;
	psdoc->font->size = size;
	ps_set_word_spacing(psdoc, psdoc->font, 0.0);
	PS_set_value(psdoc, "leading", size*1.2);
	if(psfont->metrics) {
#ifdef WIN32
		if(_stricmp(psfont->metrics->codingscheme, "FontSpecific") == 0) {
#else
		if(strcasecmp(psfont->metrics->codingscheme, "FontSpecific") == 0) {
#endif
			ps_printf(psdoc, "/%s findfont %f scalefont setfont\n", psfont->metrics->fontname, size);
		} else {
			ps_printf(psdoc, "/%s /%s-%s fontenc-%s ReEncode\n", psfont->metrics->fontname, psfont->metrics->fontname, psfont->metrics->codingscheme, psfont->metrics->codingscheme);
			ps_printf(psdoc, "/%s-%s findfont %f scalefont setfont\n", psfont->metrics->fontname, psfont->metrics->codingscheme, size);
		}
	}
}
/* }}} */

/* PS_getfont() {{{
 * gets the font currently in use
 */
PSLIB_API int PSLIB_CALL
PS_getfont(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}

	return(_ps_find_font(psdoc, psdoc->font));
}
/* }}} */

/* PS_open_image() {{{
 * Opens an image which is already in memory
 */
PSLIB_API int PSLIB_CALL
PS_open_image(PSDoc *psdoc, const char *type, const char *source, const char *data, long length, int width, int height, int components, int bpc, const char *params) {
	PSImage *psimage;
	int imageid;
	const char *imgreuse;
	
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}

	imgreuse = PS_get_parameter(psdoc, "imagereuse", 0.0);
	if(!imgreuse || strcmp(imgreuse, "true") == 0) {
		/* If the header is not written, because we are before
		 * the first page, then output the header first.
		 */
		if(psdoc->beginprologwritten == ps_false) {
			ps_write_ps_comments(psdoc);
			ps_write_ps_beginprolog(psdoc);
		}

		if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
			ps_error(psdoc, PS_Warning, _("Calling %s between pages or on pages for reusable images may cause problems when viewing the document. Call it before the first page."), __FUNCTION__);
		}

		if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE|PS_SCOPE_PROLOG)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
			return 0;
		}
	} else {
		if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
			return 0;
		}
	}

	if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
		return(0);
	}
	memset(psimage, 0, sizeof(PSImage));

	if(NULL == (psimage->data = (char *) psdoc->malloc(psdoc, length+1, _("Allocate memory for image data.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
		psdoc->free(psdoc, psimage);
		return(0);
	}

	memcpy(psimage->data, data, length);
	if(!strcmp(type, "eps")) {
		char *bb;
		psimage->data[length] = '\0';
		bb = strstr(psimage->data, "%%BoundingBox:");
		if(bb) {
			float x, y, width, height;
			bb += 15;
			sscanf(bb, "%f %f %f %f", &x, &y, &width, &height);
			psimage->width = (int) width;
			psimage->height = (int) height;
			psimage->length = length;
		}
	} else {
		psimage->length = length;
		psimage->width = width;
		psimage->height = height;
		psimage->components = components;
		psimage->bpc = bpc;
		switch(psimage->components) {
			case 1:
				psimage->colorspace = PS_COLORSPACE_GRAY;
				break;
			case 3:
				psimage->colorspace = PS_COLORSPACE_RGB;
				break;
			case 4:
				psimage->colorspace = PS_COLORSPACE_CMYK;
				break;
			default:
				ps_error(psdoc, PS_RuntimeError, _("Image has unknown number of components per pixel."));
				psdoc->free(psdoc, psimage->data);
				psdoc->free(psdoc, psimage);
				return(0);
		}
	}
			
	psimage->type = ps_strdup(psdoc, type);
	if(!imgreuse || strcmp(imgreuse, "true") == 0) {
		char buffer[25];
		psimage->isreusable = 1;
		sprintf(buffer, "Imagedata%d", rand());
		psimage->name = ps_strdup(psdoc, buffer);
	}

	if(0 == (imageid = _ps_register_image(psdoc, psimage))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register image."));
		psdoc->free(psdoc, psimage->type);
		psdoc->free(psdoc, psimage->data);
		psdoc->free(psdoc, psimage);
		return(0);
	}

	if(psimage->isreusable) {
		if(!strcmp(type, "eps")) {
			ps_printf(psdoc, "/%s\n", psimage->name);
			ps_printf(psdoc, "currentfile\n");
			ps_printf(psdoc, "<< /Filter /SubFileDecode\n");
			ps_printf(psdoc, "   /DecodeParms << /EODCount 0 /EODString (*EOD*) >>\n");
			ps_printf(psdoc, ">> /ReusableStreamDecode filter\n");
			ps_write(psdoc, psimage->data, psimage->length);
			ps_printf(psdoc, "*EOD*\n");
			ps_printf(psdoc, "def\n");
		} else {
			char *tmpdata;
			const char *imgenc;
			int reallength, j;

			imgenc = PS_get_parameter(psdoc, "imageencoding", 0.0);
			ps_printf(psdoc, "/%s\n", psimage->name);
			ps_printf(psdoc, "currentfile\n");
			ps_printf(psdoc, "<< /Filter /ASCII%sDecode >>\n", (imgenc == NULL || strcmp(imgenc, "hex") != 0) ? "85" : "Hex");
			ps_printf(psdoc, "/ReusableStreamDecode filter\n");

			if(psimage->components == 4 && psimage->colorspace == PS_COLORSPACE_RGB) {
				char *dataptr, *tmpdataptr;
				int j;
				dataptr = psimage->data;
				reallength = psimage->height*psimage->width*3;
				tmpdata = tmpdataptr = psdoc->malloc(psdoc, psimage->height*psimage->width*3, _("Allocate memory for temporary image data."));
				for(j=0; j<psimage->length; j++) {
					*tmpdataptr++ = *dataptr++;
					*tmpdataptr++ = *dataptr++;
					*tmpdataptr++ = *dataptr++;
					dataptr++;
				}
			} else {
				tmpdata = psimage->data;
				reallength = psimage->length;
			}
			if(imgenc == NULL || strcmp(imgenc, "hex") != 0)
				ps_ascii85_encode(psdoc, tmpdata, reallength);
			else
				ps_asciihex_encode(psdoc, tmpdata, reallength);
			if(psimage->components == 4 && psimage->colorspace == PS_COLORSPACE_RGB)
				psdoc->free(psdoc, tmpdata);

			ps_printf(psdoc, "\ndef\n");
		}
	}

	return(imageid);
}
/* }}} */

/* PS_open_image_file() {{{
 * Opens an image from a file
 */
PSLIB_API int PSLIB_CALL
PS_open_image_file(PSDoc *psdoc, const char *type, const char *filename, const char *stringparam, int intparam) {
	PSImage *psimage;
	int imageid;
	const char *imgreuse;
	FILE *fp;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return 0;
	}

	imgreuse = PS_get_parameter(psdoc, "imagereuse", 0.0);
	if(!imgreuse || strcmp(imgreuse, "true") == 0) {
		/* If the header is not written, because we are before
		 * the first page, then output the header first.
		 */
		if(psdoc->beginprologwritten == ps_false) {
			ps_write_ps_comments(psdoc);
			ps_write_ps_beginprolog(psdoc);
		}

		if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
			ps_error(psdoc, PS_Warning, _("Calling %s between pages or on pages for reusable images may cause problems when viewing the document. Call it before the first page."), __FUNCTION__);
		}

		if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE|PS_SCOPE_PROLOG)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
			return 0;
		}
	} else {
		if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
			return 0;
		}
	}

	if(NULL == filename) {
		ps_error(psdoc, PS_RuntimeError, _("Filename of images is NULL."));
		return 0;
	}

	if ((fp = fopen(filename, "rb")) == NULL) {
		ps_error(psdoc, PS_RuntimeError, _("Could not open image file %s."), filename);
		return 0;
	}

	if(NULL == type) {
		ps_error(psdoc, PS_RuntimeError, _("Type of image is NULL."));
		return 0;
	}

#ifdef HAVE_LIBPNG
	if(0 == strncmp("png", type, 3)) {
#define SIG_READ 8
		char *dataptr, sig[SIG_READ];
		int i;
		int color_type, bit_depth;
		png_structp png_ptr;
		png_infop info_ptr;
		png_uint_32 row_bytes;
		png_bytep *row_pointers;
//		png_bytep row_pointer;

		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//	     png_voidp user_error_ptr, user_error_fn, user_warning_fn);

		if (png_ptr == NULL) {
			ps_error(psdoc, PS_RuntimeError, _("Could not create png structure."));
			fclose(fp);
			return(0);
		}

		/* Allocate/initialize the memory for image information.  REQUIRED. */
		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL) {
			ps_error(psdoc, PS_RuntimeError, _("Could not create png info structure."));
			fclose(fp);
			png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
			return(0);
		}

		/* Set error handling if you are using the setjmp/longjmp method (this is
		* the normal method of doing things with libpng).  REQUIRED unless you
		* set up your own error handlers in the png_create_read_struct() earlier.
		*/
		if (setjmp(png_jmpbuf(png_ptr))) {
			/* Free all of the memory associated with the png_ptr and info_ptr */
			ps_error(psdoc, PS_RuntimeError, _("Could not set error handler for libpng."));
			png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
			fclose(fp);
			/* If we get here, we had a problem reading the file */
			return (0);
		}

		if (fread(sig, 1, SIG_READ, fp) == 0 || !png_check_sig(sig, SIG_READ)) {
			fclose(fp);
			ps_error(psdoc, PS_RuntimeError, "File '%s' is not a PNG file", filename);
			return (0);
		}

		/* Set up the input control if you are using standard C streams */
		png_init_io(png_ptr, fp);

		/* If we have already read some of the signature */
		png_set_sig_bytes(png_ptr, SIG_READ);

		/*
		* If you have enough memory to read in the entire image at once,
		* and you need to specify only transforms that can be controlled
		* with one of the PNG_TRANSFORM_* bits (this presently excludes
		* dithering, filling, setting background, and doing gamma
		* adjustment), then you can read the entire image (including
		* pixels) into the info structure with this call:
		*/
#define mymemory
#ifdef mymemory
		png_read_info(png_ptr, info_ptr);
		color_type = png_get_color_type(png_ptr, info_ptr);
		bit_depth = png_get_bit_depth(png_ptr, info_ptr);
		if (bit_depth == 16) {
			png_set_strip_16(png_ptr);
		}

		/* FIXME: Quick fix to circumvent a segm fault in ps_place_image()
		 * when rgb images with alpha channel are loaded.
		 */
		if (color_type & PNG_COLOR_MASK_ALPHA)
			png_set_strip_alpha(png_ptr);

		/* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
#if defined(PNG_1_0_X) || defined (PNG_1_2_X)
			png_set_gray_1_2_4_to_8(png_ptr);
#else
			png_set_expand_gray_1_2_4_to_8(png_ptr);
#endif

		png_read_update_info(png_ptr, info_ptr);
#else
		/* Read the entire image */
		png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, png_voidp_NULL);
#endif

		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));

		if(stringparam != NULL && strcmp(stringparam, "mask") == 0) {
			psimage->ismask = ps_true;
		}

		if(stringparam != NULL && strcmp(stringparam, "masked") == 0) {
			PSImage *psmaskimage = _ps_get_image(psdoc, intparam);
			if(!psmaskimage) {
				ps_error(psdoc, PS_RuntimeError, _("Could not find image mask."));
				psdoc->free(psdoc, psimage);
				
				return(0);
			}
			psimage->imagemask = psmaskimage;
		}
		psimage->psdoc = psdoc;
		psimage->type = ps_strdup(psdoc, type);
		psimage->width = png_get_image_width(png_ptr, info_ptr);
		psimage->height = png_get_image_height(png_ptr, info_ptr);
		psimage->components = png_get_channels(png_ptr, info_ptr);
		psimage->bpc = png_get_bit_depth(png_ptr, info_ptr);
		switch(png_get_color_type(png_ptr, info_ptr)) {
			case PNG_COLOR_TYPE_GRAY:
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				psimage->colorspace = PS_COLORSPACE_GRAY;
				break;
			case PNG_COLOR_TYPE_RGB:
			case PNG_COLOR_TYPE_RGB_ALPHA:
				psimage->colorspace = PS_COLORSPACE_RGB;
				break;
			case PNG_COLOR_TYPE_PALETTE:
				psimage->colorspace = PS_COLORSPACE_INDEXED;
				break;
		}
		if(psimage->colorspace == PS_COLORSPACE_INDEXED) {
			int num_palette;
			png_colorp palette;
			png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
			if(NULL == (psimage->palette = psdoc->malloc(psdoc, sizeof(PSColor) * num_palette, _("Allocate memory for color palette.")))) {
				ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for color palette."));
				psdoc->free(psdoc, psimage);
				fclose(fp);
				return(0);
			}
			for(i=0; i<num_palette; i++) {
				psimage->palette[i].colorspace = PS_COLORSPACE_RGB;
				psimage->palette[i].c1 = palette[i].red/255.0;
				psimage->palette[i].c2 = palette[i].green/255.0;
				psimage->palette[i].c3 = palette[i].blue/255.0;
				psimage->palette[i].c4 = 0.0;
			}
			psimage->numcolors = num_palette;
		}

		row_bytes = png_get_rowbytes(png_ptr, info_ptr);
		psimage->length = row_bytes * psimage->height;
/*
		fprintf(stderr, "%dx%d pixel\n", psimage->width, psimage->height);
		fprintf(stderr, "%d channels\n", psimage->components);
		fprintf(stderr, "%d bits per color\n", psimage->bpc);
		fprintf(stderr, "%d bytes per row\n", row_bytes);
*/

		if(NULL == (psimage->data = psdoc->malloc(psdoc, psimage->height*row_bytes, _("Allocate memory for image data.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}

#ifdef mymemory
		if(NULL == (row_pointers = psdoc->malloc(psdoc, psimage->height*sizeof(png_bytep), _("Allocate memory for row pointers of png image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for row pointer of png image."));
			psdoc->free(psdoc, psimage->data);
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}

		dataptr = psimage->data;
		for(i=0; i<psimage->height; i++) {
			row_pointers[i] = dataptr;
//			fprintf(stderr, "Reading row %d\n", i);
//			png_read_row(png_ptr, dataptr, NULL);
			dataptr += row_bytes;
		}
		png_read_image(png_ptr, row_pointers);
		png_read_end(png_ptr, NULL);
		psdoc->free(psdoc, row_pointers);
#else
		row_pointers = png_get_rows(png_ptr, info_ptr);
		if(row_pointers == NULL) {
			ps_error(psdoc, PS_MemoryError, _("Could get array of image rows."));
			psdoc->free(psdoc, psimage->data);
			return(0);
		}
		dataptr = psimage->data;
		for(i=0; i<psimage->height; i++) {
			fprintf(stderr, "Copying %d row 0x%X -> 0x%X\n", i, row_pointers[i], dataptr);
			memcpy(dataptr, row_pointers[i], row_bytes);
			dataptr += row_bytes;
		}
#endif
		/* clean up after the read, and free any memory allocated - REQUIRED */
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

	} else
#endif /* HAVE_LIBPNG */
#ifdef HAVE_LIBJPEG
	if(0 == strncmp("jpeg", type, 4)) {
		char *dataptr;
		struct jpeg_decompress_struct cinfo;
		JSAMPARRAY buffer;
		int row_stride;
		struct jpeg_error_mgr jerr;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);
		jpeg_stdio_src(&cinfo, fp);
		jpeg_read_header(&cinfo, TRUE);
		jpeg_start_decompress(&cinfo);
		row_stride = cinfo.output_width * cinfo.output_components;

		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));

		psimage->psdoc = psdoc;
		psimage->type = ps_strdup(psdoc, type);
		psimage->width = cinfo.output_width;
		psimage->height = cinfo.output_height;
		psimage->components = cinfo.output_components;
		psimage->bpc = 8;
		switch(cinfo.out_color_space) {
			case JCS_GRAYSCALE:
				psimage->colorspace = PS_COLORSPACE_GRAY;
				break;
			case JCS_RGB:
				psimage->colorspace = PS_COLORSPACE_RGB;
				break;
			case JCS_CMYK:
				psimage->colorspace = PS_COLORSPACE_CMYK;
				break;
		}

//		if(psimage->components == 1)
//			psimage->colorspace = PS_COLORSPACE_GRAY;
//		else
//			psimage->colorspace = PS_COLORSPACE_RGB;
		psimage->length = psimage->width * psimage->height * psimage->components;
		if(NULL == (psimage->data = psdoc->malloc(psdoc, psimage->length, _("Allocate memory for image data.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
			fclose(fp);
			return(0);
		}

		buffer = (*cinfo.mem->alloc_sarray)
			    ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
		dataptr = psimage->data;
		while (cinfo.output_scanline < cinfo.output_height) {
			jpeg_read_scanlines(&cinfo, buffer, 1);
			memcpy(dataptr, buffer[0], row_stride);
			dataptr += row_stride;
		}
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
	} else
#endif /* HAVE_LIBJPEG */
#ifdef HAVE_LIBGIF
	if(0 == strncmp("gif", type, 3)) {
		GifFileType* gft;
		int i, numcolors = 0, alphapalette = -1;
		ColorMapObject* colormap;
		GifColorType c;
		unsigned char *dataptr;
#if GIFLIB_MAJOR > 5 || GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1
		int error;
		  #define GIF_OPEN_FILENAME(filename) DGifOpenFileName(filename, &error)
		  #define GIF_CLOSE_FILE(gif) DGifCloseFile(gif, &error)
#else
		  #define GIF_OPEN_FILENAME(filename) DGifOpenFileName(filename)
		  #define GIF_CLOSE_FILE(gif) DGifCloseFile(gif)
#endif

		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));

		if ((gft = GIF_OPEN_FILENAME(filename)) == NULL) {
			ps_error(psdoc, PS_RuntimeError, _("%s is not a gif file!"), filename);
			fclose(fp);
			return(0);
		}
		if (DGifSlurp(gft) != GIF_OK) {
#if GIFLIB_MAJOR > 5 || GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1
			ps_error(psdoc, PS_RuntimeError, _("Error %d while reading gif file!"), error);
#else
			ps_error(psdoc, PS_RuntimeError, _("Error %d while reading gif file!"), GifLastError());
#endif
			fclose(fp);
			return(0);
    }

		psimage->psdoc = psdoc;
		psimage->type = ps_strdup(psdoc, type);
		psimage->width = gft->SWidth;
		psimage->height = gft->SHeight;
		psimage->components = 1;
		psimage->bpc = 8;
		psimage->colorspace = PS_COLORSPACE_INDEXED;

		// look for the transparent color extension
		for (i = 0; i < gft->SavedImages[0].ExtensionBlockCount; ++i) {
			ExtensionBlock* eb = gft->SavedImages[0].ExtensionBlocks + i;
			if (eb->Function == GRAPHICS_EXT_FUNC_CODE && eb->ByteCount == 4) {
				int has_transparency = ((eb->Bytes[0] & 1) == 1);
				if (has_transparency) 
					alphapalette = eb->Bytes[3];
			}
		}
		colormap = gft->Image.ColorMap ? gft->Image.ColorMap : gft->SColorMap;
		numcolors = colormap->ColorCount;

		if(NULL == (psimage->palette = psdoc->malloc(psdoc, sizeof(PSColor) * numcolors, _("Allocate memory for color palette.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for color palette."));
			GIF_CLOSE_FILE(gft);
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}

		for (i=0; i<numcolors; i++) {
			psimage->palette[i].colorspace = PS_COLORSPACE_RGB;
			c = colormap->Colors[i];
			if (i == alphapalette)
				psimage->palette[i].c1 = psimage->palette[i].c2 = psimage->palette[i].c3 = psimage->palette[i].c4 = 1;// Fully transparent
			else {
				psimage->palette[i].c1 = c.Red/255.0;
				psimage->palette[i].c2 = c.Green/255.0;
				psimage->palette[i].c3 = c.Blue/255.0;
				psimage->palette[i].c4 = 0;
			}
		}
		psimage->numcolors = numcolors;

		dataptr = (unsigned char *) gft->SavedImages[0].RasterBits;

		psimage->length = psimage->width * psimage->height;
		if(NULL == (psimage->data = psdoc->malloc(psdoc, psimage->length, _("Allocate memory for image data.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
			GIF_CLOSE_FILE(gft);
			psdoc->free(psdoc, psimage->palette);
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}

		if(gft->Image.Interlace) {
			unsigned char *out;
			unsigned int row;
			for (row = 0; row < psimage->height; row += 8) {
				out = psimage->data + row*psimage->width;
				memcpy(out, dataptr, psimage->width);
				dataptr += psimage->width;
			}
			for (row = 4; row < psimage->height; row += 8) {
				out = psimage->data + row*psimage->width;
				memcpy(out, dataptr, psimage->width);
				dataptr += psimage->width;
			}
			for (row = 2; row < psimage->height; row += 4) {
				out = psimage->data + row*psimage->width;
				memcpy(out, dataptr, psimage->width);
				dataptr += psimage->width;
			}
			for (row = 1; row < psimage->height; row += 2) {
				out = psimage->data + row*psimage->width;
				memcpy(out, dataptr, psimage->width);
				dataptr += psimage->width;
			}
		} else {
			memcpy(psimage->data, dataptr, psimage->length);
		}

		GIF_CLOSE_FILE(gft);

	} else
#endif /* HAVE_LIBGIF */
#ifdef HAVE_LIBTIFF
	if(0 == strncmp("tiff", type, 4)) {
		TIFF* tif;
		uint32* raster;

		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));

		psimage->psdoc = psdoc;
		psimage->type = ps_strdup(psdoc, type);
		psimage->components = 3;
		psimage->bpc = 8;
		psimage->colorspace = PS_COLORSPACE_RGB;

		if(NULL == (tif = TIFFOpen(filename, "r"))) {
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &(psimage->width));
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &(psimage->height));
		psimage->length = psimage->width * psimage->height * 3;

		raster = (uint32*) _TIFFmalloc(psimage->width * psimage->height * sizeof (uint32));
		if (raster != NULL) {
			if (TIFFReadRGBAImageOriented(tif, psimage->width, psimage->height, raster, ORIENTATION_TOPLEFT, 0)) {
				unsigned char *src, *dst;
				int pixel_count = psimage->width * psimage->height;

				if(NULL == (psimage->data = psdoc->malloc(psdoc, psimage->length, _("Allocate memory for image data.")))) {
					ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
					psdoc->free(psdoc, psimage);
					_TIFFfree(raster);
					TIFFClose(tif);
					fclose(fp);
					return(0);
				}

				dst = psimage->data;
				src = (unsigned char *) raster;
				while( pixel_count > 0 ) {
					/* The raster is in ABGR order */
					dst[0] = src[3];
					dst[1] = src[2];
					dst[2] = src[1];
					src += 4;
					dst += 3;
					pixel_count--;
				}
			}
			_TIFFfree(raster);
		}
		TIFFClose(tif);
	} else
#endif /* HAVE_LIBTIFF */
#ifndef DISABLE_BMP
	if(0 == strncmp("bmp", type, 3)) {
		int w, h, bps, spp, xres, yres, i;
		unsigned char *color_table, *ct;
		int color_table_size, color_table_elements;
		unsigned char *data;

		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));

		psimage->psdoc = psdoc;
		psimage->type = ps_strdup(psdoc, type);

		if(NULL == (data = read_bmp(psdoc, filename, &w, &h, &bps, &spp, &xres, &yres,
						 &color_table, &color_table_size, &color_table_elements))) {
			ps_error(psdoc, PS_MemoryError, _("Could not read bmp file."));
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}
		psimage->width = w;
		psimage->height = h;
		if(NULL != color_table) {
			psimage->components = 1;
			psimage->bpc = bps;
			psimage->colorspace = PS_COLORSPACE_INDEXED;
			psimage->length = w * h;

			if(NULL == (psimage->palette = psdoc->malloc(psdoc, sizeof(PSColor) * color_table_size, _("Allocate memory for color palette.")))) {
				ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for color palette."));
				psdoc->free(psdoc, psimage);
				fclose(fp);
				return(0);
			}

			ct = color_table;
			for (i=0; i<color_table_size; i++) {
				psimage->palette[i].colorspace = PS_COLORSPACE_RGB;
				psimage->palette[i].c3 = *ct++/255.0;
				psimage->palette[i].c2 = *ct++/255.0;
				psimage->palette[i].c1 = *ct++/255.0;
				psimage->palette[i].c4 = *ct++/255.0;
			}
			psimage->numcolors = color_table_size;
			psdoc->free(psdoc, color_table);
		} else {
			psimage->components = spp;
			psimage->bpc = bps;
			psimage->colorspace = PS_COLORSPACE_RGB;
			psimage->length = w * h * 3;
		}
		/* data has been allocated in read_bmp() */
		psimage->data = data;
	} else
#endif
	if(0 == strncmp("eps", type, 3)) {
		char *bb;
		struct stat statbuf;
		if(0 > stat(filename, &statbuf)) {
			ps_error(psdoc, PS_RuntimeError, _("Could not stat eps file."));
			fclose(fp);
			return(0);
		}
		if(NULL == (psimage = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for image.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image."));
			fclose(fp);
			return(0);
		}
		memset(psimage, 0, sizeof(PSImage));
		psimage->type = ps_strdup(psdoc, type);

		if(NULL == (psimage->data = psdoc->malloc(psdoc, statbuf.st_size+1, _("Allocate memory for image data.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for image data."));
			psdoc->free(psdoc, psimage);
			fclose(fp);
			return(0);
		}
		psimage->psdoc = psdoc;
		psimage->length = statbuf.st_size;

		fread(psimage->data, statbuf.st_size, 1, fp);
		psimage->data[statbuf.st_size] = '\0';
		bb = strstr(psimage->data, "%%BoundingBox:");
		if(bb) {
			float x, y, width, height;
			bb += 15;
			sscanf(bb, "%f %f %f %f", &x, &y, &width, &height);
			psimage->width = (int) width;
			psimage->height = (int) height;
		}
	} else {
		ps_error(psdoc, PS_RuntimeError, _("Images of type '%s' not supported."), type);
		fclose(fp);
		return(0);
	}
	/* close the file */
	fclose(fp);

	/* that's it */

	if(!imgreuse || strcmp(imgreuse, "true") == 0) {
		char buffer[25];
		psimage->isreusable = 1;
		sprintf(buffer, "Imagedata%d", rand());
		psimage->name = ps_strdup(psdoc, buffer);
	}

	if(0 == (imageid = _ps_register_image(psdoc, psimage))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register image."));
		psdoc->free(psdoc, psimage->type);
		psdoc->free(psdoc, psimage->data);
		psdoc->free(psdoc, psimage);
		return(0);
	}

	if(psimage->isreusable) {
		if(!strcmp(type, "eps")) {
			ps_printf(psdoc, "/%s\n", psimage->name);
			ps_printf(psdoc, "currentfile\n");
			ps_printf(psdoc, "<< /Filter /SubFileDecode\n");
			ps_printf(psdoc, "   /DecodeParms << /EODCount 0 /EODString (*EOD*) >>\n");
			ps_printf(psdoc, ">> /ReusableStreamDecode filter\n");
			ps_write(psdoc, psimage->data, psimage->length);
			ps_printf(psdoc, "*EOD*\n");
			ps_printf(psdoc, "def\n");
		} else {
			char *tmpdata;
			const char *imgenc;
			int reallength, j;

			imgenc = PS_get_parameter(psdoc, "imageencoding", 0.0);
			ps_printf(psdoc, "/%s\n", psimage->name);
			ps_printf(psdoc, "currentfile\n");
			ps_printf(psdoc, "<< /Filter /ASCII%sDecode >>\n", (imgenc == NULL || strcmp(imgenc, "hex") != 0) ? "85" : "Hex");
			ps_printf(psdoc, "/ReusableStreamDecode filter\n");

			if(psimage->components == 4 && psimage->colorspace == PS_COLORSPACE_RGB) {
				char *dataptr, *tmpdataptr;
				int j;
				dataptr = psimage->data;
				reallength = psimage->height*psimage->width*3;
				tmpdata = tmpdataptr = psdoc->malloc(psdoc, psimage->height*psimage->width*3, _("Allocate memory for temporary image data."));
				for(j=0; j<psimage->length; j++) {
					*tmpdataptr++ = *dataptr++;
					*tmpdataptr++ = *dataptr++;
					*tmpdataptr++ = *dataptr++;
					dataptr++;
				}
			} else {
				tmpdata = psimage->data;
				reallength = psimage->length;
			}
			if(imgenc == NULL || strcmp(imgenc, "hex") != 0)
				ps_ascii85_encode(psdoc, tmpdata, reallength);
			else
				ps_asciihex_encode(psdoc, tmpdata, reallength);
			if(psimage->components == 4 && psimage->colorspace == PS_COLORSPACE_RGB)
				psdoc->free(psdoc, tmpdata);

			ps_printf(psdoc, "\ndef\n");
		}
	}
	return(imageid);
}
/* }}} */

/* PS_place_image() {{{
 * Place an image on the page
 */
PSLIB_API void PSLIB_CALL
PS_place_image(PSDoc *psdoc, int imageid, float x, float y, float scale) {
	PSImage *image;
	const char *imgreuse;
	
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	imgreuse = PS_get_parameter(psdoc, "imagereuse", 0.0);
	if(!imgreuse || strcmp(imgreuse, "true") == 0) {
		if(!ps_check_scope(psdoc, PS_SCOPE_PAGE|PS_SCOPE_TEMPLATE)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' or 'template' scope."), __FUNCTION__);
			return;
		}
	} else {
		if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
			ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
			return;
		}
	}

	if(scale == 0.0) {
		ps_error(psdoc, PS_Warning, _("Scaling image to 0.0 will make it disappear."), __FUNCTION__);
	}

	image = _ps_get_image(psdoc, imageid);
	if(NULL == image) {
		ps_error(psdoc, PS_RuntimeError, _("PSImage is null."));
		return;
	}
	if(NULL == image->type) {
		ps_error(psdoc, PS_RuntimeError, _("Image has no type set."));
		return;
	}

	/* All images must have data, except for templates */
	if(NULL == image->data && strcmp(image->type, "template")) {
		ps_error(psdoc, PS_RuntimeError, _("Image has no data."));
		return;
	}

	if(
#ifdef HAVE_LIBPNG
	  0 == strncmp(image->type, "png", 3) ||
#endif
#ifdef HAVE_LIBJPEG
		0 == strncmp(image->type, "jpeg", 4) ||
#endif
#ifdef HAVE_LIBGIF
		0 == strncmp(image->type, "gif", 3) ||
#endif
#ifdef HAVE_LIBTIFF
		0 == strncmp(image->type, "tiff", 4) ||
#endif
#ifndef DISABLE_BMP
		0 == strncmp(image->type, "bmp", 3) ||
#endif
		0 == strncmp(image->type, "memory", 6) ) {
		unsigned char *dataptr, *tmpdata, *tmpdataptr;
		const char *imgenc;
		int i, j, k, reallength;
		ps_printf(psdoc, "gsave\n");
#ifdef LEVEL1
		ps_printf(psdoc, "%.2f %.2f scale\n", scale*image->width, scale*image->height);
		ps_printf(psdoc, "/readstring { currentfile exch readhexstring pop } bind def\n");
		ps_printf(psdoc, "/rpicstr %d string def\n", image->width);
		ps_printf(psdoc, "/gpicstr %d string def\n", image->width);
		ps_printf(psdoc, "/bpicstr %d string def\n", image->width);
		ps_printf(psdoc, "%d %d %d\n", image->width, image->height, image->bpc);
		ps_printf(psdoc, "[%d 0 0 %d %f %f]\n", image->width, -image->height, -x/scale, image->height+y/scale);
		ps_printf(psdoc, "{ rpicstr readstring }\n");
		ps_printf(psdoc, "{ gpicstr readstring }\n");
		ps_printf(psdoc, "{ bpicstr readstring }\n");
		ps_printf(psdoc, "true %d\n", image->components);
		ps_printf(psdoc, "colorimage\n");
		rowptr = image->data;
		dataptr = image->data;
		for(j=0; j<image->height; j++) {
			for(k=0; k<image->components; k++) {
				dataptr = rowptr+k;
				for(i=0; i<image->width; i++) {
					ps_printf(psdoc, "%02x", *dataptr);
					dataptr += 3;
				}
				ps_printf(psdoc, "\n");
			}
			rowptr += image->components*image->width;
		}
#else
		ps_printf(psdoc, "%.2f %.2f translate\n", x, y);
		ps_printf(psdoc, "%.2f %.2f scale\n", scale*image->width, scale*image->height);

		imgenc = PS_get_parameter(psdoc, "imageencoding", 0.0);

		if(image->isreusable)
			ps_printf(psdoc, "%s 0 setfileposition\n", image->name);
			
		if(!image->ismask) {
			/* Output the image data */
			switch(image->colorspace) {
				case PS_COLORSPACE_GRAY:
					ps_printf(psdoc, "/DeviceGray setcolorspace\n");
					break;
				case PS_COLORSPACE_RGB:
					ps_printf(psdoc, "/DeviceRGB setcolorspace\n");
					break;
				case PS_COLORSPACE_CMYK:
		  		ps_printf(psdoc, "/DeviceCMYK setcolorspace\n");
					break;
				case PS_COLORSPACE_INDEXED: {
					int i;
					ps_printf(psdoc, "[ /Indexed /DeviceRGB %d <\n", image->numcolors-1);
					for(i=0; i<image->numcolors; i++) {
						ps_printf(psdoc, "%02X", (int) ((image->palette[i].c1 * 255) + 0.5));
						ps_printf(psdoc, "%02X", (int) ((image->palette[i].c2 * 255) + 0.5));
						ps_printf(psdoc, "%02X", (int) ((image->palette[i].c3 * 255) + 0.5));
						if((i+1)%8 == 0)
							ps_printf(psdoc, "\n");
						else
							ps_printf(psdoc, " ");
					}
					ps_printf(psdoc, "> ] setcolorspace\n");
					break;
				}
			}
			ps_printf(psdoc, "<<\n");
			ps_printf(psdoc, "  /ImageType 1\n");
			ps_printf(psdoc, "  /Width %d\n", image->width);
			ps_printf(psdoc, "  /Height %d\n", image->height);
			ps_printf(psdoc, "  /BitsPerComponent %d\n", image->bpc);
			switch(image->colorspace) {
				case PS_COLORSPACE_GRAY:
					ps_printf(psdoc, "  /Decode [0 1]\n");
					break;
				case PS_COLORSPACE_RGB:
					ps_printf(psdoc, "  /Decode [0 1 0 1 0 1]\n");
					break;
				case PS_COLORSPACE_CMYK:
					ps_printf(psdoc, "  /Decode [1 0 1 0 1 0 1 0]\n");
					break;
				case PS_COLORSPACE_INDEXED:
					ps_printf(psdoc, "  /Decode [0 %d]\n", (int) pow(2,image->bpc)-1);
					break;
			}
			ps_printf(psdoc, "  /ImageMatrix [%d 0 0 %d 0 %d]\n", image->width, -image->height, image->height);
			if(image->isreusable) {
				ps_printf(psdoc, "  /DataSource %s\n", image->name);
				ps_printf(psdoc, ">> image\n");
			} else {
				ps_printf(psdoc, "  /DataSource currentfile /ASCII%sDecode filter\n", (imgenc == NULL || strcmp(imgenc, "hex") != 0) ? "85" : "Hex");
				ps_printf(psdoc, ">> image\n");
				if(image->components == 4 && image->colorspace == PS_COLORSPACE_RGB) {
					dataptr = image->data;
					reallength = image->height*image->width*3;
					tmpdata = tmpdataptr = psdoc->malloc(psdoc, image->height*image->width*3, _("Allocate memory for temporary image data."));
					for(j=0; j<image->length; j++) {
						*tmpdataptr++ = *dataptr++;
						*tmpdataptr++ = *dataptr++;
						*tmpdataptr++ = *dataptr++;
						dataptr++;
					}
				} else {
					tmpdata = image->data;
					reallength = image->length;
				}
				if(imgenc == NULL || strcmp(imgenc, "hex") != 0)
					ps_ascii85_encode(psdoc, tmpdata, reallength);
				else
					ps_asciihex_encode(psdoc, tmpdata, reallength);
				if(image->components == 4 && image->colorspace == PS_COLORSPACE_RGB)
					psdoc->free(psdoc, tmpdata);
			}
		} else {
			/* Output the image mask */
			if(image->components == 4 && image->colorspace == PS_COLORSPACE_RGB) {
				ps_printf(psdoc, "<<\n");
				ps_printf(psdoc, "  /ImageType 1\n");
				ps_printf(psdoc, "  /Width %d\n", image->width);
				ps_printf(psdoc, "  /Height %d\n", image->height);
				ps_printf(psdoc, "  /BitsPerComponent 1\n");
				ps_printf(psdoc, "  /Decode [0 1]\n");
				ps_printf(psdoc, "  /ImageMatrix [%d 0 0 %d 0 %d]\n", image->width, -image->height, image->height);
				ps_printf(psdoc, "  /DataSource currentfile /ASCII%sDecode filter\n", (imgenc == NULL || strcmp(imgenc, "hex") != 0) ? "85" : "Hex");
				ps_printf(psdoc, ">> imagemask\n");
				dataptr = image->data;
				tmpdata = tmpdataptr = psdoc->malloc(psdoc, (image->height*image->width/8)+1, _("Allocate memory for temporary image data."));
				i = 0;
				k = 0;
				*tmpdataptr = 0;
				for(j=0; j<image->height*image->width; j++) {
					dataptr++; /* skip red */
					dataptr++; /* skip green */
					dataptr++; /* skip blue */
					*tmpdataptr = *tmpdataptr << 1;
					if(*dataptr & 0x80) {
						*tmpdataptr += 1;
					}
					dataptr++;
					i++;
					if(i > 7) {
						tmpdataptr++;
						*tmpdataptr = 0;
						i = 0;
						k++;
					}
				}
				if(i < 8 && i > 0) {
					*tmpdataptr = *tmpdataptr << (8-i);
					k++;
				}
				if(imgenc == NULL || strcmp(imgenc, "hex") != 0)
					ps_ascii85_encode(psdoc, tmpdata, k);
				else
					ps_asciihex_encode(psdoc, tmpdata, k);
				psdoc->free(psdoc, tmpdata);
				ps_printf(psdoc, "\n");
			}
		}

#endif
		ps_printf(psdoc, "\n");
		ps_printf(psdoc, "grestore\n");
	} else if(0 == strncmp(image->type, "eps", 3)) {
		PS_save(psdoc);
		if(image->isreusable) {
			PS_translate(psdoc, x, y);
			PS_scale(psdoc, scale, scale);
			ps_printf(psdoc, "/showpage{}N/erasepage{}N/copypage{}N\n");
			ps_printf(psdoc, "%s 0 setfileposition %s cvx exec\n", image->name, image->name);
		} else {
			ps_printf(psdoc, "/showpage{}N/erasepage{}N/copypage{}N\n");
			PS_translate(psdoc, x, y);
			PS_scale(psdoc, scale, scale);
			ps_write(psdoc, image->data, image->length);
		}
		PS_restore(psdoc);
	} else if(0 == strcmp(image->type, "template")) {
		PS_save(psdoc);
		PS_translate(psdoc, x, y);
		PS_scale(psdoc, scale, scale);
		ps_printf(psdoc, "/%s /Form findresource execform\n", image->name);
//		ps_printf(psdoc, "%s\n", image->name);
		PS_restore(psdoc);
	} else {
		ps_error(psdoc, PS_RuntimeError, _("Images of type '%s' not supported."), image->type);
	}
	return;
}
/* }}} */

/* PS_close_image() {{{
 * Free the memory used by an image
 */
PSLIB_API void PSLIB_CALL
PS_close_image(PSDoc *psdoc, int imageid) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	/*
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return;
	}
	*/

	_ps_unregister_image(psdoc, imageid);
}
/* }}} */

/* PS_begin_template() {{{
 * starts a new template
 */
PSLIB_API int PSLIB_CALL
PS_begin_template(PSDoc *psdoc, float width, float height) {
	PSImage *pstemplate;
	char buffer[20];
	int templateid;

	buffer[0] = '\0';
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	/* If the header is not written, because we are before
	 * the first page, then output the header first.
	 */
	if(psdoc->beginprologwritten == ps_false) {
		ps_write_ps_comments(psdoc);
		ps_write_ps_beginprolog(psdoc);
	}
	if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
		ps_error(psdoc, PS_Warning, _("Calling %s between pages is likely to cause problems when viewing the document. Call it before the first page."), __FUNCTION__);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PROLOG)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' scope."), __FUNCTION__);
		return(0);
	}

	if(NULL == (pstemplate = (PSImage *) psdoc->malloc(psdoc, sizeof(PSImage), _("Allocate memory for template.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for template."));
		return(0);
	}
	memset(pstemplate, 0, sizeof(PSImage));
	if(0 == (templateid = _ps_register_image(psdoc, pstemplate))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register template."));
		psdoc->free(psdoc, pstemplate);
		return(0);
	}

	sprintf(buffer, "template%d", templateid);
	pstemplate->psdoc = psdoc;
	pstemplate->name = ps_strdup(psdoc, buffer);
	pstemplate->type = ps_strdup(psdoc, "template");
	pstemplate->data = NULL;
	pstemplate->width = width;
	pstemplate->height = height;

//	ps_printf(psdoc, "currentglobal true setglobal\n");
	ps_printf(psdoc, "/%s << /FormType 1 ", buffer);
	ps_printf(psdoc, "/BBox [0 0 %f %f] ", width, height);
	ps_printf(psdoc, "/Matrix [1 0 0 1 0 0] ");
	ps_printf(psdoc, "/PaintProc { pop\n");

//	ps_printf(psdoc, "/%s {\n", buffer);
	ps_enter_scope(psdoc, PS_SCOPE_TEMPLATE);

	return(templateid);
}
/* }}} */

/* PS_end_template() {{{
 * ends a template
 */
PSLIB_API void PSLIB_CALL
PS_end_template(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_TEMPLATE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'template' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "} >> /Form defineresource pop %% setglobal\n");
//	ps_printf(psdoc, "} B\n");

	ps_leave_scope(psdoc, PS_SCOPE_TEMPLATE);
}
/* }}} */

/* PS_begin_pattern() {{{
 * starts a new pattern
 */
PSLIB_API int PSLIB_CALL
PS_begin_pattern(PSDoc *psdoc, float width, float height, float xstep, float ystep, int painttype) {
	PSPattern *pspattern;
	char buffer[20];
	int patternid;

	buffer[0] = '\0';
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	if(painttype < 1 || painttype > 2) {
		ps_error(psdoc, PS_RuntimeError, _("Painttype must be 1 or 2."));
		return(0);
	}

	/* If the header is not written, because we are before
	 * the first page, then output the header first.
	 */
	if(psdoc->beginprologwritten == ps_false) {
		ps_write_ps_comments(psdoc);
		ps_write_ps_beginprolog(psdoc);
	}
	if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
		ps_error(psdoc, PS_Warning, _("Calling %s between pages is likely to cause problems when viewing the document. Call it before the first page."), __FUNCTION__);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PROLOG)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' scope."), __FUNCTION__);
		return(0);
	}

	if(NULL == (pspattern = (PSPattern *) psdoc->malloc(psdoc, sizeof(PSPattern), _("Allocate memory for pattern.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for pattern."));
		return(0);
	}
	memset(pspattern, 0, sizeof(PSPattern));
	psdoc->pattern = pspattern;
	if(0 == (patternid = _ps_register_pattern(psdoc, pspattern))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register pattern."));
		psdoc->free(psdoc, pspattern);
		return(0);
	}

	sprintf(buffer, "pattern%d", patternid);
	pspattern->psdoc = psdoc;
	pspattern->name = ps_strdup(psdoc, buffer);
	pspattern->painttype = painttype;
	pspattern->xstep = xstep;
	pspattern->ystep = ystep;
	pspattern->width = width;
	pspattern->height = height;

	ps_printf(psdoc, "<< /PatternType 1 ");
	ps_printf(psdoc, "/BBox [0 0 %f %f] ", width, height);
	ps_printf(psdoc, "/XStep %f ", xstep);
	ps_printf(psdoc, "/YStep %f ", ystep);
	ps_printf(psdoc, "/PaintType %d ", painttype);
	ps_printf(psdoc, "/TilingType 1 ");
	ps_printf(psdoc, "/PaintProc { begin \n");

	ps_enter_scope(psdoc, PS_SCOPE_PATTERN);

	return(patternid);
}
/* }}} */

/* PS_end_pattern() {{{
 * ends a pattern
 */
PSLIB_API void PSLIB_CALL
PS_end_pattern(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PATTERN)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'pattern' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, " end } bind >> matrix makepattern /%s exch def\n", psdoc->pattern->name);

	ps_leave_scope(psdoc, PS_SCOPE_PATTERN);
}
/* }}} */

/* PS_shading_pattern() {{{
 * Creates a shading pattern
 */
PSLIB_API int PSLIB_CALL
PS_shading_pattern(PSDoc *psdoc, int shading, const char *optlist) {
	PSPattern *pspattern;
	char buffer[20];
	int patternid;
	PSShading *psshading;

	buffer[0] = '\0';
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return 0;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return 0;
	}
	psshading = _ps_get_shading(psdoc, shading);
	if(NULL == psshading) {
		ps_error(psdoc, PS_RuntimeError, _("PSShading is null."));
		return 0;
	}
	if(NULL == (pspattern = (PSPattern *) psdoc->malloc(psdoc, sizeof(PSPattern), _("Allocate memory for pattern.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for pattern."));
		return(0);
	}
	memset(pspattern, 0, sizeof(PSPattern));
	psdoc->pattern = pspattern;
	if(0 == (patternid = _ps_register_pattern(psdoc, pspattern))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register pattern."));
		psdoc->free(psdoc, pspattern);
		return(0);
	}

	sprintf(buffer, "pattern%d", patternid);
	pspattern->psdoc = psdoc;
	pspattern->name = ps_strdup(psdoc, buffer);
	pspattern->painttype = 1;

	ps_printf(psdoc, "<< /PatternType 2 ", buffer);
	ps_printf(psdoc, "  /Shading\n", buffer);
	ps_output_shading_dict(psdoc, psshading);
	ps_printf(psdoc, ">> matrix makepattern /%s exch def\n", pspattern->name);
	return(patternid);
}
/* }}} */

/* PS_shading() {{{
 * Creates a shading object
 */
PSLIB_API int PSLIB_CALL
PS_shading(PSDoc *psdoc, const char *shtype, float x0, float y0, float x1, float y1, float c1, float c2, float c3, float c4, const char *optlist) {
	PSShading *psshading;
	int shadingid;
	float optlist_N = 1.0, optlist_r0 = 0.0, optlist_r1 = 0.0;
	ps_bool optlist_extend0 = ps_false, optlist_extend1 = ps_false;
	ps_bool optlist_antialias = 0;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return(0);
	}

	if(psdoc->agstates[psdoc->agstate].fillcolor.colorspace == PS_COLORSPACE_PATTERN) {
		ps_error(psdoc, PS_RuntimeError, _("Current fill color is a pattern which cannot be used for shading."));
		return(0);
	}

	if(psdoc->agstates[psdoc->agstate].fillcolor.colorspace == PS_COLORSPACE_SPOT &&
	  psdoc->agstates[psdoc->agstate].fillcolor.c1 != c1) {
		ps_error(psdoc, PS_RuntimeError, _("The current fill spot color is not the same color as the one set for shading."));
		return(0);
	}
	// FIXME: There must be some checking, if the passed spot color 
	// is in the same as the fill color set before.
	if(NULL == (psshading = (PSShading *) psdoc->malloc(psdoc, sizeof(PSShading), _("Allocate memory for pattern.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for shading."));
		return(0);
	}
	memset(psshading, 0, sizeof(PSShading));

	/* Read the option list only, if it is non empty. */
	if(NULL != optlist && optlist[0] != '\0') {
		ght_hash_table_t *opthash;
		opthash = ps_parse_optlist(psdoc, optlist);
		if(NULL == opthash) {
			ps_error(psdoc, PS_RuntimeError, _("Error while parsing option list."));
			return(0);
		}
		/* Issue a warning only if a value is given but cannot be parsed */
		if(-2 == get_optlist_element_as_float(psdoc, opthash, "N", &optlist_N)) {
			ps_error(psdoc, PS_Warning, _("Value of option list element 'N' could not be read, using default."));
		}
		if(-2 == get_optlist_element_as_bool(psdoc, opthash, "extend0", &optlist_extend0)) {
			ps_error(psdoc, PS_Warning, _("Value of option list element 'extend0' could not be read, using default."));
		}
		if(-2 == get_optlist_element_as_bool(psdoc, opthash, "extend1", &optlist_extend1)) {
			ps_error(psdoc, PS_Warning, _("Value of option list element 'extend1' could not be read, using default."));
		}
		if(-2 == get_optlist_element_as_bool(psdoc, opthash, "antialias", &optlist_antialias)) {
			ps_error(psdoc, PS_Warning, _("Value of option list element 'antialias' could not be read, using default."));
		}
		if(strcmp(shtype, "radial") == 0) {
			if(0 > get_optlist_element_as_float(psdoc, opthash, "r0", &optlist_r0)) {
				ps_error(psdoc, PS_RuntimeError, _("Could not retrieve required parameter 'r0' from option list."));
				return(0);
			}
			if(0 > get_optlist_element_as_float(psdoc, opthash, "r1", &optlist_r1)) {
				ps_error(psdoc, PS_RuntimeError, _("Could not retrieve required parameter 'r1' from option list."));
				return(0);
			}
		}
		ps_free_optlist(psdoc, opthash);
	} else if(strcmp(shtype, "radial") == 0) {
		ps_error(psdoc, PS_RuntimeError, _("If type of shading is 'radial' the parameters 'r0' and 'r1' must be set in the option list."));
		return(0);
	}

	if(strcmp(shtype, "axial") == 0) {
		psshading->type = 2;
	} else if(strcmp(shtype, "radial") == 0) {
		psshading->type = 3;
	} else {
		ps_error(psdoc, PS_RuntimeError, _("Type of shading must be 'radial' or 'axial'."));
		return(0);
	}
	psshading->x0 = x0;
	psshading->y0 = y0;
	psshading->x1 = x1;
	psshading->y1 = y1;
	psshading->r0 = optlist_r0;
	psshading->r1 = optlist_r1;
	psshading->N = optlist_N;
	psshading->extend0 = optlist_extend0;
	psshading->extend1 = optlist_extend1;
	psshading->antialias = optlist_antialias;
	psshading->startcolor.colorspace = psdoc->agstates[psdoc->agstate].fillcolor.colorspace;
	psshading->startcolor.prevcolorspace = psdoc->agstates[psdoc->agstate].fillcolor.prevcolorspace;
	psshading->startcolor.pattern = psdoc->agstates[psdoc->agstate].fillcolor.pattern;
	psshading->startcolor.c1 = psdoc->agstates[psdoc->agstate].fillcolor.c1;
	psshading->startcolor.c2 = psdoc->agstates[psdoc->agstate].fillcolor.c2;
	psshading->startcolor.c3 = psdoc->agstates[psdoc->agstate].fillcolor.c3;
	psshading->startcolor.c4 = psdoc->agstates[psdoc->agstate].fillcolor.c4;
	psshading->endcolor.colorspace = psshading->startcolor.colorspace;
	psshading->endcolor.prevcolorspace = 0;
	psshading->endcolor.pattern = 0;
	psshading->endcolor.c1 = c1;
	psshading->endcolor.c2 = c2;
	psshading->endcolor.c3 = c3;
	psshading->endcolor.c4 = c4;

	if(0 == (shadingid = _ps_register_shading(psdoc, psshading))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register shading."));
		psdoc->free(psdoc, psshading);
		return(0);
	}
	return(shadingid);
}
/* }}} */

/* PS_begin_font() {{{
 * starts a new font
 */
PSLIB_API int PSLIB_CALL
PS_begin_font(PSDoc *psdoc, const char *fontname, int reserverd, double a, double b, double c, double d, double e, double f, const char *optlist) {
	PSFont *psfont;
	ENCODING *fontenc;
	ADOBEFONTMETRIC *metrics;
	char buffer[20];
	int fontid, i;

	buffer[0] = '\0';
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}

	/* If the header is not written, because we are before
	 * the first page, then output the header first.
	 */
	if(psdoc->beginprologwritten == ps_false) {
		ps_write_ps_comments(psdoc);
		ps_write_ps_beginprolog(psdoc);
	}
	if(ps_check_scope(psdoc, PS_SCOPE_DOCUMENT)) {
		ps_error(psdoc, PS_Warning, _("Calling %s between pages is likely to cause problems when viewing the document. Call it before the first page."), __FUNCTION__);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PROLOG)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' scope."), __FUNCTION__);
		return(0);
	}

	if(NULL == (psfont = (PSFont *) psdoc->malloc(psdoc, sizeof(PSFont), _("Allocate memory for font.")))) {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for font."));
		return(0);
	}
	memset(psfont, 0, sizeof(PSFont));

	if(NULL == (metrics = (ADOBEFONTMETRIC *) psdoc->malloc(psdoc, sizeof(ADOBEFONTMETRIC), _("Allocate space for font metric.")))) {
		return(0);
	}
	memset(metrics, 0, sizeof(ADOBEFONTMETRIC));
	metrics->codingscheme = ps_strdup(psdoc, "FontSpecific");
	metrics->fontname = ps_strdup(psdoc, fontname);
	metrics->fontenc = NULL;
	metrics->gadobechars = ght_create(512);
	ght_set_alloc(metrics->gadobechars, ps_ght_malloc, ps_ght_free, psdoc);
	readencoding(psdoc, metrics, NULL);
	psfont->encoding = ps_strdup(psdoc, "default");
	psfont->metrics = metrics;
	psdoc->font = psfont;

	if(0 == (fontid = _ps_register_font(psdoc, psfont))) {
		ps_error(psdoc, PS_MemoryError, _("Could not register font."));
		psdoc->free(psdoc, psfont);
		return(0);
	}

	psfont->psdoc = psdoc;

	ps_printf(psdoc, "8 dict begin\n");
	ps_printf(psdoc, "  /FontType 3 def\n");
	ps_printf(psdoc, "  /FontMatrix [%f %f %f %f %f %f] def\n", a, b, c, d, e, f);
//	ps_printf(psdoc, "  /FontName %s \n", fontname);
	ps_printf(psdoc, "  /FontBBox [0 0 750 750] def\n");
	ps_printf(psdoc, "  /Encoding 256 array def 0 1 255 {Encoding exch /.notdef put} for\n");
	fontenc = &fontencoding[0];
	for(i=0; i<255; i++) {
		if((fontenc->vec[i] != NULL) && (*(fontenc->vec[i]) != '\0')) {
				ps_printf(psdoc, "  Encoding %d /%s put\n", i, fontenc->vec[i]);
		}
	}
	ps_printf(psdoc, "  /BuildGlyph\n");
	ps_printf(psdoc, "    { %%1000 0\n");
	ps_printf(psdoc, "      %%0 0 750 750\n");
	ps_printf(psdoc, "      %%setcachedevice\n");
	ps_printf(psdoc, "      exch /CharProcs get exch\n");
	ps_printf(psdoc, "      2 copy known not { pop /.notdef} if\n");
	ps_printf(psdoc, "      get exec\n");
	ps_printf(psdoc, "    } bind def\n");
	ps_printf(psdoc, "  /BuildChar\n");
	ps_printf(psdoc, "    { 1 index /Encoding get exch get\n");
	ps_printf(psdoc, "      1 index /BuildGlyph get exec\n");
	ps_printf(psdoc, "    } bind def\n");
//	ps_printf(psdoc, "  currentdict\n");
	ps_printf(psdoc, "  /CharProcs 255 dict def\n");
	ps_printf(psdoc, "    CharProcs begin\n");
	ps_printf(psdoc, "      /.notdef { } def\n");

	ps_enter_scope(psdoc, PS_SCOPE_FONT);

	return(fontid);
}
/* }}} */

/* PS_end_font() {{{
 * ends a font
 */
PSLIB_API void PSLIB_CALL
PS_end_font(PSDoc *psdoc) {
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_FONT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'font' scope."), __FUNCTION__);
		return;
	}

	psfont = psdoc->font;
	ps_printf(psdoc, "    end\n");
	ps_printf(psdoc, "  currentdict\n");
	ps_printf(psdoc, "end\n");
	ps_printf(psdoc, "/%s exch definefont pop\n", psfont->metrics->fontname);

	ps_leave_scope(psdoc, PS_SCOPE_FONT);
}
/* }}} */

/* PS_begin_glyph() {{{
 * starts a new glyph
 */
PSLIB_API int PSLIB_CALL
PS_begin_glyph(PSDoc *psdoc, const char *glyphname, double wx, double llx, double lly, double urx, double ury) {
	ADOBEINFO *ai;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return 0;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_FONT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'font' scope."), __FUNCTION__);
		return 0;
	}

	psfont = psdoc->font;
	if(ai = gfindadobe(psfont->metrics->gadobechars, glyphname)) {
		ps_error(psdoc, PS_RuntimeError, _("Font already contains glyph with name '%s'."), glyphname);
		return 0;
	}

	ai = (ADOBEINFO *) psdoc->malloc(psdoc, (unsigned long) sizeof(ADOBEINFO), "newchar: allocate memory for new characters") ;
	ai->adobenum = -1 ;
	ai->texnum = -1 ;
	ai->adobename = ps_strdup(psdoc, glyphname);
	ai->llx = llx;
	ai->lly = lly;
	ai->urx = urx;
	ai->ury = ury;
	ai->lkern = 0;
	ai->rkern = 0;
	ai->ligs = NULL ;
	ai->kerns = NULL ;
	ai->kern_equivs = NULL ;
	ai->pccs = NULL ;
	ai->width = wx;
	ght_insert(psfont->metrics->gadobechars, ai, strlen(glyphname)+1, (void *) glyphname);

	ps_printf(psdoc, "      /%s {\n", glyphname);
	ps_printf(psdoc, "      %.4f 0 %.4f %.4f %.4f %.4f setcachedevice\n", (float) wx, (float) llx, (float) lly, (float) urx, (float) ury);
	ps_enter_scope(psdoc, PS_SCOPE_GLYPH);
	return 1;
}
/* }}} */

/* PS_end_glyph() {{{
 * ends a glyph
 */
PSLIB_API void PSLIB_CALL
PS_end_glyph(PSDoc *psdoc) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_GLYPH)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'glyph' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "      } bind def\n");

	ps_leave_scope(psdoc, PS_SCOPE_GLYPH);
}
/* }}} */

/* PS_add_kerning() {{{
 * adds kerning for pair of glyphs
 */
PSLIB_API void PSLIB_CALL
PS_add_kerning(PSDoc *psdoc, int fontid, const char *glyphname1, const char *glyphname2, int kern) {
	ADOBEINFO *ai1, *ai2;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_FONT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'font' scope."), __FUNCTION__);
		return;
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return;
	}

	ai1 = gfindadobe(psfont->metrics->gadobechars, glyphname1);
	if(!ai1) {
		ps_error(psdoc, PS_RuntimeError, _("First glyph '%s' of kerning pair does not exist in font."), glyphname1);
		return;
	}
	ai2 = gfindadobe(psfont->metrics->gadobechars, glyphname2);
	if(!ai2) {
		ps_error(psdoc, PS_RuntimeError, _("Second glyph '%s' of kerning pair does not exist in font."), glyphname2);
		return;
	}
	if(calculatekern(ai1, ai2) != 0) {
		ps_error(psdoc, PS_RuntimeError, _("Kerning pair (%s, %s) already exists in font."), glyphname1, glyphname2);
	}
	addkern(psdoc, ai1, ai2, kern);
}
/* }}} */

/* PS_add_ligature() {{{
 * adds ligature for pair of glyphs
 */
PSLIB_API void PSLIB_CALL
PS_add_ligature(PSDoc *psdoc, int fontid, const char *glyphname1, const char *glyphname2, const char *glyphname3) {
	ADOBEINFO *ai1, *ai2, *ai3;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_FONT)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'font' scope."), __FUNCTION__);
		return;
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return;
	}

	ai1 = gfindadobe(psfont->metrics->gadobechars, glyphname1);
	if(!ai1) {
		ps_error(psdoc, PS_RuntimeError, _("First glyph '%s' of ligature does not exist in font."), glyphname1);
		return;
	}
	ai2 = gfindadobe(psfont->metrics->gadobechars, glyphname2);
	if(!ai2) {
		ps_error(psdoc, PS_RuntimeError, _("Successor glyph '%s' of ligature does not exist in font."), glyphname2);
		return;
	}
	ai3 = gfindadobe(psfont->metrics->gadobechars, glyphname3);
	if(!ai3) {
		ps_error(psdoc, PS_RuntimeError, _("Substitute glyph '%s' of ligature does not exist in font."), glyphname3);
		return;
	}
	addligature(psdoc, ai1, ai2, ai3);
}
/* }}} */

/* PS_get_buffer() {{{
 * Returns a pointer to the current buffer and sets its size to 0
 */
PSLIB_API const char * PSLIB_CALL
PS_get_buffer(PSDoc *psdoc, long *size) {
	const char *tmp;
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(NULL);
	}
	if(NULL == psdoc->sb) {
		*size = 0;
		return(NULL);
	}
	*size = str_buffer_len(psdoc, psdoc->sb);	
	tmp = str_buffer_get(psdoc, psdoc->sb);
	str_buffer_clear(psdoc, psdoc->sb);
	return(tmp);
}
/* }}} */

/* PS_set_border_style() {{{
 * Sets style of border for link destination
 */
PSLIB_API void PSLIB_CALL
PS_set_border_style(PSDoc *psdoc, const char *style, float width) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return;
	}

	psdoc->border_width = width;
	if(0 == strcmp(style, "solid"))
		psdoc->border_style = PS_BORDER_SOLID;
	else if(0 == strcmp(style, "dashed")) {
		psdoc->border_style = PS_BORDER_DASHED;
		psdoc->border_white = psdoc->border_black = 3.0;
	} else
		ps_error(psdoc, PS_RuntimeError, _("Parameter style of PS_set_border_style() must be 'solid' or 'dashed'\n"));
}
/* }}} */

/* PS_set_border_color() {{{
 * Sets color of border for link destination
 */
PSLIB_API void PSLIB_CALL
PS_set_border_color(PSDoc *psdoc, float red, float green, float blue) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return;
	}

	psdoc->border_red = red;
	psdoc->border_green = green;
	psdoc->border_blue = blue;
}
/* }}} */

/* PS_set_border_dash() {{{
 * Sets dash of border for link destination
 */
PSLIB_API void PSLIB_CALL
PS_set_border_dash(PSDoc *psdoc, float black, float white) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_DOCUMENT|PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'document' or 'page' scope."), __FUNCTION__);
		return;
	}

	psdoc->border_black = black;
	psdoc->border_white = white;
}
/* }}} */

/* _ps_output_anno_border() {{{
 *
 */
void static
_ps_output_anno_border(PSDoc *psdoc) {
	switch(psdoc->border_style) {
		case PS_BORDER_SOLID:
			ps_printf(psdoc, "/Border [ %f 1 1 ] ", psdoc->border_width);
			break;
		case PS_BORDER_DASHED:
			ps_printf(psdoc, "/Border [ %f %f %f ] ", psdoc->border_black, psdoc->border_white, psdoc->border_width);
			break;
	}
	ps_printf(psdoc, "/Color [ %f %f %f ] ", psdoc->border_red, psdoc->border_green, psdoc->border_blue);
}
/* }}} */

/* PS_add_weblink() {{{
 * Adds a link to an external web page
 */
PSLIB_API void PSLIB_CALL
PS_add_weblink(PSDoc *psdoc, float llx, float lly, float urx, float ury, const char *url) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "[ /Rect [ %f %f %f %f] ", llx, lly, urx, ury);
	_ps_output_anno_border(psdoc);
	ps_printf(psdoc, "/Action << /Subtype /URI /URI (%s) >> /Subtype /Link /ANN pdfmark\n", url);
}
/* }}} */

/* PS_add_pdflink() {{{
 * Adds a link to external pdf document
 */
PSLIB_API void PSLIB_CALL
PS_add_pdflink(PSDoc *psdoc, float llx, float lly, float urx, float ury, const char *filename, int page, const char *dest) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "[ /Rect [ %f %f %f %f] ", llx, lly, urx, ury);
	_ps_output_anno_border(psdoc);
	ps_printf(psdoc, "/Page %d ", page);
	if(0 == strcmp(dest, "fitpage"))
		ps_printf(psdoc, "/View %s ", "[ /Fit ]");
	else if(0 == strcmp(dest, "fitwidth"))
		ps_printf(psdoc, "/View %s ", "[ /FitH -32768 ]");
	else if(0 == strcmp(dest, "fitheight"))
		ps_printf(psdoc, "/View %s ", "[ /FitV -32768 ]");
	else if(0 == strcmp(dest, "fitbbox"))
		ps_printf(psdoc, "/View %s ", "/FitB");
	else if(0 != strcmp(dest, "retain"))
		ps_error(psdoc, PS_RuntimeError, _("Parameter dest of PS_add_pdflink() must be 'fitpage', 'fitwidth', 'fitheight', 'fitbbox', 'retain'."));
	ps_printf(psdoc, "/Action /GoToR /File (%s) /Subtype /Link /ANN pdfmark\n", filename);
}
/* }}} */

/* PS_add_locallink() {{{
 * Adds a link within the pdf document
 */
PSLIB_API void PSLIB_CALL
PS_add_locallink(PSDoc *psdoc, float llx, float lly, float urx, float ury, const int page, const char *dest) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "[ /Rect [ %f %f %f %f] ", llx, lly, urx, ury);
	_ps_output_anno_border(psdoc);
	if(page == PS_GOTO_NEXT_PAGE)
		ps_printf(psdoc, "/Page /Next ");
	else if(page == PS_GOTO_PREV_PAGE)
		ps_printf(psdoc, "/Page /Prev ");
	else
		ps_printf(psdoc, "/Page %d ", page);
	if(0 == strcmp(dest, "fitpage"))
		ps_printf(psdoc, "/View %s ", "[ /Fit ]");
	else if(0 == strcmp(dest, "fitwidth"))
		ps_printf(psdoc, "/View %s ", "[ /FitH -32768 ]");
	else if(0 == strcmp(dest, "fitheight"))
		ps_printf(psdoc, "/View %s ", "[ /FitV -32768 ]");
	else if(0 == strcmp(dest, "fitbbox"))
		ps_printf(psdoc, "/View %s ", "/FitB");
	else if(0 != strcmp(dest, "retain"))
		ps_error(psdoc, PS_RuntimeError, _("Parameter dest of PS_add_locallink() must be 'fitpage', 'fitwidth', 'fitheight', 'fitbbox', 'retain'."));
	ps_printf(psdoc, "/Subtype /Link /ANN pdfmark\n");
}
/* }}} */

/* PS_add_launchlink() {{{
 * Adds a link to an external program or file
 */
PSLIB_API void PSLIB_CALL
PS_add_launchlink(PSDoc *psdoc, float llx, float lly, float urx, float ury, const char *filename) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "[ /Rect [ %f %f %f %f] ", llx, lly, urx, ury);
	_ps_output_anno_border(psdoc);
	ps_printf(psdoc, "/Action << /S /Launch /F (%s) >> /Subtype /Link /ANN pdfmark\n", filename);
}
/* }}} */

/* PS_add_bookmark() {{{
 * Adds a bookmark (nested bookmarks are not yet support)
 */
PSLIB_API int PSLIB_CALL
PS_add_bookmark(PSDoc *psdoc, const char *text, int parent, int open) {
	PS_BOOKMARK *bookmark, *parentbookmark;
	DLIST *bl;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return(0);
	}

	if(parent < 0 || parent > psdoc->lastbookmarkid) {
		ps_error(psdoc, PS_RuntimeError, _("Parent bookmark ist out of possible range."));
		return(0);
	}
	if(parent == 0) { /* Add bookmark on first level */
		bl = psdoc->bookmarks;
	} else {
		parentbookmark = psdoc->bookmarkdict[parent-1];
		bl = parentbookmark->children;
	}
	bookmark = (PS_BOOKMARK *) dlst_newnode(bl, sizeof(PS_BOOKMARK));
	if(NULL == bookmark)  {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for new bookmark."));
		return(0);
	}
	bookmark->page = psdoc->page;
	bookmark->text = ps_strdup(psdoc, text);
	bookmark->open = open;
	if(psdoc->bookmarkcnt <= psdoc->lastbookmarkid) {
		psdoc->bookmarkcnt += 20;
		if(NULL == (psdoc->bookmarkdict = psdoc->realloc(psdoc, psdoc->bookmarkdict, psdoc->bookmarkcnt*sizeof(PS_BOOKMARK *), _("Allocate memory for new bookmark lookup table.")))) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for larger bookmark lookup table."));
			psdoc->bookmarkcnt -= 20;
			psdoc->free(psdoc, bookmark->text);
			dlst_freenode(bl, bookmark);
			return(0);
		}
	}
	psdoc->bookmarkdict[psdoc->lastbookmarkid] = bookmark;
	if(NULL == (bookmark->children = dlst_init(psdoc->malloc, psdoc->realloc, psdoc->free))) {
		ps_error(psdoc, PS_RuntimeError, _("Could not initialize bookmark list of new bookmark."));
		psdoc->free(psdoc, bookmark->text);
		dlst_freenode(bl, bookmark);
		return(0);
	}
	psdoc->lastbookmarkid++;
	bookmark->id = psdoc->lastbookmarkid;
	dlst_insertafter(bl, bookmark, PS_DLST_HEAD(bl));
	return(bookmark->id);
}
/* }}} */

/* PS_add_note() {{{
 * Adds a note
 */
PSLIB_API void PSLIB_CALL
PS_add_note(PSDoc *psdoc, float llx, float lly, float urx, float ury, const char *contents, const char *title, const char *icon, int open) {
	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}

	ps_printf(psdoc, "[ /Rect [ %f %f %f %f] ", llx, lly, urx, ury);
	_ps_output_anno_border(psdoc);
	if(open)
		ps_printf(psdoc, "/Open true ");
	if(0 == strcmp(icon, "comment"))
		ps_printf(psdoc, "/Name /Comment ");
	else if(0 == strcmp(icon, "insert"))
		ps_printf(psdoc, "/Name /Insert ");
	else if(0 == strcmp(icon, "note"))
		ps_printf(psdoc, "/Name /Note ");
	else if(0 == strcmp(icon, "paragraph"))
		ps_printf(psdoc, "/Name /Paragraph ");
	else if(0 == strcmp(icon, "newparagraph"))
		ps_printf(psdoc, "/Name /Newparagraph ");
	else if(0 == strcmp(icon, "key"))
		ps_printf(psdoc, "/Name /Key ");
	else if(0 == strcmp(icon, "help"))
		ps_printf(psdoc, "/Name /Help ");
	ps_printf(psdoc, "/Title (%s) /Contents (%s) /ANN pdfmark\n", title,  contents);
}
/* }}} */

/* PS_hyphenate() {{{
 * hyphenate a word
 */
PSLIB_API int PSLIB_CALL
PS_hyphenate(PSDoc *psdoc, const char *text, char **hyphens) {
	char *hyphentext;
	int hyphenminchars;

	*hyphens[0] = '\0';
	if(!psdoc->hdict) {
		ps_error(psdoc, PS_Warning, _("No hyphenation table set."));
		return -1;
	} else {
		if(0 == (hyphenminchars = (int) PS_get_value(psdoc, "hyphenminchars", 0)))
			hyphenminchars = 3;
	}

	hyphentext = ps_strdup(psdoc, text);
	if(NULL != hyphentext) {
		int k;
		k = 0;
		while(hyphentext[k] && !isalpha(hyphentext[k]))
			k++;
		if(strlen(hyphentext)-k > 2*hyphenminchars) {
			char *buffer;
			buffer = (char*) psdoc->malloc(psdoc, sizeof(char) * (strlen(hyphentext)+3), _("Could not allocate memory for hyphenation buffer."));
			hnj_hyphen_hyphenate(psdoc->hdict, &hyphentext[k], strlen(&hyphentext[k]), buffer);
			memset(*hyphens, '0', k);
			memcpy(*hyphens+k, buffer, strlen(hyphentext)+1);
			psdoc->free(psdoc, buffer);
		} else {
			return -1;
		}
		psdoc->free(psdoc, hyphentext);
		return 0;
	} else {
		return -1;
	}
}
/* }}} */

/* Extra functions */

/* PS_symbol() {{{
 * Output symbol without taking input encoding into account
 */
PSLIB_API void PSLIB_CALL
PS_symbol(PSDoc *psdoc, unsigned char c) {
	char text[2];
	ENCODING *fontenc;
	ADOBEINFO *ai;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}
	fontenc = ps_build_enc_vector(psdoc, psdoc->font->metrics->fontenc);
	if(NULL == fontenc) {
		ps_error(psdoc, PS_RuntimeError, _("Could not build font encoding vector."));
		return;
	}
	ai = gfindadobe(psdoc->font->metrics->gadobechars, fontenc->vec[c]);
	if(ai) {
		text[0] = c;
		text[1] = '\0';
		ps_printf(psdoc, "%.2f %.2f a\n", psdoc->tstates[psdoc->tstate].tx, psdoc->tstates[psdoc->tstate].ty);
		ps_render_text(psdoc, text);
		psdoc->tstates[psdoc->tstate].tx += ai->width*psdoc->font->size/1000.0;
	}
	ps_free_enc_vector(psdoc, fontenc);

}
/* }}} */

/* PS_glyph_show() {{{
 * Output symbol by its name
 */
PSLIB_API void PSLIB_CALL
PS_glyph_show(PSDoc *psdoc, const char *name) {
	ADOBEINFO *ai;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PAGE)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'page' scope."), __FUNCTION__);
		return;
	}
	ai = gfindadobe(psdoc->font->metrics->gadobechars, name);
	if(ai) {
		ps_printf(psdoc, "%.2f %.2f a\n", psdoc->tstates[psdoc->tstate].tx, psdoc->tstates[psdoc->tstate].ty);
		ps_printf(psdoc, "/%s glyphshow\n", name);
		psdoc->tstates[psdoc->tstate].tx += ai->width*psdoc->font->size/1000.0;
	} else {
		ps_error(psdoc, PS_RuntimeError, _("glyph '%s' is not available in current font."), __FUNCTION__);
	}
}
/* }}} */

/* PS_glyph_width() {{{
 * Return width of a glyph
 */
PSLIB_API float PSLIB_CALL
PS_glyph_width(PSDoc *psdoc, const char *glyphname, int fontid, float size) {
	ADOBEINFO *ai;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0.0);
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return(0.0);
	}

	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("No font available."));
		return(0.0);
	}

	if(NULL == psfont->metrics) {
		ps_error(psdoc, PS_RuntimeError, _("No font metrics available. Cannot calculate width of string."));
		return(0.0);
	}

	if(0.0 == size)
		size = psfont->size;

//	fprintf(stderr, "search for %s\n", glyphname);
	ai = gfindadobe(psfont->metrics->gadobechars, glyphname);
	if(ai) {
//		fprintf(stderr, "width = %d, size = %f\n", ai->width, size);
		return(ai->width*size/1000.0);
	} else {
		return(0.0);
	}
}
/* }}} */

/* PS_glyph_list() {{{
 * Returns list of all glyph names in the font. The list of glyphs must
 * be freed with PS_free_glyph_list().
 */
PSLIB_API char** PSLIB_CALL
PS_glyph_list(PSDoc *psdoc, int fontid, char ***charlist, int *len) {
	ght_iterator_t iterator;
	char *p_key;
	ADOBEINFO *p_e;
	PSFont *psfont;
	int i;
	char **tmp;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return NULL;
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return NULL;
	}

	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("No font available."));
		return NULL;
	}

	if(psfont->metrics->gadobechars) {
		*len = ght_size(psfont->metrics->gadobechars);
		if(NULL == (tmp = psdoc->malloc(psdoc, *len * sizeof(char **), _("Allocate memory for list of glyph names.")))) {
		ps_error(psdoc, PS_RuntimeError, _("Could not allocate memory for list of glyph names."));
			return NULL;
		}
		i = 0;
		for(p_e = ght_first(psfont->metrics->gadobechars, &iterator, (void **) &p_key); p_e; p_e = ght_next(psfont->metrics->gadobechars, &iterator, (void **) &p_key)) {
			tmp[i++] = ps_strdup(psdoc, p_e->adobename);
		}
		*charlist = tmp;
		return tmp;
	} else {
		ps_error(psdoc, PS_RuntimeError, _("Font does not have list of glyphs."));
		return NULL;
	}
}
/* }}} */

/* PS_free_glyph_list() {{{
 * Frees the memory allocated for a glyph list returned by PS_glyph_list().
 */
PSLIB_API void PSLIB_CALL
PS_free_glyph_list(PSDoc *psdoc, char **charlist, int len) {
	int i;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	for(i=0; i<len; i++) {
		if(charlist[i])
			psdoc->free(psdoc, charlist[i]);
	}
	psdoc->free(psdoc, charlist);
}
/* }}} */

/* PS_symbol_width() {{{
 * Return width of a symbol without taking input encoding into account
 */
PSLIB_API float PSLIB_CALL
PS_symbol_width(PSDoc *psdoc, unsigned char c, int fontid, float size) {
	ENCODING *fontenc;
	ADOBEINFO *ai;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return(0.0);
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return(0.0);
	}

	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("No font available."));
		return(0.0);
	}

	if(NULL == psfont->metrics) {
		ps_error(psdoc, PS_RuntimeError, _("No font metrics available. Cannot calculate width of string."));
		return(0.0);
	}

	if(0.0 == size)
		size = psfont->size;

	fontenc = ps_build_enc_vector(psdoc, psfont->metrics->fontenc);
	if(NULL == fontenc) {
		ps_error(psdoc, PS_RuntimeError, _("Could not build font encoding vector."));
		return(0.0);
	}
	ai = gfindadobe(psfont->metrics->gadobechars, fontenc->vec[c]);
	ps_free_enc_vector(psdoc, fontenc);
	if(ai) {
		return(ai->width*size/1000.0);
	} else {
		return(0.0);
	}
}
/* }}} */

/* PS_symbol_name() {{{
 * Returns name of symbol
 */
PSLIB_API void PSLIB_CALL
PS_symbol_name(PSDoc *psdoc, unsigned char c, int fontid, char *name, int size) {
	ENCODING *fontenc;
	PSFont *psfont;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return;
	}

	if(0 == fontid)
		psfont = psdoc->font;
	else {
		if(NULL == (psfont = _ps_get_font(psdoc, fontid)))
			return;
	}

	if(NULL == psfont) {
		ps_error(psdoc, PS_RuntimeError, _("No font available."));
		return;
	}

	if(NULL == psfont->metrics) {
		ps_error(psdoc, PS_RuntimeError, _("No font metrics available. Cannot lookup symbol name."));
		return;
	}

	fontenc = ps_build_enc_vector(psdoc, psfont->metrics->fontenc);
	if(fontenc) {
		if(fontenc->vec[c]) {
			strncpy(name, fontenc->vec[c], size);
		} else {
			name[0] = '\0';
		}
		ps_free_enc_vector(psdoc, fontenc);
	} else {
		name[0] = '\0';
	}
	return;

}
/* }}} */

/* PS_include_file() {{{
 * Includes a file into the output file
 */
PSLIB_API int PSLIB_CALL
PS_include_file(PSDoc *psdoc, const char *filename) {
	unsigned char *bb;
	FILE *fp;
	long fsize;

	if(NULL == psdoc) {
		ps_error(psdoc, PS_RuntimeError, _("PSDoc is null."));
		return -1;
	}
	/* If the header is not written, because we are before
	 * the first page, then output the header first.
	 */
	if(psdoc->beginprologwritten == ps_false) {
		ps_write_ps_comments(psdoc);
		ps_write_ps_beginprolog(psdoc);
	}
	if(!ps_check_scope(psdoc, PS_SCOPE_PROLOG)) {
		ps_error(psdoc, PS_RuntimeError, _("%s must be called within 'prolog' scope."), __FUNCTION__);
		return -1;
	}

	if(NULL == filename || filename[0] == '\0') {
		ps_error(psdoc, PS_IOError, _("Cannot include file without a name."));
		return -1;
	}
	
	if(NULL == (fp = ps_open_file_in_path(psdoc, filename))) {
		ps_error(psdoc, PS_IOError, _("Could not open include file '%s'."), filename);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	if(fsize <= 0) {
		ps_error(psdoc, PS_Warning, _("Include file '%s' is empty"), filename);
		fclose(fp);
		return 0;
	}
	fseek(fp, 0, SEEK_SET);
	if(NULL != (bb = malloc(fsize))) {
		fread(bb, fsize, 1, fp);
		ps_printf(psdoc, "PslibDict begin\n");
		ps_write(psdoc, bb, fsize);
		ps_printf(psdoc, "end\n");
		free(bb);
	} else {
		ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for include file '%s'"), filename);
		return -1;
	}
	fclose(fp);
	return 0;
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
