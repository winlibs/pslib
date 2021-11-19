/*
 *   This program converts AFM files to TeX TFM files, and optionally
 *   to TeX VPL files that retain all kerning and ligature information.
 *   Both files make the characters not normally encoded by TeX available
 *   by character codes greater than 127.
 */

/*   (Modified by Don Knuth from Tom Rokicki's pre-VPL version.) */
/*   VM/CMS port by J. Hafner (hafner@almaden.ibm.com), based on
 *   the port by Alessio Guglielmi (guglielmi@ipisnsib.bitnet)
 *   and Marco Prevedelli (prevedelli@ipisnsva.bitnet).
 *   This port is still in test state.  No guarantees.
 *   11/3/92: more corrections to VM/CMS port. Now it looks correct
 *   and will be supported by J. Hafner.
 *
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "ps_intern.h"
#include "ps_fontenc.h"
#include "ps_error.h"

#ifdef VMCMS
#define interesting lookstr  /* for 8 character truncation conflicts */
#include "dvipscms.h"
extern char ebcdic2ascii[] ;
extern char ascii2ebcdic[] ;
#ifdef fopen
#undef fopen
#endif
#endif

#if (defined(MSDOS) && defined(__TURBOC__)) || (defined(OS2) && defined(_MSC_VER))
#define SMALLMALLOC
#endif
	 
/* Ligatures {{{
 *
 * It's easier to put this in static storage and parse it as we go
 * than to build the structures ourselves.
 */
static char *staticligkern[] = {
	"% LIGKERN space l =: lslash ; space L =: Lslash ;",
	"% LIGKERN question quoteleft =: questiondown ;",
	"% LIGKERN exclam quoteleft =: exclamdown ;",
	"% LIGKERN hyphen hyphen =: endash ; endash hyphen =: emdash ;",
	"% LIGKERN quoteleft quoteleft =: quotedblleft ;",
	"% LIGKERN quoteright quoteright =: quotedblright ;",
	"% LIGKERN space {} * ; * {} space ; zero {} * ; * {} zero ;",
	"% LIGKERN one {} * ; * {} one ; two {} * ; * {} two ;",
	"% LIGKERN three {} * ; * {} three ; four {} * ; * {} four ;",
	"% LIGKERN five {} * ; * {} five ; six {} * ; * {} six ;",
	"% LIGKERN seven {} * ; * {} seven ; eight {} * ; * {} eight ;",
	"% LIGKERN nine {} * ; * {} nine ;",

/* Kern accented characters the same way as their base. */
/* There cannot be used currently and what are they for, anyway?
	"% LIGKERN Aacute <> A ; aacute <> a ;",
	"% LIGKERN Acircumflex <> A ; acircumflex <> a ;",
	"% LIGKERN Adieresis <> A ; adieresis <> a ;",
	"% LIGKERN Agrave <> A ; agrave <> a ;",
	"% LIGKERN Aring <> A ; aring <> a ;",
	"% LIGKERN Atilde <> A ; atilde <> a ;",
	"% LIGKERN Ccedilla <> C ; ccedilla <> c ;",
	"% LIGKERN Eacute <> E ; eacute <> e ;",
	"% LIGKERN Ecircumflex <> E ; ecircumflex <> e ;",
	"% LIGKERN Edieresis <> E ; edieresis <> e ;",
	"% LIGKERN Egrave <> E ; egrave <> e ;",
	"% LIGKERN Iacute <> I ; iacute <> i ;",
	"% LIGKERN Icircumflex <> I ; icircumflex <> i ;",
	"% LIGKERN Idieresis <> I ; idieresis <> i ;",
	"% LIGKERN Igrave <> I ; igrave <> i ;",
	"% LIGKERN Ntilde <> N ; ntilde <> n ;",
	"% LIGKERN Oacute <> O ; oacute <> o ;",
	"% LIGKERN Ocircumflex <> O ; ocircumflex <> o ;",
	"% LIGKERN Odieresis <> O ; odieresis <> o ;",
	"% LIGKERN Ograve <> O ; ograve <> o ;",
	"% LIGKERN Oslash <> O ; oslash <> o ;",
	"% LIGKERN Otilde <> O ; otilde <> o ;",
	"% LIGKERN Scaron <> S ; scaron <> s ;",
	"% LIGKERN Uacute <> U ; uacute <> u ;",
	"% LIGKERN Ucircumflex <> U ; ucircumflex <> u ;",
	"% LIGKERN Udieresis <> U ; udieresis <> u ;",
	"% LIGKERN Ugrave <> U ; ugrave <> u ;",
	"% LIGKERN Yacute <> Y ; yacute <> y ;",
	"% LIGKERN Ydieresis <> Y ; ydieresis <> y ;",
	"% LIGKERN Zcaron <> Z ; zcaron <> z ;",
*/
/*
 *   These next are only included for deficient afm files that
 *   have the lig characters but not the lig commands.
 */
	"% LIGKERN f i =: fi ; f l =: fl ; f f =: ff ; ff i =: ffi ;",
	"% LIGKERN ff l =: ffl ;",
	0 } ;
/*
 *   The above layout corresponds to TeX Typewriter Type and is compatible
 *   with TeX Text because the position of ligatures is immaterial.
 */
/* }}} */

