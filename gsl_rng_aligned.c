/*
 * Aligned version of GSL rng allocation and cloning
 * Based on rng/rng.c of GSL version 2.7.1
 *
 * Copyright (C) 1996, 1997, 1998, 1999, 2000, 2007 James Theiler, Brian Gough
 * Copyright (C) 2024 Giovanni Garberoglio
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsl_rng_aligned.h"

gsl_rng *
gsl_rng_alloc_aligned (const gsl_rng_type * T)
{

  gsl_rng *r;
  
  if(posix_memalign((void *)&r, CACHELINE_SIZE, sizeof(gsl_rng)))
    {
      GSL_ERROR_VAL ("failed to allocate space for rng struct",
                        GSL_ENOMEM, 0);
    };

  if(posix_memalign((void *)&r->state, CACHELINE_SIZE, T->size))
    {
      free (r);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to allocate space for rng state",
                        GSL_ENOMEM, 0);
    };
  memset(r->state, 0, T->size); /* zero out the state */
  
  r->type = T;

  gsl_rng_set (r, gsl_rng_default_seed);        /* seed the generator */

  return r;
}

gsl_rng *
gsl_rng_clone_aligned (const gsl_rng * q)
{

  gsl_rng *r;
  
  if(posix_memalign((void *)&r, CACHELINE_SIZE, sizeof(gsl_rng)))
    {
      GSL_ERROR_VAL ("failed to allocate space for rng struct",
                        GSL_ENOMEM, 0);
    };

  if(posix_memalign((void *)&r->state, CACHELINE_SIZE, q->type->size))
    
    {
      free (r);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to allocate space for rng state",
                        GSL_ENOMEM, 0);
    };
  memset(r->state, 0, q->type->size); /* zero out the state */
  
  r->type = q->type;

  memcpy (r->state, q->state, q->type->size);

  return r;
}
