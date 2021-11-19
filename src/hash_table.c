/*********************************************************************
 *
 * Copyright (C) 2001-2002,  Simon Kagstrom
 *
 * Filename:      hash_table.c
 * Description:   The implementation of the hash table (MK2).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 *
 ********************************************************************/

#include <stdlib.h> /* malloc */
#include <stdio.h>  /* perror */
#include <errno.h>  /* errno */
#include <string.h> /* memcmp */
#include <assert.h> /* assert */

#include "ght_hash_table.h"

#if defined(WIN32) || defined(__hpux)
#define PSLIB_INLINE
#else
#define PSLIB_INLINE inline
#endif

/* Flags for the elements. This is currently unused. */
#define FLAGS_NONE     0 /* No flags */
#define FLAGS_NORMAL   0 /* Normal item. All user-inserted stuff is normal */
#define FLAGS_INTERNAL 1 /* The item is internal to the hash table */

/* Prototypes */
static PSLIB_INLINE void              transpose(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry);
static PSLIB_INLINE void              move_to_front(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry);
static PSLIB_INLINE void              free_entry_chain(ght_hash_table_t *p_ht, ght_hash_entry_t *p_entry);
static PSLIB_INLINE ght_hash_entry_t *search_in_bucket(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_key_t *p_key, unsigned char i_heuristics);

static PSLIB_INLINE void              hk_fill(ght_hash_key_t *p_hk, int i_size, void *p_key);
static PSLIB_INLINE ght_hash_entry_t *he_create(ght_hash_table_t *p_ht, void *p_data, unsigned int i_key_size, void *p_key_data);
static PSLIB_INLINE void              he_finalize(ght_hash_table_t *p_ht, ght_hash_entry_t *p_he);

/* --- private methods --- */

/* Move p_entry one up in its list. */
static PSLIB_INLINE void transpose(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry)
{
  /*
   *  __    __    __    __
   * |A_|->|X_|->|Y_|->|B_|
   *             /
   * =>        p_entry
   *  __    __/   __    __
   * |A_|->|Y_|->|X_|->|B_|
   */
  if (p_entry->p_prev) /* Otherwise p_entry is already first. */
    {
      ght_hash_entry_t *p_x = p_entry->p_prev;
      ght_hash_entry_t *p_a = p_x?p_x->p_prev:NULL;
      ght_hash_entry_t *p_b = p_entry->p_next;

      if (p_a)
	{
	  p_a->p_next = p_entry;
	}
      else /* This element is now placed first */
	{
	  p_ht->pp_entries[l_bucket] = p_entry;
	}

      if (p_b)
	{
	  p_b->p_prev = p_x;
	}
      if (p_x)
	{
	  p_x->p_next = p_entry->p_next;
	  p_x->p_prev = p_entry;
	}
      p_entry->p_next = p_x;
      p_entry->p_prev = p_a;
    }
}

/* Move p_entry first */
static PSLIB_INLINE void move_to_front(ght_hash_table_t *p_ht, ght_uint32_t l_bucket, ght_hash_entry_t *p_entry)
{
  /*
   *  __    __    __
   * |A_|->|B_|->|X_|
   *            /
   * =>  p_entry
   *  __/   __    __
   * |X_|->|A_|->|B_|
   */
  if (p_entry == p_ht->pp_entries[l_bucket])
    {
      return;
    }

  /* Link p_entry out of the list. */
  p_entry->p_prev->p_next = p_entry->p_next;
  if (p_entry->p_next)
    {
      p_entry->p_next->p_prev = p_entry->p_prev;
    }

  /* Place p_entry first */
  p_entry->p_next = p_ht->pp_entries[l_bucket];
  p_entry->p_prev = NULL;
  p_ht->pp_entries[l_bucket]->p_prev = p_entry;
  p_ht->pp_entries[l_bucket] = p_entry;
}