static int boundarychar = -1 ;		 /* the boundary character */
static int ignoreligkern = 0;			 /* do we look at ligkern info in the encoding? */
static char *encligops[] = {
	"=:", "|=:", "|=:>", "=:|", "=:|>", "|=:|", "|=:|>", "|=:|>>", 0
} ;

#define BUFFERLEN 512
static char buffer[BUFFERLEN]; /* input buffer (modified while parsing) */
static char obuffer[BUFFERLEN] ; /* unmodified copy of input buffer */
static char *param ; /* current position in input buffer */

static void error(char *s) {
	(void)fprintf(stderr, "%s\n", s) ;
	if (obuffer[0]) {
		(void)fprintf(stderr, "%s\n", obuffer) ;
		while (param > buffer) {
			(void)fprintf(stderr, " ") ;
			param-- ;
		}
		(void)fprintf(stderr, "^\n") ;
	}
	if (*s == '!')
		exit(1) ;
}

static int transform(int x, int y) {
	float efactor = 1.0;
	float slant = 0.0;
	double acc ;
	acc = efactor * x + slant *y ;
	return (int)(acc>=0? floor(acc+0.5) : ceil(acc-0.5) ) ;
}

static int afm_getline(FILE *afmin) {
	char *p ;
	int c ;

	param = buffer ;
	for (p=buffer; (c=getc(afmin)) != EOF && c != '\n';)/* changed 10 to '\n' */
		*p++ = c ;
	*p = 0 ;
	(void)strncpy(obuffer, buffer, BUFFERLEN) ;
	obuffer[BUFFERLEN-1] = '\0';
	if (p == buffer && c == EOF)
		return(0) ;
	else
		return(1) ;
}

static char *interesting[] = { "FontName", "ItalicAngle", "IsFixedPitch",
	"XHeight", "C", "KPX", "CC", "EncodingScheme", "UnderlinePosition", "UnderlineThickness", "Ascender", "Descender", "CapHeight", "N", NULL} ;
#define FontName (0)
#define ItalicAngle (1)
#define IsFixedPitch (2)
#define XHeight (3)
#define C (4)
#define KPX (5)
#define CC (6)
#define EncodingScheme (7)
#define UnderlinePosition (8)
#define UnderlineThickness (9)
#define Ascender (10)
#define Descender (11)
#define CapHeight (12)
#define N (13)  /* Needed for .pro files */
#define NONE (-1)
static int interest(char *s) {
	char **p ;
	int n ;

	for (p=interesting, n=0; *p; p++, n++)
		if (strcmp(s, *p)==0)
			return(n) ;
	return(NONE) ;
}

/*
static char *mymalloc(unsigned long len) {
	char *p ;

#ifdef SMALLMALLOC
	if (len > 65500L)
		error("! can't allocate more than 64K!") ;
#endif
	p = (char *) malloc((unsigned int)len) ;
	if (p == NULL)
		error("! out of memory") ;
	memset(p, 0, len);
	return(p) ;
}
*/

static char * newstring(PSDoc *psdoc, char *s) {
	char *q = psdoc->malloc(psdoc, (unsigned long)(strlen(s) + 1), s) ;
	(void)strcpy(q, s) ;
	return q ;
}

static char *paramnewstring(PSDoc *psdoc) {
	char *p, *q ;

	p = param ;
	while (*p > ' ')
		p++ ;
	if (*p != 0)
		*p++ = 0 ;
	q = newstring(psdoc, param) ;
	while (*p && *p <= ' ')
		p++ ;
	param = p ;
	return(q) ;
}

static char *paramstring() {
	char *p, *q ;

	p = param ;
	while (*p > ' ')
		p++ ;
	q = param ;
	if (*p != 0)
		*p++ = 0 ;
	while (*p && *p <= ' ')
		p++ ;
	param = p ;
	return(q) ;
}

static int paramnum() {
	char *p ;
	int i ;

	p = paramstring() ;
	if (sscanf(p, "%d", &i) != 1)
		error("! integer expected") ;
	return(i) ;
}

static float paramfloat() {
	char *p ;
	float i ;

	p = paramstring() ;
	if (sscanf(p, "%f", &i) != 1)
		error("! number expected") ;
	return(i) ;
}

static ADOBEINFO *newchar(PSDoc *psdoc) {
	ADOBEINFO *ai ;

	ai = (ADOBEINFO *) psdoc->malloc(psdoc, (unsigned long) sizeof(ADOBEINFO), "newchar: allocate memory for new character") ;
	ai->adobenum = -1 ;
	ai->texnum = -1 ;
	ai->width = -1 ;
	ai->adobename = NULL ;
	ai->llx = -1 ;
	ai->lly = -1 ;
	ai->urx = -1 ;
	ai->ury = -1 ;
	ai->lkern = 0;
	ai->rkern = 0;
	ai->ligs = NULL ;
	ai->kerns = NULL ;
	ai->kern_equivs = NULL ;
	ai->pccs = NULL ;
	return(ai) ;
}

static KERN *newkern(PSDoc *psdoc) {
	KERN *nk ;

	nk = (KERN *) psdoc->malloc(psdoc, (unsigned long) sizeof(KERN), "newkern: allocate memory for new kerning") ;
	nk->next = NULL ;
	nk->succ = NULL ;
	nk->delta = 0 ;
	return(nk) ;
}

