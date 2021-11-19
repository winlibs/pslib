/*
 *    (c) Copyright 2003-2004  Uwe Steinmann.
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

#ifndef __PS_INTERN_H__
#define __PS_INTERN_H__

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(a) dgettext(GETTEXT_PACKAGE, a)
#else
#define _(a) a
#endif

#include <locale.h>
#include "libps/pslib.h"
#include "libps/psversion.h"
#include "ps_list.h"
#include "ps_strbuf.h"
#include "ght_hash_table.h"
#ifdef HAVE_LIBPNJ
#include "libhnj/hyphen.h"
#else
#include "hyphen.h"
#endif

#ifndef WIN32
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

#define PS_BORDER_UNSET  0
#define PS_BORDER_SOLID  1
#define PS_BORDER_DASHED 2

/* horizontal modes for PS_show_boxed() */
#define PS_TEXT_HMODE_JUSTIFY 1
#define PS_TEXT_HMODE_LEFT 2
#define PS_TEXT_HMODE_RIGHT 3
#define PS_TEXT_HMODE_CENTER 4
#define PS_TEXT_HMODE_FULLJUSTIFY 5

typedef struct encoding_ ENCODING;

typedef struct lig_ LIG;

typedef struct kern_ KERN;

typedef struct adobeptr_ ADOBEPTR;

typedef struct pcc_ PCC;

typedef struct adobeinfo_ ADOBEINFO;

typedef struct adobefontmetric_ ADOBEFONTMETRIC;

typedef union _optvalue_value {
	long lval;      /* long value */
	double dval;    /* double value */
	struct {
		char *val;
		int len;
	} str;
	ght_hash_table_t *ht;        /* hash table value */
} optvalue_value;

typedef struct _optval_struct optval;

struct _optval_struct {
	optvalue_value value;
	char type;
};

struct lig_ {
   struct lig_ *next ;
   char *succ, *sub ;
   short op, boundleft ;
};

struct encoding_ {
	char *name ;
	char *vec[256] ;
};

struct kern_ {
   struct kern_ *next ;
   char *succ ;
   int delta ;
};

struct adobeptr_ {
	 struct adobeptr_ *next;
	 struct adobeinfo_ *ch;
};

struct pcc_ {
   struct pcc_ *next ;
   char * partname ;
   int xoffset, yoffset ;
};

struct adobeinfo_ {
   int adobenum, texnum, width ;
   char *adobename ;
   int llx, lly, urx, ury ;
   LIG *ligs ;
   KERN *kerns ;
   ADOBEPTR *kern_equivs ;
   PCC *pccs ;
	 int lkern, rkern;  /* Kerning on left and right margin */
};

struct adobefontmetric_ {
	ght_hash_table_t *gadobechars;
	FILE *afmin;        /* File for encoding */
	char *fontname; // = "Unknown" ;
	char *codingscheme; // = "Unspecified" ;
	ght_hash_table_t *fontenc;
#ifdef VMCMS
	char *ebfontname ;
	char *ebcodingscheme ;
#endif
	float italicangle; // = 0.0 ;
	float underlineposition;
	float underlinethickness;
	float ascender;
	float descender;
	char fixedpitch ;
	char makevpl ;
	char pedantic ;
	int xheight; // = 400 ;
	int fontspace ;
	int bc, ec ;
	long cksum ;
	float efactor; // = 1.0;
	float slant; // = 0.0 ;
	float capheight; // = 0.8 ;
	char *efactorparam, *slantparam ;
	double newslant ;
};

typedef struct PSColor_ PSColor;

typedef struct PSImage_ PSImage;

struct PSImage_ {
	PSDoc *psdoc; // Doc for which this font was created
	char *name;   // currently only used for templates
	char *type;
	char *data;
	long length;
	int width;
	int height;
	int components;
	int bpc;
	int colorspace;
	ps_bool ismask;
	PSColor *palette;
	int numcolors;
	PSImage *imagemask; // Use other image a s mask for this image
	ps_bool isreusable;
};

typedef struct PSFont_ PSFont;

struct PSFont_ {
	PSDoc *psdoc; // Doc for which this font was created
	char *name;   // currently not used
	float size;
	int wordspace;
	char *encoding;
	ADOBEFONTMETRIC *metrics;
};

typedef struct PSPattern_ PSPattern;

struct PSPattern_ {
	PSDoc *psdoc; // Doc for which this font was created
	char *name;
	int painttype;
	int shading;
	float width;
	float height;
	float xstep;
	float ystep;
};

#define PS_COLORSPACE_GRAY    1
#define PS_COLORSPACE_RGB     2
#define PS_COLORSPACE_CMYK    3
#define PS_COLORSPACE_SPOT    4
#define PS_COLORSPACE_PATTERN 5
#define PS_COLORSPACE_INDEXED 6

#define PS_COLORTYPE_FILL   1
#define PS_COLORTYPE_STROKE 2

typedef struct PSSpotColor_ PSSpotColor;