/* Search for an element in a bucket */
static PSLIB_INLINE ght_hash_entry_t *search_in_bucket(ght_hash_table_t *p_ht, ght_uint32_t l_bucket,
						 ght_hash_key_t *p_key, unsigned char i_heuristics)
{
  ght_hash_entry_t *p_e;

  for (p_e = p_ht->pp_entries[l_bucket];
       p_e;
       p_e = p_e->p_next)
    {
      if ((p_e->key.i_size == p_key->i_size) &&
	  (memcmp(p_e->key.p_key, p_key->p_key, p_e->key.i_size) == 0))
	{
	  /* Matching entry found - Apply heuristics, if any */
	  switch (i_heuristics)
	    {
	    case GHT_HEURISTICS_MOVE_TO_FRONT:
	      move_to_front(p_ht, l_bucket, p_e);
	      break;
	    case GHT_HEURISTICS_TRANSPOSE:
	      transpose(p_ht, l_bucket, p_e);
	      break;
	    default:
	      break;
	    }
	  return p_e;
	}
    }
  return NULL;
}

/* Free a chain of entries (in a bucket) */
static PSLIB_INLINE void free_entry_chain(ght_hash_table_t *p_ht, ght_hash_entry_t *p_entry)
{
  ght_hash_entry_t *p_e = p_entry;

  while(p_e)
    {
      ght_hash_entry_t *p_e_next = p_e->p_next;
      he_finalize(p_ht, p_e);
      p_e = p_e_next;
    }
}


/* Fill in the data to a existing hash key */
static PSLIB_INLINE void hk_fill(ght_hash_key_t *p_hk, int i_size, void *p_key)
{
  assert(p_hk);

  p_hk->i_size = i_size;
  p_hk->p_key = p_key;
}

/* Create an hash entry */
static PSLIB_INLINE ght_hash_entry_t *he_create(ght_hash_table_t *p_ht, void *p_data,
					  unsigned int i_key_size, void *p_key_data)
{
  ght_hash_entry_t *p_he;

  /*
   * An element like the following is allocated:
   *        elem->p_key
   *       /   elem->p_key->p_key_data
   *  ____|___/________
   * |elem|key|key data|
   * |____|___|________|
   *
   * That is, the key and the key data is stored "inline" within the
   * hash entry.
   *
   * This saves space since malloc only is called once and thus avoids
   * some fragmentation. Thanks to Dru Lemley for this idea.
   */
  if ( !(p_he = (ght_hash_entry_t*)p_ht->fn_alloc (sizeof(ght_hash_entry_t)+i_key_size, p_ht->userdata)) )
    {
      fprintf(stderr, "fn_alloc failed!\n");
      return NULL;
    }

  p_he->p_data = p_data;
  p_he->p_next = NULL;
  p_he->p_prev = NULL;

  /* Create the key */
  p_he->key.i_size = i_key_size;
  p_he->key.p_key = (void*)(p_he+1);
  memcpy(p_he->key.p_key, p_key_data, i_key_size);

  return p_he;
}

/* Finalize (free) a hash entry */
static PSLIB_INLINE void he_finalize(ght_hash_table_t *p_ht, ght_hash_entry_t *p_he)
{
  assert(p_he);

#if !defined(NDEBUG)
  p_he->p_next = NULL;
  p_he->p_prev = NULL;
#endif /* NDEBUG */

  /* Free the entry */
  p_ht->fn_free(p_he, p_ht->userdata);
}

/* Allocate memory */
static void *ght_malloc(size_t size, void *data)
{
	return(malloc(size));
}

/* Free memory */
static void ght_free(void *ptr, void *data)
{
	free(ptr);
}