static PCC *newpcc(PSDoc *psdoc) {
	PCC *np ;

	np = (PCC *) psdoc->malloc(psdoc, (unsigned long) sizeof(PCC), "newppc: allocate  memory for new pcc") ;
	np->next = NULL ;
	np->partname = NULL ;
	np->xoffset = 0 ;
	np->yoffset = 0 ;
	return(np) ;
}

static LIG *newlig(PSDoc *psdoc) {
	LIG *nl ;

	nl = (LIG *) psdoc->malloc(psdoc, (unsigned long) sizeof(LIG), "newlig: allocate memory for new ligature") ;
	nl->next = NULL ;
	nl->succ = NULL ;
	nl->sub = NULL ;
	nl->op = 0 ; /* the default =: op */
	nl->boundleft = 0 ;
	return(nl) ;
}

static int expect(char *s) {
	if (strcmp(paramstring(), s) != 0) {
		return(ps_false);
	}
	return(ps_true);
}

/* handlechar() {{{
 */
static ADOBEINFO * handlechar(PSDoc *psdoc, ADOBEFONTMETRIC *metric) { /* an input line beginning with C */
	ADOBEINFO *ai ;
	LIG *nl ;

	ai = newchar(psdoc) ;
	ai->adobenum = paramnum() ;
	if(!expect(";")) {
		psdoc->free(psdoc, ai);
		ps_error(psdoc, PS_RuntimeError, _("Expected ';' in afm file."));
		return(NULL);
	}
	if(!expect("WX")) {
		psdoc->free(psdoc, ai);
		ps_error(psdoc, PS_RuntimeError, _("Expected 'WX' in afm file."));
		return(NULL);
	}
	ai->width = paramnum(); //transform(paramnum(),0) ;
//	if (ai->adobenum >= 0 && ai->adobenum < 256) {
//		metric->adobeptrs[ai->adobenum] = ai ;
//	}
	if(!expect(";")) {
		psdoc->free(psdoc, ai);
		ps_error(psdoc, PS_RuntimeError, _("Expected ';' in afm file."));
		return(NULL);
	}
	if(!expect("N")) {
		psdoc->free(psdoc, ai);
		ps_error(psdoc, PS_RuntimeError, _("Expected 'N' in afm file."));
		return(NULL);
	}
	ai->adobename = paramnewstring(psdoc) ;
	if(!expect(";")) {
		psdoc->free(psdoc, ai->adobename);
		psdoc->free(psdoc, ai);
		ps_error(psdoc, PS_RuntimeError, _("Expected ';' in afm file."));
		return(NULL);
	}
	if(expect("B")) {
		ai->llx = paramnum() ;
		ai->lly = paramnum() ;
//	ai->llx = transform(ai->llx, ai->lly) ;
		ai->urx = paramnum() ;
		ai->ury = paramnum() ;
//	ai->urx = transform(ai->urx, ai->ury) ;
		expect(";") ;
	}
/* Now look for ligatures (which aren't present in fixedpitch fonts) */
	while (*param == 'L' && !metric->fixedpitch) {
		if(!expect("L")) {
			ps_error(psdoc, PS_RuntimeError, _("Expected 'L' in afm file."));
			return(NULL);
		}
		nl = newlig(psdoc) ;
		nl->succ = paramnewstring(psdoc) ;
		nl->sub = paramnewstring(psdoc) ;
		nl->next = ai->ligs ;
		ai->ligs = nl ;
		if(!expect(";")) {
			ps_error(psdoc, PS_RuntimeError, _("Expected ';' in afm file."));
			return(NULL);
		}
	}
	return(ai);
}
/* }}} */

/* handleprotusion() {{{
 */
static int handleprotusion(PSDoc *psdoc, ADOBEFONTMETRIC *metric) { /* an input line begining with N */
	ADOBEINFO *ai ;
	char *adobename;

	adobename = paramstring() ;
	ai = gfindadobe(metric->gadobechars, adobename);
	if(ai) {
		if(!expect(";")) {
			ps_error(psdoc, PS_RuntimeError, _("Expected ';' in protusion file."));
			return(ps_false);
		}
		if(!expect("M")) {
			ps_error(psdoc, PS_RuntimeError, _("Expected 'M' in protusion file."));
			return(ps_false);
		}
		ai->lkern = paramnum();
		ai->rkern = paramnum();
		if(!expect(";")) {
			ps_error(psdoc, PS_RuntimeError, _("Expected ';' in protusion file."));
			return(ps_false);
		}
	}
	return(ps_true);
}
/* }}} */

/* calculatekern() {{{
 *
 * Calculates the kerning between two consecutive glyphs.
 */
int calculatekern(ADOBEINFO *ai, ADOBEINFO *succ) {
	KERN *k;

	if((NULL == ai) || (NULL == succ))
		return(0);

	k = ai->kerns;
//	printf("get kerning for %s followed by %s\n", ai->adobename, succ->adobename);
	while (k && strcmp(k->succ, succ->adobename)!=0) {
//		printf("'%s' == '%s': kerning %d\n", k->succ, succ->adobename, k->delta);
		k = k->next ;
	}
	if(k) {
//		printf("%s == %s: kerning %d\n", k->succ, succ->adobename, k->delta);
		return(k->delta);
	} else {
		return(0);
	}
}
/* }}} */

/* addkern() {{{
 *
 * Adds a new kerning pair to the list. Does not check if the pair
 * already exists. The new pair is inserted at the beginning of the
 * list.
 */
