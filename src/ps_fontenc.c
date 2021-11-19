#include "ps_intern.h"

#define NUM_FONTENCODINGS 2

ENCODING fontencoding[NUM_FONTENCODINGS] = {
	{"CorkEncoding",
	{
/* 0x00 */
  "grave", "acute", "circumflex", "tilde", "dieresis", "hungarumlaut", "ring", "caron",
  "breve", "macron", "dotaccent", "cedilla",
  "ogonek", "quotesinglbase", "guilsinglleft", "guilsinglright",
/* 0x10 */
  "quotedblleft", "quotedblright", "quotedblbase", "guillemotleft", 
  "guillemotright", "endash", "emdash", "compwordmark",
  "perthousandzero", "dotlessi", "dotlessj", "ff", "fi", "fl", "ffi", "ffl",
/* 0x20 */
  "visualspace", "exclam", "quotedbl", "numbersign",
  "dollar", "percent", "ampersand", "quoteright",
  "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
/* 0x30 */
  "zero", "one", "two", "three", "four", "five", "six", "seven",
  "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
/* 0x40 */
  "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
/* 0x50 */
  "P", "Q", "R", "S", "T", "U", "V", "W",
  "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
/* 0x60 */
  "quoteleft", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
/* 0x70 */
  "p", "q", "r", "s", "t", "u", "v", "w",
  "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "hyphen",
/* 0x80 */
  "Abreve", "Aogonek", "Cacute", "Ccaron", "Dcaron", "Ecaron", "Eogonek", "Gbreve",
  "Lacute", "Lcaron", "Lslash", "Nacute", "Ncaron", "Ng", "Ohungarumlaut", "Racute",
/* 0x90 */
  "Rcaron", "Sacute", "Scaron", "Scedilla",
  "Tcaron", "Tcedilla", "Uhungarumlaut", "Uring",
  "Ydieresis", "Zacute", "Zcaron", "Zdotaccent", "IJ", "Idotaccent", "dbar", "section",
/* 0xA0 */
  "abreve", "aogonek", "cacute", "ccaron", "dcaron", "ecaron", "eogonek", "gbreve",
  "lacute", "lcaron", "lslash", "nacute", "ncaron", "ng", "ohungarumlaut", "racute",
/* 0xB0 */
  "rcaron", "sacute", "scaron", "scedilla",
  "tquoteright", "tcedilla", "uhungarumlaut", "uring",
  "ydieresis", "zacute", "zcaron", "zdotaccent",
  "ij", "exclamdown", "questiondown", "sterling",
/* 0xC0 */
  "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring", "AE", "Ccedilla",
  "Egrave", "Eacute", "Ecircumflex", "Edieresis",
  "Igrave", "Iacute", "Icircumflex", "Idieresis",
/* 0xD0 */
  "Eth", "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "OE",
  "Oslash", "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "Germandbls",
/* 0xE0 */
  "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
  "egrave", "eacute", "ecircumflex", "edieresis",
  "igrave", "iacute", "icircumflex", "idieresis",
/* 0xF0 */
  "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "oe",
  "oslash", "ugrave", "uacute", "ucircumflex", "udieresis", "yacute", "thorn", "germandbls"
	}},
	{"TeXBase1",
	{"", "dotaccent", "fi", "fl", "fraction", "hungarumlaut", "Lslash", "lslash",
	 "ogonek", "ring", "", "breve", "minus", "", "Zcaron", "zcaron",
	 "caron", "dotlessi", "dotlessj", "ff", "ffi", "ffl", "", "",
	 "", "", "", "", "", "", "grave", "quotesingle",
	 "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quoteright",
	 "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
	 "zero", "one", "two", "three", "four", "five", "six", "seven",
	 "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
	 "at", "A", "B", "C", "D", "E", "F", "G",
	 "H", "I", "J", "K", "L", "M", "N", "O",
	 "P", "Q", "R", "S", "T", "U", "V", "W",
	 "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
	 "quoteleft", "a", "b", "c", "d", "e", "f", "g",
	 "h", "i", "j", "k", "l", "m", "n", "o",
	 "p", "q", "r", "s", "t", "u", "v", "w",
	 "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "",
	 "Euro", "", "quotesinglbase", "florin", "quotedblbase", "ellipsis", "dagger", "daggerdbl",
	 "circumflex", "perthousand", "Scaron", "guilsinglleft", "OE", "", "", "",
	 "", "", "", "quotedblleft", "quotedblright", "bullet", "endash", "emdash",
	 "tilde", "trademark", "scaron", "guilsinglright", "oe", "", "", "Ydieresis",
	 "", "exclamdown", "cent", "sterling", "currency", "yen", "brokenbar", "section",
	 "dieresis", "copyright", "ordfeminine", "guillemotleft", "logicalnot", "hyphen", "registered", "macron",
	 "degree", "plusminus", "twosuperior", "threesuperior", "acute", "mu", "paragraph", "periodcentered",
	 "cedilla", "onesuperior", "ordmasculine", "guillemotright", "onequarter", "onehalf", "threequarters", "questiondown",
	 "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring", "AE", "Ccedilla",
	 "Egrave", "Eacute", "Ecircumflex", "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis",
	 "Eth", "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply",
	 "Oslash", "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "germandbls",
	 "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
	 "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex", "idieresis",
	 "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "divide",
	 "oslash", "ugrave", "uacute", "ucircumflex", "udieresis", "yacute", "thorn", "ydieresis" } }
	};