/* --- Exported methods --- */
/* Create a new hash table */
ght_hash_table_t *ght_create(unsigned int i_size)
{
  ght_hash_table_t *p_ht;
  int i=0;

  if ( !(p_ht = (ght_hash_table_t*)malloc (sizeof(ght_hash_table_t))) )
    {
      perror("malloc");
      return NULL;
    }

  /* Set the size of the hash table to the nearest 2^i higher then i_size */
  p_ht->i_size = 0;
  while(p_ht->i_size < i_size)
    {
      p_ht->i_size = 1<<i++;
    }

  p_ht->i_size_mask = (1<<(i-1))-1; /* Mask to & with */
  p_ht->i_items = 0;

  p_ht->fn_hash = ght_one_at_a_time_hash;

  /* Standard values for allocations */
  p_ht->fn_alloc = ght_malloc;
  p_ht->fn_free = ght_free;
	p_ht->userdata = NULL;

  /* Set flags */
  p_ht->i_heuristics = GHT_HEURISTICS_NONE;
  p_ht->i_automatic_rehash = FALSE;

  /* Create an empty bucket list. */
  if ( !(p_ht->pp_entries = (ght_hash_entry_t**)malloc(p_ht->i_size*sizeof(ght_hash_entry_t*))) )
    {
      perror("malloc");
      free(p_ht);
      return NULL;
    }
  memset(p_ht->pp_entries, 0, p_ht->i_size*sizeof(ght_hash_entry_t*));

  /* Initialise the number of entries in each bucket to zero */
  if ( !(p_ht->p_nr = (int*)malloc(p_ht->i_size*sizeof(int))))
    {
      perror("malloc");
      free(p_ht->pp_entries);
      free(p_ht);
      return NULL;
    }
  memset(p_ht->p_nr, 0, p_ht->i_size*sizeof(int));

  return p_ht;
}

/* Set the allocation/deallocation function to use */
void ght_set_alloc(ght_hash_table_t *p_ht, ght_fn_alloc_t fn_alloc, ght_fn_free_t fn_free, void *data)
{
  p_ht->fn_alloc = fn_alloc;
  p_ht->fn_free = fn_free;
	p_ht->userdata = data;
}

/* Set the hash function to use */
void ght_set_hash(ght_hash_table_t *p_ht, ght_fn_hash_t fn_hash)
{
  p_ht->fn_hash = fn_hash;
}

/* Set the heuristics to use. */
void ght_set_heuristics(ght_hash_table_t *p_ht, int i_heuristics)
{
  p_ht->i_heuristics = i_heuristics;
}

/* Set the rehashing status of the table. */
void ght_set_rehash(ght_hash_table_t *p_ht, int b_rehash)
{
  p_ht->i_automatic_rehash = b_rehash;
}

/* Get the number of items in the hash table */
unsigned int ght_size(ght_hash_table_t *p_ht)
{
  return p_ht->i_items;
}

/* Get the size of the hash table */
unsigned int ght_table_size(ght_hash_table_t *p_ht)
{
  return p_ht->i_size;
}

/* Insert an entry into the hash table */
int ght_insert(ght_hash_table_t *p_ht,
	       void *p_entry_data,
	       unsigned int i_key_size, void *p_key_data)
{
  ght_hash_entry_t *p_entry;
  ght_uint32_t l_key;
  ght_hash_key_t key;

  assert(p_ht);

  hk_fill(&key, i_key_size, p_key_data);
  l_key = p_ht->fn_hash(&key)&p_ht->i_size_mask;
  if (search_in_bucket(p_ht, l_key, &key, 0))
    {
      /* Don't insert if the key is already present. */
      return -1;
    }
  if (!(p_entry = he_create(p_ht, p_entry_data,
			    i_key_size, p_key_data)))
    {
      return -2;
    }

  /* Rehash if the number of items inserted is too high. */
  if (p_ht->i_automatic_rehash && p_ht->i_items > 2*p_ht->i_size)
    {
      ght_rehash(p_ht, 2*p_ht->i_size);
    }

  /* Place the entry first in the list. */
  p_entry->p_next = p_ht->pp_entries[l_key];
  p_entry->p_prev = NULL;
  if (p_ht->pp_entries[l_key])
    {
      p_ht->pp_entries[l_key]->p_prev = p_entry;
    }
  p_ht->pp_entries[l_key] = p_entry;
  p_ht->p_nr[l_key]++;

  assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );

  p_ht->i_items++;

  return 0;
}

/* Get an entry from the hash table. The entry is returned, or NULL if it wasn't found */
void *ght_get(ght_hash_table_t *p_ht,
	      unsigned int i_key_size, void *p_key_data)
{
  ght_hash_entry_t *p_e;
  ght_hash_key_t key;
  ght_uint32_t l_key;

  assert(p_ht);

  hk_fill(&key, i_key_size, p_key_data);

  l_key = p_ht->fn_hash(&key)&p_ht->i_size_mask;

  /* Check that the first element in the list really is the first. */
  assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );

  p_e = search_in_bucket(p_ht, l_key, &key, p_ht->i_heuristics);
  return (p_e?p_e->p_data:NULL);
}