void addkern(PSDoc *psdoc, ADOBEINFO *ai, ADOBEINFO *succ, int kerning) {
	KERN *nk ;

	if((NULL == ai) || (NULL == succ)) {
		error("One of the glyphs is not set.");
		return;
	}
	nk = newkern(psdoc) ;
	nk->succ = newstring(psdoc, succ->adobename);
	nk->delta = kerning;
	nk->next = ai->kerns ;
	ai->kerns = nk ;
}
/* }}} */

/* addligature() {{{
 *
 * Adds a new ligature to the list. Does not check if the ligature
 * already exists. The new ligature is inserted at the beginning of the
 * list.
 */
void addligature(PSDoc *psdoc, ADOBEINFO *ai, ADOBEINFO *succ, ADOBEINFO *sub) {
	LIG *nl ;

	if((NULL == ai) || (NULL == succ) || (NULL == sub)) {
		error("One of the glyphs is not set.");
		return;
	}
	nl = newlig(psdoc) ;
	nl->succ = newstring(psdoc, succ->adobename);
	nl->sub = newstring(psdoc, sub->adobename) ;
	nl->next = ai->ligs ;
	ai->ligs = nl ;
}
/* }}} */

/* gfindadobe() {{{
 *
 * Finds metric of a glyph by its name.
 * This function replaces findadobe().
 */
ADOBEINFO *gfindadobe(ght_hash_table_t *gadobechars, const char *p) {
	ADOBEINFO *ai ;

	if(p == NULL || p[0] == '\0')
		return(NULL);
	if(gadobechars == NULL)
		return(NULL);
	ai = ght_get(gadobechars, strlen(p)+1, (void *) p);
	return(ai) ;
}
/* }}} */

/* handlekern() {{{
 *
 * The following comment no longer applies; we rely on the LIGKERN
 * entries to kill space kerns.  Also, the same applies to numbers.
 *
 * We ignore kerns before and after space characters, because (1) TeX
 * is using the space only for Polish ligatures, and (2) TeX's
 * boundarychar mechanisms are not oriented to kerns (they apply
 * to both spaces and punctuation) so we don't want to use them.
 */
static void handlekern(PSDoc *psdoc, ADOBEFONTMETRIC *metric) { /* an input line beginning with KPX */
	ADOBEINFO *ai ;
	char *p ;
	KERN *nk ;

	p = paramstring() ;
	ai = gfindadobe(metric->gadobechars, p) ;
	if (ai == NULL)
		error("kern char not found") ;
	else {
		nk = newkern(psdoc) ;
		nk->succ = paramnewstring(psdoc) ;
		nk->delta = paramnum(); //transform(paramnum(),0) ;
		nk->next = ai->kerns ;
		ai->kerns = nk ;
	 }
}
/* }}} */

/* handleconstruct() {{{
 *
 * Reads lines for composite chars from afm file.
 */
static int handleconstruct(PSDoc *psdoc, ADOBEFONTMETRIC *metric) { /* an input line beginning with CC */
	ADOBEINFO *ai ;
	char *p ;
	PCC *np ;
	int n ;
	PCC *npp = NULL;

	p = paramstring() ;
	ai = gfindadobe(metric->gadobechars, p) ;
	if (ai == NULL)
		error("! composite character name not found") ;
	n = paramnum() ;
	if(!expect(";")) {
		fprintf(stderr, "; expected: ") ;
		error("syntax error");
		return(ps_false);
	}
	while (n--) {
		if (strcmp(paramstring(),"PCC") != 0)
			return(ps_false);
		  /* maybe I should expect("PCC") instead, but I'm playing it safe */
		np = newpcc(psdoc) ;
		np->partname = paramnewstring(psdoc) ;
		if (gfindadobe(metric->gadobechars, np->partname)==NULL)
			return(ps_false);
		np->xoffset = paramnum() ;
		np->yoffset = paramnum() ;
//		np->xoffset = transform(np->xoffset, np->yoffset) ;
		if (npp) npp->next = np ;
		else ai->pccs = np ;
		npp = np ;
		if(!expect(";")) {
			fprintf(stderr, "; expected: ") ;
			error("syntax error");
			return(ps_false);
		}
	}
	return(ps_true);
}
/* }}} */

/* makeaccentligs() {{{
 *
 * Creates artificial ligatures for glyphs like adieresis or Aacute by
 * using the first letter (a, A, ...) and the rest of the glyph name
 * (dieresis, acute, ...) to form a ligature. This will also try to
 * make ligature for eg. one, exclam, space, which will fail because
 * there is no glyph 'ne', 'xclam' or 'pace'. This is somewhat a brute
 * force methode.
 */
/*
static void makeaccentligs(ght_hash_table_t *gadobechars) {
	ADOBEINFO *ai, *aci ;
	char *p ;
	LIG *nl ;
	ght_iterator_t iterator;

	for(ai = ght_first(gadobechars, &iterator, (void **) &p); ai != NULL; ai = ght_next(gadobechars, &iterator, (void **) &p)) {
		if (strlen(p)>2)
			if ((aci=gfindadobe(gadobechars, p+1)) && (aci->adobenum > 127)) {
				nl = newlig() ;
				nl->succ = mymalloc((unsigned long)2) ;
				*(nl->succ + 1) = 0 ;
				*(nl->succ) = *p ;
				nl->sub = ai->adobename ;
				nl->next = aci->ligs ;
				aci->ligs = nl ;
			}
	}
}
*/
/* }}} */

