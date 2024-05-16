/*
 * Functions to have aligned states of GSL random number generators.
 * Might be useful to avoid False Sharing when using OpenMP integrations
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

#include <gsl/gsl_rng.h>

#ifdef __APPLE__
#define CACHELINE_SIZE 128 /* Apple's M series processors */
#else
#define CACHELINE_SIZE 64  /* rest of the world (i.e., x86-64) circa 2024 */
#endif

gsl_rng *gsl_rng_alloc_aligned (const gsl_rng_type * T);
gsl_rng *gsl_rng_clone_aligned (const gsl_rng * r);