/* Replace an entry from the hash table. The entry is returned, or NULL if it wasn't found */
void *ght_replace(ght_hash_table_t *p_ht,
		  void *p_entry_data,
		  unsigned int i_key_size, void *p_key_data)
{
  ght_hash_entry_t *p_e;
  ght_hash_key_t key;
  ght_uint32_t l_key;
  void *p_old;

  assert(p_ht);

  hk_fill(&key, i_key_size, p_key_data);

  l_key = p_ht->fn_hash(&key)&p_ht->i_size_mask;

  /* Check that the first element in the list really is the first. */
  assert( p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1 );

  p_e = search_in_bucket(p_ht, l_key, &key, p_ht->i_heuristics);

  if ( !p_e )
    return NULL;

  p_old = p_e->p_data;
  p_e->p_data = p_entry_data;

  return p_old;
}

/* Remove an entry from the hash table. The removed entry, or NULL, is
   returned (and NOT free'd). */
void *ght_remove(ght_hash_table_t *p_ht,
		 unsigned int i_key_size, void *p_key_data)
{
  ght_hash_entry_t *p_out;
  ght_hash_key_t key;
  ght_uint32_t l_key;
  void *p_ret=NULL;

  assert(p_ht);

  hk_fill(&key, i_key_size, p_key_data);
  l_key = p_ht->fn_hash(&key)&p_ht->i_size_mask;

  /* Check that the first element really is the first */
  assert( (p_ht->pp_entries[l_key]?p_ht->pp_entries[l_key]->p_prev == NULL:1) );

  p_out = search_in_bucket(p_ht, l_key, &key, 0);

  /* Link p_out out of the list. */
  if (p_out)
    {
      if (p_out->p_prev)
	{
	  p_out->p_prev->p_next = p_out->p_next;
	}
      else /* first in list */
	{
	  p_ht->pp_entries[l_key] = p_out->p_next;
	}
      if (p_out->p_next)
	{
	  p_out->p_next->p_prev = p_out->p_prev;
	}

      /* This should ONLY be done for normal items (for now all items) */
      p_ht->i_items--;

      p_ht->p_nr[l_key]--;
#if !defined(NDEBUG)
      p_out->p_next = NULL;
      p_out->p_prev = NULL;
#endif /* NDEBUG */

      p_ret = p_out->p_data;
      he_finalize(p_ht, p_out);
    }

  return p_ret;
}

/* Get the first entry in an iteration */
void *ght_first(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, void **pp_key)
{
  assert(p_ht && p_iterator);

  /* Fill the iterator */
  p_iterator->p_entry = p_ht->pp_entries[0];
  p_iterator->i_curr_bucket = 0;

  /* Step until non-empty bucket */
  for (; (p_iterator->i_curr_bucket < p_ht->i_size) && !p_ht->pp_entries[p_iterator->i_curr_bucket];
       p_iterator->i_curr_bucket++);
  if (p_iterator->i_curr_bucket < p_ht->i_size)
    {
      p_iterator->p_entry = p_ht->pp_entries[p_iterator->i_curr_bucket];
    }

  if (p_iterator->p_entry)
    {
      p_iterator->p_next = p_iterator->p_entry->p_next;
      *pp_key = p_iterator->p_entry->key.p_key;

      return p_iterator->p_entry->p_data;
    }

  p_iterator->p_next = NULL;
  *pp_key = NULL;

  return NULL;
}

/* Get the next entry in an iteration. You have to call ght_first
   once initially before you use this function */