/* readprotusion() {{{
 *
 * Read file with information about protusion of single chars.
 */
int readprotusion(PSDoc *psdoc, PSFont *psfont, const char *filename) {
	ADOBEFONTMETRIC *metrics;
	FILE *fp;
#ifdef VMCMS
	 int i;
#endif

	metrics = psfont->metrics;
	if(NULL == (fp = ps_open_file_in_path(psdoc, filename))) {
		return(-1);
	}

	while (afm_getline(fp)) {
		switch(interest(paramstring())) {
			case N:
				handleprotusion(psdoc, metrics) ;
				break ;
		}
	}
	fclose(fp) ;
	return(0);
}
/* }}} */

/* readadobe() {{{
 *
 * Reads an afm file and returns a adobe font metric structure.
 */
ADOBEFONTMETRIC *readadobe(PSDoc *psdoc, const char *filename) {
	ADOBEINFO *ai;
	ADOBEFONTMETRIC *metric;
#ifdef VMCMS
	 int i;
#endif

	if(NULL == (metric = (ADOBEFONTMETRIC *) psdoc->malloc(psdoc, sizeof(ADOBEFONTMETRIC), _("Allocate space for font metric.")))) {
		return(NULL);
	}
	memset(metric, 0, sizeof(ADOBEFONTMETRIC));

	if(0 == (metric->afmin = ps_open_file_in_path(psdoc, filename))) {
		ps_error(psdoc, PS_RuntimeError, _("Couldn't open afm file: %s\n"), filename);
		psdoc->free(psdoc, metric);
		return(NULL);
	}

	/*
	 * Create the hash table for the chars
	 */
	metric->gadobechars = ght_create(512);
	ght_set_alloc(metric->gadobechars, ps_ght_malloc, ps_ght_free, psdoc);

	/*
	 * Set default font encoding
	 */
//	metric->fontenc = ps_build_enc_hash(psdoc, &fontencoding);
	metric->fontenc = NULL;

	/*
	 * Read file line by line.
	 */
	while (afm_getline(metric->afmin)) {
		switch(interest(paramstring())) {
case FontName:
			metric->fontname = paramnewstring(psdoc) ;
#ifdef VMCMS
			/* fontname comes in as ebcdic but we need it asciified for tfm file
				so we save it in ebfontname and change it in fontname */
			metric->ebfontname = newstring(psdoc, metric->fontname) ;
			i=0;
			while(metric->fontname[i] != '\0') {
				metric->fontname[i]=ebcdic2ascii[metric->fontname[i]];
				i++;
			}
#endif
			break ;
case EncodingScheme:
			metric->codingscheme = paramnewstring(psdoc) ;
#ifdef VMCMS
/* for codingscheme, we do the same as we did for fontname */
	metric->ebcodingscheme = newstring(psdoc, metric->codingscheme) ;
	i=0;
	while(metric->codingscheme[i] != '\0') {
		metric->codingscheme[i]=ebcdic2ascii[metric->codingscheme[i]];
		i++;
	}
#endif
			break ;
case ItalicAngle:
			metric->italicangle = paramfloat() ;
			break ;
case UnderlinePosition:
			metric->underlineposition = paramfloat() ;
			break ;
case UnderlineThickness:
			metric->underlinethickness = paramfloat() ;
			break ;
case Ascender:
			metric->ascender = paramfloat() ;
			break ;
case Descender:
			metric->descender = paramfloat() ;
			break ;
case CapHeight:
			metric->capheight = paramfloat() ;
			break ;
case IsFixedPitch:
			if (*param == 't' || *param == 'T')
				metric->fixedpitch = 1 ;
			else
				metric->fixedpitch = 0 ;
			break ;
case XHeight:
			metric->xheight = paramnum() ;
			break ;
case C: {
			ADOBEINFO *hai = handlechar(psdoc, metric) ;
			if(gfindadobe(metric->gadobechars, hai->adobename)) {
				ps_error(psdoc, PS_Warning, _("Trying to insert the glyph '%s' which already exists. Please check your afm file for duplicate glyph names."), hai->adobename);
				/* FIXME: Freeing the name and the ADOBEINFO structure may not be
				 * enough. There can be kerning pairs and ligatures.
				 */
				psdoc->free(psdoc, hai->adobename);
				psdoc->free(psdoc, hai);
			} else
				ght_insert(metric->gadobechars, hai, strlen(hai->adobename)+1, hai->adobename);
			break ;
}
case KPX:
			handlekern(psdoc, metric) ;
			break ;
case CC:
			handleconstruct(psdoc, metric) ;
			break ;
default:
			break ;
		}
	}
	fclose(metric->afmin) ;
	metric->afmin = 0 ;

	if (0 != (ai = gfindadobe(metric->gadobechars, "space"))) {
		metric->fontspace = ai->width ;
	} else {
		metric->fontspace = 500; //transform(500, 0) ;
	}

/*
	{
	ght_iterator_t iterator;
	char *p_key;
	ADOBEINFO *p_e;
	for(p_e = ght_first(metric->gadobechars, &iterator, &p_key); p_e; p_e = ght_next(metric->gadobechars, &iterator, &p_key)) {
		fprintf(stderr, "%s = %s\n", p_key, p_e->adobename);
	}
	}
*/
	return(metric);
}
/* }}} */