struct PSSpotColor_ {
	PSDoc *psdoc; // Doc for which this font was created
	char *name;
	int colorspace;
	float c1;
	float c2;
	float c3;
	float c4;
};

struct PSColor_ {
	int colorspace;
	int prevcolorspace;  // The last colorspace used for pattern of type 1
	int pattern;         // The pattern id
	float c1;
	float c2;
	float c3;
	float c4;
};

typedef struct PSShading_ PSShading;

struct PSShading_ {
	char *name;
	int type;
	float x0;
	float y0;
	float x1;
	float y1;
	float r0;
	float r1;
	float N;
	ps_bool extend0;
	ps_bool extend1;
	ps_bool antialias;
	PSColor startcolor;
	PSColor endcolor;
};

typedef struct PSGState_ PSGState;

struct PSGState_ {
	char *name;
	/* x, y for drawing */
	float x;
	float y;

	/* current fill color */
	PSColor fillcolor;
	ps_bool fillcolorinvalid;   /* set to ps_true if fill color has changed */

	/* current stroke color */
	PSColor strokecolor;
	ps_bool strokecolorinvalid; /* set to ps_true if stroke color has changed */

	ght_hash_table_t *opthash;
};

typedef struct PSTState_ PSTState;

struct PSTState_ {
	/* x, y for drawing text */
	float tx;
	float ty;
	/* x, y for continue text */
	float cx;
	float cy;
};

typedef struct value_ PS_VALUE;
struct value_ {
   char *name;
   float value;
};

typedef struct resource_ PS_PARAMETER;
typedef struct resource_ PS_RESOURCE;
struct resource_ {
   char *name;
   char *value;
};

typedef struct categorie_ PS_CATEGORY;
struct categorie_ {
   char *name;
   DLIST *resources;
};

typedef struct bookmark_ PS_BOOKMARK;
struct bookmark_ {
	int id;
	char *text;
	int open;
	int page;
	DLIST *children;
};

#define PS_MAX_GSTATE_LEVELS 10
#define PS_MAX_TSTATE_LEVELS 10

#define MAX_SCOPES       20  // maximum number of scopes on the stack
#define PS_SCOPE_NONE     0
#define PS_SCOPE_OBJECT   1
#define PS_SCOPE_DOCUMENT 2
#define PS_SCOPE_PAGE     4
#define PS_SCOPE_PATH     8
#define PS_SCOPE_TEMPLATE 16
#define PS_SCOPE_PATTERN  32
#define PS_SCOPE_PROLOG   64
#define PS_SCOPE_FONT     128
#define PS_SCOPE_GLYPH    256

#define LINEBUFLEN    1024

struct PSDoc_ {
	/* Document Information */
	char  *Keywords;
	char  *Subject;
	char  *Title;
	char  *Creator;
	char  *Author;
	char  *BoundingBox;
	char  *Orientation;

	int copies;

	STRBUFFER *sb;
	FILE *fp;
	ps_bool closefp;
	ps_bool headerwritten;
	ps_bool commentswritten;
	ps_bool beginprologwritten;
	ps_bool endprologwritten;
	ps_bool setupwritten;

	/* Creation date */
	char *CreationDate;

	/* Input encoding */
	ENCODING *inputenc;

	/* Hyphenation dictionary */
	HyphenDict *hdict;
	char *hdictfilename;

	/* The current font */
	PSFont *font;

	/* The current pattern */
	PSPattern *pattern;

	DLIST *categories;
	DLIST *parameters;
	DLIST *values;
	DLIST *bookmarks;	          /* list of first level bookmarks */
	int lastbookmarkid;         /* id of last bookmark */
	PS_BOOKMARK **bookmarkdict;  /* Bookmark lookup table */
	int bookmarkcnt;            /* Number of entries in lookup table */

	/* page stores the current page number.
	 * It is increased each time a page begins */
	unsigned int page;
	int in_error;
	ps_bool warnings;          /* Whether warnings shall be output */

	/* The stack of scopes */
	int scopecount;
	int scopes[MAX_SCOPES];

	/* List of fonts, images, patterns and spotcolors used in this document */
	PSFont **fonts;
	int fontcnt;
	PSImage **images;
	int imagecnt;
	PSPattern **patterns;
	int patterncnt;
	PSShading **shadings;
	int shadingcnt;
	PSSpotColor **spotcolors;
	int spotcolorcnt;
	PSGState **gstates;
	int gstatecnt;

	/* text appearance */
	ps_bool underline;
	ps_bool overline;
	ps_bool strikeout;
	int textrendering;

	/* grafic states */
	int agstate;
	PSGState agstates[PS_MAX_GSTATE_LEVELS];
	int tstate;
	PSTState tstates[PS_MAX_TSTATE_LEVELS];

	/* misc */
	ps_bool page_open;   /* set to true when PS_begin_page() is called
												* reset to false in PS_end_page() */
	ps_bool doc_open;    /* set to true when document is opened and reset
												* to false in PS_close() */