void *ght_next(ght_hash_table_t *p_ht, ght_iterator_t *p_iterator, void **pp_key)
{
  assert(p_ht && p_iterator);

  if (p_iterator->p_next)
    {
      /* More entries in the current bucket */
      p_iterator->p_entry = p_iterator->p_next;
      p_iterator->p_next = p_iterator->p_next->p_next;

      *pp_key = p_iterator->p_entry->key.p_key;
      return p_iterator->p_entry->p_data; /* We know that this is non-NULL */
    }
  else
    {
      p_iterator->p_entry = NULL;
      p_iterator->i_curr_bucket++;
    }

  /* Step until non-empty bucket */
  for (; (p_iterator->i_curr_bucket < p_ht->i_size) && !p_ht->pp_entries[p_iterator->i_curr_bucket];
       p_iterator->i_curr_bucket++);

  /* FIXME: Add someplace here:
   *  if (p_iterator->p_entry->i_flags & FLAGS_INTERNAL)
   *     return ght_next(p_ht, p_iterator);
   */
  if (p_iterator->i_curr_bucket < p_ht->i_size)
    {
      p_iterator->p_entry = p_ht->pp_entries[p_iterator->i_curr_bucket];

      if (p_iterator->p_entry)
	{
	  p_iterator->p_next = p_iterator->p_entry->p_next;
	}

      *pp_key = p_iterator->p_entry->key.p_key;
      return p_iterator->p_entry->p_data;
    }
  else
    {
      /* Last entry */
      p_iterator->i_curr_bucket = 0;
      p_iterator->p_entry = NULL;
      p_iterator->p_next = NULL;

      *pp_key = NULL;
      return NULL;
    }
}

/* Finalize (free) a hash table */
void ght_finalize(ght_hash_table_t *p_ht)
{
  int i;

  assert(p_ht);

  if (p_ht->pp_entries)
    {
      /* For each bucket, free all entries */
      for (i=0; i<p_ht->i_size; i++)
	{
	  free_entry_chain(p_ht, p_ht->pp_entries[i]);
	  p_ht->pp_entries[i] = NULL;
	}
      free (p_ht->pp_entries);
      p_ht->pp_entries = NULL;
    }
  if (p_ht->p_nr)
    {
      free (p_ht->p_nr);
      p_ht->p_nr = NULL;
    }

  free (p_ht);
}

/* Rehash the hash table (i.e. change its size and reinsert all
 * items). This operation is slow and should not be used frequently.
 */
void ght_rehash(ght_hash_table_t *p_ht, unsigned int i_size)
{
  ght_hash_table_t *p_tmp;
  ght_iterator_t iterator;
  void *p_key;
  void *p;
  int i;

  assert(p_ht);

  /* Recreate the hash table with the new size */
  p_tmp = ght_create(i_size);
  assert(p_tmp);

  /* Set the flags for the new hash table */
  ght_set_hash(p_tmp, p_ht->fn_hash);
  ght_set_heuristics(p_tmp, 0);
  ght_set_rehash(p_tmp, FALSE);

  /* Walk through all elements in the table and insert them into the temporary one. */
  for (p = ght_first(p_ht, &iterator, &p_key); p; p = ght_next(p_ht, &iterator, &p_key))
    {
      assert(iterator.p_entry);

      /* Insert the entry into the new table */
      if (ght_insert(p_tmp,
		     iterator.p_entry->p_data,
		     iterator.p_entry->key.i_size, iterator.p_entry->key.p_key) < 0)
	{
	  fprintf(stderr, "hash_table.c ERROR: Out of memory error or entry already in hash table\n"
		  "when rehashing (internal error)\n");
	}
    }

  /* Remove the old table... */
  for (i=0; i<p_ht->i_size; i++)
    {
      if (p_ht->pp_entries[i])
	{
	  /* Delete the entries in the bucket */
	  free_entry_chain (p_ht, p_ht->pp_entries[i]);
	  p_ht->pp_entries[i] = NULL;
	}
    }

  free (p_ht->pp_entries);
  free (p_ht->p_nr);

  /* ... and replace it with the new */
  p_ht->i_size = p_tmp->i_size;
  p_ht->i_size_mask = p_tmp->i_size_mask;
  p_ht->i_items = p_tmp->i_items;
  p_ht->pp_entries = p_tmp->pp_entries;
  p_ht->p_nr = p_tmp->p_nr;

  /* Clean up */
  p_tmp->pp_entries = NULL;
  p_tmp->p_nr = NULL;
  free (p_tmp);
}