/*
 *   Re-encode the adobe font.  Assumes that the header file will
 *   also contain the appropriate instructions!
 */
/*
static char *outenname, *inenname ;
static ENCODING *outencoding = NULL ;
static ENCODING *inencoding = NULL ;
void
handlereencoding(ADOBEFONTMETRIC *metric) {
	if (inenname) {
		int i ;
		ADOBEINFO *ai ;
		char *p ;

		ignoreligkern = 1 ;
		inencoding = readencoding(inenname) ;
		for (i=0; i<256; i++)
			if (0 != (ai=metric->adobeptrs[i])) {
				ai->adobenum = -1 ;
				metric->adobeptrs[i] = NULL ;
			}
		for (i=0; i<256; i++) {
			p = inencoding->vec[i] ;
			if (p && *p && 0 != (ai = gfindadobe(metric->gadobechars, p))) {
				ai->adobenum = i ;
				metric->adobeptrs[i] = ai ;
			}
		}
		metric->codingscheme = inencoding->name ;
	}
	ignoreligkern = 0 ;
	if (outenname) {
		outencoding = readencoding(outenname) ;
	} else {
		outencoding = readencoding((char *)0) ;
	}
}
*/

/* rmkernmatch() {{{
 *
 * Some routines to remove kerns that match certain patterns.
 */
static KERN *rmkernmatch(PSDoc *psdoc, KERN *k, char *s)
{
	KERN *nk, *ktmp ;

	while (k != NULL && strcmp(k->succ, s)==0) {
		psdoc->free(psdoc, k->succ);
		ktmp = k->next;
		psdoc->free(psdoc, k);
		k = ktmp;
	}
	if (k) {
		for (nk = k; nk; nk = nk->next)
			while (nk->next && strcmp(nk->next->succ, s)==0) {
				psdoc->free(psdoc, nk->next->succ);
				ktmp = nk->next->next;
				psdoc->free(psdoc, nk->next);
				nk->next = ktmp;
			}
	}
	return k ;
}
/* }}} */

/* rmkern() {{{
 *
 * Recursive to one level.
 */
static void rmkern(PSDoc *psdoc, ght_hash_table_t *gadobechars, char *s1, char *s2, ADOBEINFO *ai)
{
	ght_iterator_t iterator;
	char *p;

	if (ai == 0) {
		if (strcmp(s1, "*") == 0) {

			for(ai = ght_first(gadobechars, &iterator, (void **) &p); ai; ai = ght_next(gadobechars, &iterator, (void **) &p))
				rmkern(psdoc, gadobechars, s1, s2, ai) ;
			return ;
		} else {
			ai = gfindadobe(gadobechars, s1) ;
			if (ai == 0)
				return ;
		}
	}
	if (strcmp(s2, "*")==0) {
		KERN *k, *ktmp;
		k = ai->kerns;
		while(k != NULL) {
			if(k->succ)
				psdoc->free(psdoc, k->succ);
			ktmp = k->next;
			psdoc->free(psdoc, k);
			k = ktmp;
		}
		ai->kerns = NULL ; /* drop them on the floor */
	} else
		ai->kerns = rmkernmatch(psdoc, ai->kerns, s2) ;
}
/* }}} */

/* copykern() {{{
 * Make the kerning for character S1 equivalent to that for S2.
 * If either S1 or S2 do not exist, do nothing.
 * If S1 already has kerning, do nothing.
 */
static void copykern(PSDoc *psdoc, ght_hash_table_t *gadobechars, char *s1, char *s2)
{
	ADOBEINFO *ai1 = gfindadobe(gadobechars, s1);
	ADOBEINFO *ai2 = gfindadobe(gadobechars, s2);
	if (ai1 && ai2 && !ai1->kerns) {
		/* Put the new one at the head of the list, since order is immaterial.  */
		ADOBEPTR *ap = (ADOBEPTR *) psdoc->malloc(psdoc, (unsigned long) sizeof(ADOBEPTR), "copykern: adobeptr");
		ap->next = ai2->kern_equivs;
		ap->ch = ai1;
		ai2->kern_equivs = ap;
	}
}
/* }}} */

static int sawligkern ;
/* checkligkern() {{{
 *
 * Reads a ligkern line, if this is one.  Assumes the first character
 * passed is `%'.
 */
