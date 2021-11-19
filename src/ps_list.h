/****************************************************************************
*
*					Copyright (C) 1991 Kendall Bennett.
*							All rights reserved.
*
* Filename:		$RCSfile$
* Version:		$Revision$
*
* Language:		ANSI C
* Environment:	any
*
* Description:	Header file for doubly linked list routines.
*
* $Id$
*
* Revision History:
* -----------------
*
* $Log$
* Revision 1.1  2002/12/17 10:37:20  steinm
* - Moved ps_list.h in source directory since it shouldn't go
*   into the development packages
*
* Revision 1.2  2002/10/22 09:08:36  steinm
* - ps_begin_page uses page dimension for boundig box
* - list operations use user malloc, realloc, free functions
* - many bug fixes
*
* Revision 1.1  2002/10/12 21:34:06  steinm
* - Many bugfixes
* - Working text output with kerning
* - resource and parameter management
*
* Revision 1.1.1.1  03/21/00 19:24:36 MET  steinm
* Imported sources
*
* Revision 1.1  1999/07/19 18:58:44  andrey
* Moving dlist stuff into core.
*
* Revision 1.1  1999/04/21 23:11:20  ssb
* moved apache, com and hyperwave into ext/
*
* Revision 1.1.1.1  1999/04/07 21:03:20  zeev
* PHP 4.0
*
* Revision 1.1.1.1  1999/03/17 04:29:11  andi
* PHP4
*
* Revision 1.1.1.1  1998/12/21 07:56:22  andi
* Trying to start the zend CVS tree
*
* Revision 1.2  1998/08/14 15:51:12  shane
* Some work on getting hyperwave to work on windows.  hg_comm needs a lot of work.
*
* Mainly, signals, fnctl, bzero, bcopy, etc are not portable functions.  Most of this
* will have to be rewriten for windows.
*
* Revision 1.1  1998/08/12 09:29:16  steinm
* First version of Hyperwave module.
*
* Revision 1.5  91/12/31  19:40:54  kjb
* 
* Modified include files directories.
* 
* Revision 1.4  91/09/27  03:10:41  kjb
* Added compatibility with C++.
* 
* Revision 1.3  91/09/26  10:07:16  kjb
* Took out extern references
* 
* Revision 1.2  91/09/01  19:37:20  ROOT_DOS
* Changed DLST_TAIL macro so that it returns a pointer to the REAL last
* node of the list, not the dummy last node (l->z).
* 
* Revision 1.1  91/09/01  18:38:23  ROOT_DOS
* Initial revision
* 
****************************************************************************/

#ifndef	__PS_LIST_H
#define	__PS_LIST_H

#ifndef	__DEBUG_H
/*#include "debug.h"*/
#endif

/*---------------------- Macros and type definitions ----------------------*/

typedef struct PS_DLST_BUCKET {
	struct PS_DLST_BUCKET	*next;
	struct PS_DLST_BUCKET	*prev;
	} PS_DLST_BUCKET;

/* necessary for AIX 4.2.x */

#ifdef hz
#undef hz
#endif
typedef struct dlist_ DLIST;
struct dlist_ {
	int			count;			/* Number of elements currently in list	*/
	PS_DLST_BUCKET	*head;			/* Pointer to head element of list		*/
	PS_DLST_BUCKET	*z;				/* Pointer to last node of list			*/
	PS_DLST_BUCKET	hz[2];			/* Space for head and z nodes			*/
	void  *(*malloc)(PSDoc *ps, size_t size, const char *caller);
	void  *(*realloc)(PSDoc *ps, void *mem, size_t size, const char *caller);
	void   (*free)(PSDoc *ps, void *mem);
	};

/* Return a pointer to the user space given the address of the header of
 * a node.
 */

#define	PS_DLST_USERSPACE(h)	((void*)((PS_DLST_BUCKET*)(h) + 1))

/* Return a pointer to the header of a node, given the address of the
 * user space.
 */

#define	PS_DLST_HEADER(n)		((PS_DLST_BUCKET*)(n) - 1)

/* Return a pointer to the user space of the list's head node. This user
 * space does not actually exist, but it is useful to be able to address
 * it to enable insertion at the start of the list.
 */

#define	PS_DLST_HEAD(l)		PS_DLST_USERSPACE((l)->head)

/* Return a pointer to the user space of the last node in list.	*/

#define	PS_DLST_TAIL(l)		PS_DLST_USERSPACE((l)->z->prev)

/* Determine if a list is empty
 */

#define	PS_DLST_EMPTY(l)		((l)->count == 0)

/*-------------------------- Function Prototypes --------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

void *dlst_newnode(DLIST *l, int size);
void dlst_freenode(DLIST *l, void *node);
DLIST *dlst_init(void* (*allocproc)(PSDoc *ps, size_t size, const char *caller), void* (*reallocproc)(PSDoc *ps, void *mem, size_t size, const char *caller), void  (*freeproc)(PSDoc *ps, void *mem));
void dlst_kill(DLIST *l,void (*freeNode)(DLIST *l, void *node));
void dlst_insertafter(DLIST *l,void *node,void *after);
void *dlst_deletenext(DLIST *l,void *node);
void *dlst_first(DLIST *l);
void *dlst_last(DLIST *l);
void *dlst_next(void *prev);
void *dlst_prev(void *next);
void dlst_mergesort(DLIST *l,int (*cmp_func)(void*,void*));

#ifdef __cplusplus
}
#endif

#endif
