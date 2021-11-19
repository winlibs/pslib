#ifndef __PS_ERROR_H__
#define __PS_ERROR_H__
void _ps_errorhandler(PSDoc *p, int error, const char *str, void *data);
void ps_error(PSDoc *p, int type, const char *fmt, ...);
#endif