static void checkligkern(PSDoc *psdoc, ADOBEFONTMETRIC *metrics, char *s)
{
	char *oparam = param ;
	char *mlist[5] ;
	int n ;
	ght_hash_table_t *gadobechars = metrics->gadobechars;

	s++ ;
	while (*s && *s <= ' ')
		s++ ;
	if (strncmp(s, "LIGKERN", 7)==0) {
		sawligkern = 1 ;
		s += 7 ;
		while (*s && *s <= ' ')
			s++ ;
		param = s ;
		while (*param) {
			for (n=0; n<5;) {
				if (*param == 0)
					break ;
				mlist[n] = paramstring() ;
				if (strcmp(mlist[n], ";") == 0)
					break ;
				n++ ;
			}
			if (n > 4) {
				ps_error(psdoc, PS_RuntimeError, _("Too many parameters in lig kern data."));
				return;
			}
			if (n < 3) {
				ps_error(psdoc, PS_RuntimeError, _("Too few parameters in lig kern data."));
				return;
			}
			if (n == 3 && strcmp(mlist[1], "{}") == 0) { /* rmkern command */
				rmkern(psdoc, gadobechars, mlist[0], mlist[2], (ADOBEINFO *)0) ;
			} else if (n == 3 && strcmp(mlist[1], "<>") == 0) { /* copykern */
				/* FIXME: What is the meaning of kern_equivs?
				 * copykern() does not set the kern by kern_equivs which is not
				 * used.
				 */
				copykern(psdoc, gadobechars, mlist[0], mlist[2]) ;
			} else if (n == 3 && strcmp(mlist[0], "||") == 0 &&
				strcmp(mlist[1], "=") == 0) { /* bc command */
				ADOBEINFO *ai = gfindadobe(gadobechars, "||") ;

				if (boundarychar != -1) {
					ps_error(psdoc, PS_RuntimeError, _("Multiple boundary character commands?"));
					return;
				}
				if (sscanf(mlist[2], "%d", &n) != 1) {
					ps_error(psdoc, PS_RuntimeError, _("Expected number assignment for boundary char."));
					return;
				}
				if (n < 0 || n > 255) {
					ps_error(psdoc, PS_RuntimeError, _("Boundary character number must be 0..255."));
					return;
				}
				boundarychar = n ;
				if (ai == 0) {
					ps_error(psdoc, PS_RuntimeError, _("Internal error: boundary char."));
					return;
				}
				ai->texnum = n ; /* prime the pump, so to speak, for lig/kerns */
			} else if (n == 4) {
				int op = -1 ;
				ADOBEINFO *ai ;

				for (n=0; encligops[n]; n++)
					if (strcmp(mlist[2], encligops[n])==0) {
						op = n ;
						break ;
					}
				if (op < 0) {
					ps_error(psdoc, PS_RuntimeError, _("Bad ligature operation specified."));
					return;
				}
				if (0 != (ai = gfindadobe(gadobechars, mlist[0]))) {
					LIG *lig ;

					/* This does not make sense */
					if (gfindadobe(gadobechars, mlist[2]))	  /* remove coincident kerns */
						rmkern(psdoc, gadobechars, mlist[0], mlist[1], ai) ;
					if (strcmp(mlist[3], "||") == 0) {
						ps_error(psdoc, PS_RuntimeError, _("You can't lig to the boundary character!"));
						return;
					}
					/* FIXME: The following code is commented because the staticligkern
					 * instruction didn't work anymore. For some reason e.g. the endash
					 * wasn't found, though it was part of the font. I appears that
					 * at this point the list of glyphs on gadobechars isn't complete
					 */
					/*
					if (gfindadobe(gadobechars, mlist[3]))	{
						ps_error(psdoc, PS_RuntimeError, _("Ligature '%s' not available in font."), mlist[3]);
						return;
					}
					*/
					if (!metrics->fixedpitch) { /* fixed pitch fonts get *0* ligs */
						for (lig=ai->ligs; lig; lig = lig->next)
							if (strcmp(lig->succ, mlist[1]) == 0)
								break ; /* we'll re-use this structure */
//						printf("Adding new lig %s %s -> %s\n", mlist[0], mlist[1], mlist[3]);
						if (lig == 0) {
							lig = newlig(psdoc) ;
							lig->succ = newstring(psdoc, mlist[1]) ;
							lig->next = ai->ligs ;
							ai->ligs = lig ;
						}
						if(lig->sub)
							psdoc->free(psdoc, lig->sub);
						lig->sub = newstring(psdoc, mlist[3]) ;
						lig->op = op ;
						if (strcmp(mlist[1], "||")==0) {
							lig->boundleft = 1 ;
							if (strcmp(mlist[0], "||")==0) {
								ps_error(psdoc, PS_RuntimeError, _("You can't lig boundary character to boundary character."));
								return;
							}
						} else
							lig->boundleft = 0 ;
					}
				}
			} else {
				ps_error(psdoc, PS_RuntimeError, _("Bad form in LIGKERN command."));
				return;
			}
		}
	}
	param = oparam ;
}
/* }}} */

/* char *gettoken() {{{
 *
 * Here we get a token from the encoding file. We parse just as much PostScript
 * as we expect to find in an encoding file.	We allow commented lines and
 * names like 0, .notdef, _foo_.	We do not allow //abc.
 * This function calls checkligkern() which reads extra ligatures which
 * are usually at the beginning of the file marked as PostScript comments.
 */
static char smbuffer[100] ;		/* for tokens */
static char *gettoken(PSDoc *psdoc, ADOBEFONTMETRIC *metrics) {
	char *p, *q ;

	while (1) {
		while (param == NULL || *param == '\0') {
			if (afm_getline(metrics->afmin) == 0)
				ps_error(psdoc, PS_RuntimeError, _("Premature end of encoding file."));
			for (p=buffer; *p != '\0'; p++)
				if (*p == '%') {
					if (ignoreligkern == 0)
						checkligkern(psdoc, metrics, p) ;
					*p = '\0' ;
					break ;
				}
		}
		while (*param != '\0' && *param <= ' ')
			param++ ;
		if (*param) {
			if (*param == '[' || *param == ']' ||
				 *param == '{' || *param == '}') {
				smbuffer[0] = *param++ ;
				smbuffer[1] = '\0' ;
				return smbuffer ;
			} else if (*param == '/' || *param == '-' || *param == '_' ||
						  *param == '.' ||
						  ('0' <= *param && *param <= '9') ||
						  ('a' <= *param && *param <= 'z') ||
						  ('A' <= *param && *param <= 'Z')) {
				smbuffer[0] = *param ;
				for (p=param+1, q=smbuffer+1;
								*p == '-' || *p == '_' || *p == '.' ||
								('0' <= *p && *p <= '9') ||
								('a' <= *p && *p <= 'z') ||
								('A' <= *p && *p <= 'Z'); p++, q++)
					*q = *p ;
				*q = '\0' ;
				param = p ;
				return smbuffer ;
			}
		}
	}
}
/* }}} */

