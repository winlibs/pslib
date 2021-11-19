#include <string.h>
#include "ps_intern.h"

#define NUM_INPUTENCODINGS 4

ENCODING inputencoding[NUM_INPUTENCODINGS] = {
  {"ISO-8859-1",{
/*0x00*/  "", "", "", "", "", "", "", "",
/*0x08*/  "", "", "", "", "", "", "", "",
/*0x10*/  "", "", "", "", "", "", "", "", //0x10
/*0x18*/  "", "", "", "", "", "", "", "", //0x18
/*0x20*/  "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle",
/*0x28*/  "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
/*0x30*/  "zero", "one", "two", "three", "four", "five", "six", "seven",
/*0x38*/  "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
/*0x40*/  "at", "A", "B", "C", "D", "E", "F", "G",
/*0x48*/  "H", "I", "J", "K", "L", "M", "N", "O",
/*0x50*/  "P", "Q", "R", "S", "T", "U", "V", "W",
/*0x58*/  "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
/*0x60*/  "grave", "a", "b", "c", "d", "e", "f", "g",
/*0x68*/  "h", "i", "j", "k", "l", "m", "n", "o",
/*0x70*/  "p", "q", "r", "s", "t", "u", "v", "w",
/*0x78*/  "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "",
/*0x80*/  "", "", "", "", "", "", "", "",
/*0x88*/  "", "", "", "", "", "", "", "",
/*0x90*/  "dotlessi", "grave", "acute", "circumflex", "tilde", "macron", "breve", "dotaccent",
/*0x98*/  "dieresis", "", "ring", "cedilla", "", "hungarumlaut", "ogonek", "caron",
/*0xA0*/  "space", "exclamdown", "cent", "sterling", "Euro", "yen", "brokenbar", "section",
/*0xA8*/  "dieresis", "copyright", "ordfeminine", "guillemotleft", "logicalnot", "hyphen", "registered", "macron",
/*0xB0*/  "degree", "plusminus", "twosuperior", "threesuperior", "acute", "mu", "paragraph", "periodcentered",
/*0xB8*/  "cedilla", "onesuperior", "ordmasculine", "guillemotright", "onequarter", "onehalf", "threequarters", "questiondown",
/*0xC0*/  "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring", "AE", "Ccedilla",
/*0xC8*/  "Egrave", "Eacute", "Ecircumflex", "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis",
/*0xD0*/  "Eth", "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply",
/*0xD8*/  "Oslash", "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "germandbls",
/*0xE0*/  "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
/*0xE8*/  "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex", "idieresis",
/*0xF0*/  "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "divide",
/*0xF8*/  "oslash", "ugrave", "uacute", "ucircumflex", "udieresis", "yacute", "thorn", "ydieresis" } },
	{"ISO-8859-2",
	{"", "", "", "", "", "", "", "",
	 "", "", "", "", "", "", "", "",
	 "", "", "", "", "", "", "", "",
	 "", "", "", "", "", "", "", "",
	 "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
    "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
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
	 "", "", "", "", "", "", "", "",
	 "", "", "", "", "", "", "", "",
	 "dotlessi", "grave", "acute", "circumflex", "tilde", "macron", "breve", "dotaccent",
	 "dieresis", "", "ring", "cedilla", "", "hungarumlaut", "ogonek", "caron",
	 "space", "Aogonek", "breve", "Lslash", "currency", "Lcaron", "Sacute", "section",
	 "dieresis", "Scaron", "Scedilla", "Tcaron", "Zacute", "hyphen", "Zcaron", "Zdotaccent",
	 "ring", "aogonek", "ogonek", "lslash", "acute", "lcaron", "sacute", "caron",
	 "cedilla", "scaron", "scedilla", "tcaron", "zacute", "hungarumlaut", "zcaron", "zdotaccent",
	 "Racute", "Aacute", "Acircumflex", "Abreve", "Adieresis", "Lacute", "Cacute", "Ccedilla",
	 "Ccaron", "Eacute", "Eogonek", "Edieresis", "Ecaron", "Iacute", "Icircumflex", "Dcaron",
	 "Dcroat", "Nacute", "Ncaron", "Oacute", "Ocircumflex", "Ohungarumlaut", "Odieresis", "multiply",
	 "Rcaron", "Uring", "Uacute", "Uhungarumlaut", "Udieresis", "Yacute", "Tcommaaccent", "germandbls",
	 "racute", "aacute", "acircumflex", "abreve", "adieresis", "lacute", "cacute", "ccedilla",
	 "ccaron", "eacute", "eogonek", "edieresis", "ecaron", "iacute", "icircumflex", "dcaron",
	 "dcroat", "nacute", "ncaron", "oacute", "ocircumflex", "ohungerumlaut", "odieresis", "divide",
    "rcaron", "uring", "uacute", "uhungarumlaut", "udieresis", "yacute", "tcommaaccent", "dotaccent" }
  },
  {"ISO-8859-15",{
/*0x00*/  "", "", "", "", "", "", "", "",
/*0x08*/  "", "", "", "", "", "", "", "",
/*0x10*/  "", "", "", "", "", "", "", "", //0x10
/*0x18*/  "", "", "", "", "", "", "", "", //0x18
/*0x20*/  "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
/*0x28*/  "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
/*0x30*/  "zero", "one", "two", "three", "four", "five", "six", "seven",
/*0x38*/  "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
/*0x40*/  "at", "A", "B", "C", "D", "E", "F", "G",
/*0x48*/  "H", "I", "J", "K", "L", "M", "N", "O",
/*0x50*/  "P", "Q", "R", "S", "T", "U", "V", "W",
/*0x58*/  "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
/*0x60*/  "quoteleft", "a", "b", "c", "d", "e", "f", "g",
/*0x68*/  "h", "i", "j", "k", "l", "m", "n", "o",
/*0x70*/  "p", "q", "r", "s", "t", "u", "v", "w",
/*0x78*/  "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "",
/*0x80*/  "", "", "", "", "", "", "", "",
/*0x88*/  "", "", "", "", "", "", "", "",
/*0x90*/  "dotlessi", "grave", "acute", "circumflex", "tilde", "macron", "breve", "dotaccent",
/*0x98*/  "dieresis", "", "ring", "cedilla", "", "hungarumlaut", "ogonek", "caron",
/*0xA0*/  "space", "exclamdown", "cent", "sterling", "Euro", "yen", "Scaron", "section",
/*0xA8*/  "scaron", "copyright", "ordfeminine", "guillemotleft", "logicalnot", "hyphen", "registered", "macron",
/*0xB0*/  "degree", "plusminus", "twosuperior", "threesuperior", "Zcaron", "mu", "paragraph", "periodcentered",
/*0xB8*/  "zcaron", "onesuperior", "ordmasculine", "guillemotright", "OE", "oe", "Ydieresis", "questiondown",
/*0xC0*/  "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring", "AE", "Ccedilla",
/*0xC8*/  "Egrave", "Eacute", "Ecircumflex", "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis",
/*0xD0*/  "Eth", "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply",
/*0xD8*/  "Oslash", "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "germandbls",
/*0xE0*/  "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
/*0xE8*/  "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex", "idieresis",
/*0xF0*/  "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "divide",
/*0xF8*/  "oslash", "ugrave", "uacute", "ucircumflex", "udieresis", "yacute", "thorn", "ydieresis" } },
	 {NULL, NULL}
	};

ENCODING *ps_get_inputencoding(const char *name) {
	int i;

	for(i=0; i<NUM_INPUTENCODINGS; i++) {
#ifdef WIN32
		if(0 == _stricmp(name, inputencoding[i].name)) {
#else
		if(0 == strcasecmp(name, inputencoding[i].name)) {
#endif
			return(&inputencoding[i]);
		}
	}
	return(NULL);
}

