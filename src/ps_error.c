#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "ps_intern.h"

/* _ps_errorhandler() {{{
 * Default error handler, set if none is provided by application.
 */
void _ps_errorhandler(PSDoc *p, int error, const char *str, void *data) {
	fprintf(stderr, "PSLib: %s\n", str);
}
/* }}} */

/* ps_error() {{{
 * Output an error using the errorhandler for the given document.
 */
void ps_error(PSDoc *p, int type, const char *fmt, ...) {
	char msg[256];
	va_list ap;

	if(type == PS_Warning && p->warnings == ps_false)
		return;

	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);

	if (!p->in_error) {
		p->in_error = 1; /* avoid recursive errors */
		(p->errorhandler)(p, type, msg, p->user_data);
	}

	/* If the error handler returns the error was non-fatal */
	p->in_error = 0;

	va_end(ap);
}
/* }}} */