/* getligkerndefaults() {{{
 * 
 * Set default ligatures
 */
static void getligkerndefaults(PSDoc *psdoc, ADOBEFONTMETRIC *metrics) {
	int i ;

	for (i=0; staticligkern[i]; i++) {
		strncpy(buffer, staticligkern[i], BUFFERLEN) ;
		strncpy(obuffer, staticligkern[i], BUFFERLEN) ;
		param = buffer ;
		checkligkern(psdoc, metrics, buffer) ;
	}
}
/* }}} */

/* readencoding() {{{
 *
 * This routine reads in an encoding file, given the name.  It modifies
 * the passed font metrics structure. It returns -1 in case of error and
 * 0 otherwise. It performs a number of consistency checks.
 * If enc is NULL it will use the default encoding from ps_fontenc.c
 */
int readencoding(PSDoc *psdoc, ADOBEFONTMETRIC *metrics, const char *enc) {
	char *p ;
	int i ;
	ENCODING *e;
	sawligkern = 0 ;
	if (metrics->afmin) {
		ps_error(psdoc, PS_RuntimeError, _("Encoding file for this font seems to be already open."));
		return -1;
	}
	if(enc) {
		metrics->afmin = ps_open_file_in_path(psdoc, enc) ;
		param = 0 ;
		if (metrics->afmin == NULL) {
			ps_error(psdoc, PS_RuntimeError, _("Could not open encoding file '%s'."), enc);
			return -1;
		}
		p = gettoken(psdoc, metrics) ;
		if (*p != '/' || p[1] == '\0') {
			ps_error(psdoc, PS_RuntimeError, _("Encoding file must start with name of encoding."));
			return -1;
		}

		e = (ENCODING *) psdoc->malloc(psdoc, sizeof(ENCODING), _("Allocate memory for new encoding vector.")) ;
		if(NULL == e) {
			ps_error(psdoc, PS_MemoryError, _("Could not allocate memory for encoding vector."));
			return -1;
		}

		e->name = newstring(psdoc, p+1) ;
		p = gettoken(psdoc, metrics) ;
		if (0 != strcmp(p, "[")) {
			ps_error(psdoc, PS_RuntimeError, _("Name of encoding must be followed by an '['."));
			psdoc->free(psdoc, e->name);
			psdoc->free(psdoc, e);
			return -1;
		}
		for (i=0; i<256; i++) {
			p = gettoken(psdoc, metrics) ;
			if (*p != '/' || p[1] == '\0') {
				ps_error(psdoc, PS_RuntimeError, _("Encoding vector must contain 256 glyph names."));
				for (i--; i>=0; i--) {
					psdoc->free(psdoc, e->vec[i]);
				}
				psdoc->free(psdoc, e->name);
				psdoc->free(psdoc, e);
				return -1;
			}
			e->vec[i] = newstring(psdoc, p+1) ;
		}
		p = gettoken(psdoc, metrics) ;
		if (0 != strcmp(p, "]")) {
			ps_error(psdoc, PS_RuntimeError, _("Encoding vector must be ended by an ']'."));
			for (i=0; i<256; i++) {
				psdoc->free(psdoc, e->vec[i]);
			}
			psdoc->free(psdoc, e->name);
			psdoc->free(psdoc, e);
			return -1;
		}
		while (afm_getline(metrics->afmin)) {
			for (p=buffer; *p != '\0'; p++)
				if (*p == '%') {
					if (ignoreligkern == 0)
						checkligkern(psdoc, metrics, p) ;
					*p = '\0' ;
					break ;
				}
		}
		fclose(metrics->afmin) ;
		metrics->afmin = NULL ;
		if (ignoreligkern == 0 && sawligkern == 0) {
			getligkerndefaults(psdoc, metrics) ;
		}
		metrics->fontenc = ps_build_enc_hash(psdoc, e);
		if(metrics->codingscheme)
			psdoc->free(psdoc, metrics->codingscheme);
		metrics->codingscheme = newstring(psdoc, e->name);
		/* FIXME: For now we through away the fontenc from the file and
		 * take the predefined font encoding.
		 */
		for (i=0; i<256; i++) {
			psdoc->free(psdoc, e->vec[i]);
		}
		psdoc->free(psdoc, e->name);
		psdoc->free(psdoc, e);
	} else { /* not font encoding file name passed */
		e = &fontencoding[0] ;
		getligkerndefaults(psdoc, metrics) ;
		metrics->fontenc = ps_build_enc_hash(psdoc, e);
		if(metrics->codingscheme)
			psdoc->free(psdoc, metrics->codingscheme);
		metrics->codingscheme = newstring(psdoc, e->name);
	}
	param = 0 ;
	return 0 ;
}
/* }}} */

/*   readadobe() ;
	if (fontspace == 0) {
		ADOBEINFO *ai ;

	}
	handlereencoding() ;
*/