	/* border of annotations */
	int border_style;
	float border_width;
	float border_red;
	float border_green;
	float border_blue;
	float border_black;
	float border_white;

	/* output function */
	size_t  (*writeproc)(PSDoc *p, void *data, size_t size);

	/* user data passed by PS_new2(). Can be retrieved with PS_get_opaque() */
	void *user_data;

	/* error handler function */
	void  (*errorhandler)(PSDoc *p, int level, const char* msg, void *data);

	/* Memory allocation functions */
	void  *(*malloc)(PSDoc *p, size_t size, const char *caller);
	void  *(*calloc)(PSDoc *p, size_t size, const char *caller);
	void  *(*realloc)(PSDoc *p, void *mem, size_t size, const char *caller);
	void   (*free)(PSDoc *p, void *mem);
};

/* ps_util.c */
void ps_write(PSDoc *p, const void *data, size_t size);
void ps_putc(PSDoc *p, char c);
void ps_puts(PSDoc *p, const char *s);
void ps_printf(PSDoc *p, const char *fmt, ...);
void* ps_ght_malloc(size_t size, void *data);
void ps_ght_free(void *ptr, void *data);
PS_RESOURCE **ps_get_resources(PSDoc *psdoc, const char *category, int *count);
char *ps_find_resource(PSDoc *psdoc, const char *category, const char *resource);
PS_RESOURCE * ps_add_resource(PSDoc *psdoc, const char *category, const char *resource, const char *filename, const char *prefix);
int ps_get_bool_parameter(PSDoc *psdoc, const char *name, int deflt);
float ps_get_word_spacing(PSDoc *psdoc, PSFont *psfont);
void ps_set_word_spacing(PSDoc *psdoc, PSFont *psfont, float value);
void ps_add_word_spacing(PSDoc *psdoc, PSFont *psfont, float value);
void ps_del_resources(PSDoc *psdoc);
void ps_del_parameters(PSDoc *psdoc);
void ps_del_values(PSDoc *psdoc);
void ps_del_bookmarks(PSDoc *psdoc, DLIST *bookmarks);
int ps_check_for_lig(PSDoc *psdoc, ADOBEFONTMETRIC *metrics, ADOBEINFO *ai, const char *text, char ligdischar, char **newadobename, int *offset);
ght_hash_table_t *ps_build_enc_hash(PSDoc *psdoc, ENCODING *enc);
ght_hash_table_t *ps_build_enc_from_font(PSDoc *psdoc, ADOBEFONTMETRIC *metrics);
ENCODING *ps_build_enc_vector(PSDoc *psdoc, ght_hash_table_t *hashvec);
void ps_list_fontenc(PSDoc *psdoc, ght_hash_table_t *hashvec);
void ps_free_enc_vector(PSDoc *psdoc, ENCODING *enc);
unsigned char ps_fontenc_code(PSDoc *psdoc, ght_hash_table_t *fontenc, char *adobename);
int ps_fontenc_has_glyph(PSDoc *psdoc, ght_hash_table_t *fontenc, char *adobename);
char *ps_inputenc_name(PSDoc *psdoc, unsigned char c);
void ps_enter_scope(PSDoc *psdoc, int scope);
void ps_leave_scope(PSDoc *psdoc, int scope);
int ps_current_scope(PSDoc *psdoc);
ps_bool ps_check_scope(PSDoc *psdoc, int scope);
FILE *ps_open_file_in_path(PSDoc *psdoc, const char *filename);
ght_hash_table_t *ps_parse_optlist(PSDoc *psdoc, const char *optstr);
void ps_free_optlist(PSDoc *psdoc, ght_hash_table_t *opthash);
int get_optlist_element_as_float(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, float *value);
int get_optlist_element_as_int(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, int *value);
int get_optlist_element_as_bool(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, ps_bool *value);
int get_optlist_element_as_string(PSDoc *psdoc, ght_hash_table_t *opthash, const char *name, char **value);
void ps_ascii85_encode(PSDoc *psdoc, char *data, size_t len);
void ps_asciihex_encode(PSDoc *psdoc, char *data, size_t len);

/* ps_afm.c */
int calculatekern(ADOBEINFO *ai, ADOBEINFO *succ);
ADOBEFONTMETRIC *readadobe(PSDoc *psdoc, const char *filename);
ADOBEINFO *gfindadobe(ght_hash_table_t *adobechars, const char *p);
int readprotusion(PSDoc *psdoc, PSFont *psfont, const char *filename);
int readencoding(PSDoc *psdoc, ADOBEFONTMETRIC *metrics, const char *enc);
void addkern(PSDoc *psdoc, ADOBEINFO *ai, ADOBEINFO *succ, int kerning);
void addligature(PSDoc *psdoc, ADOBEINFO *ai, ADOBEINFO *succ, ADOBEINFO *sub);

#include "ps_inputenc.h"
#include "ps_fontenc.h"


#endif
