/*
  gsl_vegas_parallel.c - based on GSL/monte/vegas.c
  
  Parallelized version of GSL Vegas. Either MPI or OpenMP.
  Same interface as the serial version.

  MPI version: compile with mpicc -DMPI
  Remember to use a different seed for the random number generator on each
  MPI instance.

  OpenMP version: compile with -fopenmp
  NEEDS A RE-ENTRANT INTEGRAND.
  
  Copyright (C) 2023 Giovanni Garberoglio
  Fondazione Bruno Kessler, ECT*
  garberoglio@ectstar.eu  
*/
/* monte/vegas.c
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 Michael Booth
 * Copyright (C) 2009 Brian Gough
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

/* Author: MJB */
/* Modified by: Brian Gough, 12/2000 */

/* This is an implementation of the adaptive Monte-Carlo algorithm
   of G. P. Lepage, originally described in J. Comp. Phys. 27, 192(1978).
   The current version of the algorithm was described in the Cornell
   preprint CLNS-80/447 of March, 1980.

   This code follows most closely the c version by D.R.Yennie, coded
   in 1984.

   The input coordinates are x[j], with upper and lower limits xu[j]
   and xl[j].  The integration length in the j-th direction is
   delx[j].  Each coordinate x[j] is rescaled to a variable y[j] in
   the range 0 to 1.  The range is divided into bins with boundaries
   xi[i][j], where i=0 corresponds to y=0 and i=bins to y=1.  The grid
   is refined (ie, bins are adjusted) using d[i][j] which is some
   variation on the squared sum.  A third parameter used in defining
   the real coordinate using random numbers is called z.  It ranges
   from 0 to bins.  Its integer part gives the lower index of the bin
   into which a call is to be placed, and the remainder gives the
   location inside the bin.

   When stratified sampling is used the bins are grouped into boxes,
   and the algorithm allocates an equal number of function calls to
   each box.

   The variable alpha controls how "stiff" the rebinning algorithm is.  
   alpha = 0 means never change the grid.  Alpha is typically set between
   1 and 2.

   */

#if defined(MPI) && defined(_OPENMP)
#error "Cannot compile gsl_vegas_parallel with both MPI and _OPENMP defined"  
#endif

/* standard headers */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gsl headers */
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_monte.h>
#include <gsl/gsl_monte_vegas.h>
#include <gsl/gsl_cblas.h>

#ifdef MPI
#include <mpi.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#include "gsl_rng_aligned.h"
#endif

#include "gsl_vegas_parallel.h"

#define MAXDIM  128
#define PADDING 16

/* lib-specific headers */
#define BINS_MAX 50             /* even integer, will be divided by two */

/* A separable grid with coordinates and values */
#define COORD(s,i,j) ((s)->xi[(i)*(s)->dim + (j)])
#define NEW_COORD(s,i) ((s)->xin[(i)])
#define VALUE(s,i,j) ((s)->d[(i)*(s)->dim + (j)])

#define USE_ORIGINAL_CHISQ_FORMULA 0

/* predeclare functions */

typedef int coord;

static void
get_box_coord(gsl_monte_vegas_state *s, size_t n, coord box[]);

static void init_grid (gsl_monte_vegas_state * s, double xl[], double xu[],
                size_t dim);
static void reset_grid_values (gsl_monte_vegas_state * s);
static void accumulate_distribution (gsl_monte_vegas_state * s, coord bin[],
                                     double y);
static void random_point (double x[], coord bin[], double *bin_vol,
                          const coord box[], 
                          const double xl[], const double xu[],
                          gsl_monte_vegas_state * s, gsl_rng * r);
static void resize_grid (gsl_monte_vegas_state * s, unsigned int bins);
static void refine_grid (gsl_monte_vegas_state * s);

static void print_lim (gsl_monte_vegas_state * state,
                       double xl[], double xu[], unsigned long dim);
static void print_head (gsl_monte_vegas_state * state,
                        unsigned long num_dim, unsigned long calls,
                        unsigned int it_num, 
                        unsigned int bins, unsigned int boxes);
static void print_res (gsl_monte_vegas_state * state,
                       unsigned int itr, double res, double err, 
                       double cum_res, double cum_err,
                       double chi_sq);
static void print_dist (gsl_monte_vegas_state * state, unsigned long dim);
static void print_grid (gsl_monte_vegas_state * state, unsigned long dim);

int
#ifdef MPI
gsl_monte_vegas_integrate_mpi
#endif
#ifdef _OPENMP
gsl_monte_vegas_integrate_openmp
#endif
(gsl_monte_function * f,
 double xl[], double xu[],
 size_t dim, size_t calls,
 gsl_rng * r,
 gsl_monte_vegas_state * state,
 double *result, double *abserr)
{
  double cum_int, cum_sig;
  size_t i, it;

  if (dim != state->dim)
    {
      GSL_ERROR ("number of dimensions must match allocated size", GSL_EINVAL);
    }

  for (i = 0; i < dim; i++)
    {
      if (xu[i] <= xl[i])
        {
          GSL_ERROR ("xu must be greater than xl", GSL_EINVAL);
        }

      if (xu[i] - xl[i] > GSL_DBL_MAX)
        {
          GSL_ERROR ("Range of integration is too large, please rescale",
                     GSL_EINVAL);
        }
    }
  
#ifdef MPI
  int par_rank=0, par_size=1;
  MPI_Comm_rank(MPI_COMM_WORLD,&par_rank);
  MPI_Comm_size(MPI_COMM_WORLD,&par_size);
  double tmp;
  /* a buffer to reduce the distributions computed during the main computational cycle */
  double *buf = (double *)malloc(BINS_MAX * dim * sizeof(double));
#endif

#ifdef _OPENMP
  const int par_size = omp_get_max_threads();
  double *buf = (double *)malloc(par_size * (BINS_MAX * dim + PADDING) * sizeof(double));  
#endif
  
  if (state->stage == 0)
    {
      init_grid (state, xl, xu, dim);

      if (state->verbose >= 0)
        {
          print_lim (state, xl, xu, dim);
        }
    }

  if (state->stage <= 1)
    {
      state->wtd_int_sum = 0;
      state->sum_wgts = 0;
      state->chi_sum = 0;
      state->it_num = 1;
      state->samples = 0;
      state->chisq = 0;
    }

  if (state->stage <= 2)
    {
      unsigned int bins = state->bins_max;
      unsigned int boxes = 1;

      if (state->mode != GSL_VEGAS_MODE_IMPORTANCE_ONLY)
        {
          /* shooting for 2 calls/box */

          boxes = floor (pow (calls / 2.0, 1.0 / dim));
          state->mode = GSL_VEGAS_MODE_IMPORTANCE;

          if (2 * boxes >= state->bins_max)
            {
              /* if bins/box < 2 */
              int box_per_bin = GSL_MAX (boxes / state->bins_max, 1);

              bins = GSL_MIN(boxes / box_per_bin, state->bins_max);
              boxes = box_per_bin * bins;

              state->mode = GSL_VEGAS_MODE_STRATIFIED;
            }
        }

      {
        double tot_boxes = gsl_pow_int ((double) boxes, dim);
        state->calls_per_box = GSL_MAX (calls / tot_boxes, 2);
        calls = state->calls_per_box * tot_boxes;
      }

      /* total volume of x-space/(avg num of calls/bin) */
      state->jac = state->vol * pow ((double) bins, (double) dim) / calls;

      state->boxes = boxes;

      /* If the number of bins changes from the previous invocation, bins
         are expanded or contracted accordingly, while preserving bin
         density */

      if (bins != state->bins)
        {
          resize_grid (state, bins);

          if (state->verbose > 1)
            {
              print_grid (state, dim);
            }
        }

      if (state->verbose >= 0)
        {
          print_head (state,
                      dim, calls, state->it_num, state->bins, state->boxes);
        }
    }

  state->it_start = state->it_num;

  cum_int = 0.0;
  cum_sig = 0.0;

  const size_t tot_boxes = (size_t)gsl_pow_int((double)state->boxes,state->dim);
  
  for (it = 0; it < state->iterations; it++)
    {
      double intgrl = 0.0, intgrl_sq = 0.0;
      double tss = 0.0;
      double wgt, var, sig;
      const size_t calls_per_box = state->calls_per_box;
      const double jacbin = state->jac;
      gsl_rng *rng = r;
      
      state->it_num = state->it_start + it;

      reset_grid_values (state);
      
#ifdef MPI
      for(size_t n = par_rank ; n < tot_boxes ; n += par_size)
#endif
#ifdef _OPENMP
      double *old_dist = state->d;
      memset(buf, 0, par_size * (BINS_MAX * dim + PADDING) * sizeof(double));

#pragma omp parallel default(shared) firstprivate(state) private(rng)
      {
        int par_rank = omp_get_thread_num();
        state->d = &buf[par_rank * (BINS_MAX * dim + PADDING)];

        rng = gsl_rng_clone_aligned(r);
#pragma omp critical
        {
          gsl_rng_set(rng, gsl_rng_get(r));
        }
        
        /* #pragma omp parallel for default(shared) firstprivate(state) reduction(+:intgrl,tss) */
#pragma omp for reduction(+:intgrl,tss)
        for(size_t n = 0; n < tot_boxes; n++)        
#endif /* _OPENMP */
          {
            double m = 0.0, q = 0.0;
            double f_sq_sum = 0.0;
            
            size_t k;
            double x[MAXDIM] ;
            coord mybin[MAXDIM]; /* the bin under consideration */
            coord mybox[MAXDIM];

            memset(mybox, 0, MAXDIM * sizeof(coord));
            get_box_coord(state, n, mybox);
            
            for (k = 0; k < calls_per_box; k++)
              {
                double fval;
                double bin_vol;
                
                random_point (x, mybin, &bin_vol, mybox, xl, xu, state, rng);
                
                fval = jacbin * bin_vol * GSL_MONTE_FN_EVAL (f, x);
                
                /* 
                   recurrence for mean and variance (sum of squares) 
                */
                {
                  double d = fval - m;
                  m += d / (k + 1.0);
                  q += d * d * (k / (k + 1.0));
                }
                
                if (state->mode != GSL_VEGAS_MODE_STRATIFIED)
                  {
                    double f_sq = fval * fval;
                    accumulate_distribution (state, mybin, f_sq);
                  }
              }
            
            intgrl   += m * calls_per_box;	  
            f_sq_sum  = q * calls_per_box;
            tss      += f_sq_sum;
            
            if (state->mode == GSL_VEGAS_MODE_STRATIFIED)
              {
                accumulate_distribution (state, mybin, f_sq_sum);
              }
          } /* end of for loop */
#ifdef _OPENMP
        gsl_rng_free(rng);
      } /* end of OpenMP parallel block */
#endif
#ifdef MPI
      tmp = intgrl;
      MPI_Allreduce(&tmp, &intgrl, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);  

      tmp = tss;
      MPI_Allreduce(&tmp,    &tss, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

      MPI_Allreduce(state->d, buf, BINS_MAX * dim, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
      memcpy(state->d, buf, BINS_MAX * dim * sizeof(double));
#endif

#ifdef _OPENMP
      state->d = old_dist;
      for(size_t n=0; n<par_size; n++)
        for(size_t k=0; k< BINS_MAX * dim; k++) state->d[k] += buf[n*(BINS_MAX*dim+PADDING) + k];
      //cblas_daxpy(BINS_MAX * dim, 1.0, &buf[n*(BINS_MAX*dim+PADDING)], 1, state->d, 1);
#endif
      /* Compute final results for this iteration   */

      var = tss / (calls_per_box - 1.0)  ;
      
      if (var > 0) 
        {
          wgt = 1.0 / var;
        }
      else if (state->sum_wgts > 0) 
        {
          wgt = state->sum_wgts / state->samples;
        }
      else 
        {
          wgt = 0.0;
        }

     intgrl_sq = intgrl * intgrl;

     sig = sqrt (var);

     state->result = intgrl;
     state->sigma  = sig;

     if (wgt > 0.0)
       {
         double sum_wgts = state->sum_wgts;
         double wtd_int_sum = state->wtd_int_sum;
         double m = (sum_wgts > 0) ? (wtd_int_sum / sum_wgts) : 0;
         double q = intgrl - m;

         state->samples++ ;
         state->sum_wgts += wgt;
         state->wtd_int_sum += intgrl * wgt;
         state->chi_sum += intgrl_sq * wgt;

         cum_int = state->wtd_int_sum / state->sum_wgts;
         cum_sig = sqrt (1 / state->sum_wgts);

#if USE_ORIGINAL_CHISQ_FORMULA
/* This is the chisq formula from the original Lepage paper.  It
   computes the variance from <x^2> - <x>^2 and can suffer from
   catastrophic cancellations, e.g. returning negative chisq. */
         if (state->samples > 1)
           {
             state->chisq = (state->chi_sum - state->wtd_int_sum * cum_int) /
               (state->samples - 1.0);
           }
#else
/* The new formula below computes exactly the same quantity as above
   but using a stable recurrence */
         if (state->samples == 1) {
           state->chisq = 0;
         } else {
           state->chisq *= (state->samples - 2.0);
           state->chisq += (wgt / (1 + (wgt / sum_wgts))) * q * q;
           state->chisq /= (state->samples - 1.0);
         }
#endif
       }
     else
       {
         cum_int += (intgrl - cum_int) / (it + 1.0);
         cum_sig = 0.0;
       }         


      if (state->verbose >= 0)
        {
          print_res (state,
                     state->it_num, intgrl, sig, cum_int, cum_sig,
                     state->chisq);
          if (it + 1 == state->iterations && state->verbose > 0)
            {
              print_grid (state, dim);
            }
        }

      if (state->verbose > 1)
        {
          print_dist (state, dim);
        }

      refine_grid (state);

      if (state->verbose > 1)
        {
          print_grid (state, dim);
        }

    }

  /* By setting stage to 1 further calls will generate independent
     estimates based on the same grid, although it may be rebinned. */

  state->stage = 1;  

  *result = cum_int;
  *abserr = cum_sig;

#ifdef MPI
  free(buf);
#endif
#ifdef _OPENMP
  free(buf);
#endif
  
  return GSL_SUCCESS;
}


static void
get_box_coord(gsl_monte_vegas_state *s, size_t n, coord box[])
{
  const int ng = s->boxes;
  int index   = s->dim-1;
  
  while(n>0)
    {
      box[index] = n % ng;
      n /= ng;
      index--;
    }
}


/* Deleted functions already available in libgsl */

static void
init_grid (gsl_monte_vegas_state * s, double xl[], double xu[], size_t dim)
{
  size_t j;

  double vol = 1.0;

  s->bins = 1;

  for (j = 0; j < dim; j++)
    {
      double dx = xu[j] - xl[j];
      s->delx[j] = dx;
      vol *= dx;

      COORD (s, 0, j) = 0.0;
      COORD (s, 1, j) = 1.0;
    }

  s->vol = vol;
}


static void
reset_grid_values (gsl_monte_vegas_state * s)
{
  size_t i, j;

  size_t dim = s->dim;
  size_t bins = s->bins;

  for (i = 0; i < bins; i++)
    {
      for (j = 0; j < dim; j++)
        {
          VALUE (s, i, j) = 0.0;
        }
    }
}

static void
accumulate_distribution (gsl_monte_vegas_state * s, coord bin[], double y)
{
  size_t j;
  size_t dim = s->dim;

  for (j = 0; j < dim; j++)
    {
      int i = bin[j];
      VALUE (s, i, j) += y;
    }
}

static void
random_point (double x[], coord bin[], double *bin_vol,
              const coord box[], const double xl[], const double xu[],
              gsl_monte_vegas_state * s, gsl_rng * r)
{
  /* Use the random number generator r to return a random position x
     in a given box.  The value of bin gives the bin location of the
     random position (there may be several bins within a given box) */

  double vol = 1.0;

  size_t j;

  size_t dim = s->dim;
  size_t bins = s->bins;
  size_t boxes = s->boxes;

  for (j = 0; j < dim; ++j)
    {
      /* box[j] + ran gives the position in the box units, while z
         is the position in bin units.  */

      const double csi = gsl_rng_uniform_pos (r);
      double z = (( box[j] + csi ) / boxes) * bins;

      int k = z;

      double y, bin_width;

      bin[j] = k;

      if (k == 0)
        {
          bin_width = COORD (s, 1, j);
          y = z * bin_width;
        }
      else
        {
          bin_width = COORD (s, k + 1, j) - COORD (s, k, j);
          y = COORD (s, k, j) + (z - k) * bin_width;
        }

      x[j] = xl[j] + y * s->delx[j];

      vol *= bin_width;

    }

  *bin_vol = vol;
}


static void
resize_grid (gsl_monte_vegas_state * s, unsigned int bins)
{
  size_t j, k;
  size_t dim = s->dim;

  /* weight is ratio of bin sizes */
  
  double pts_per_bin = (double) s->bins / (double) bins;

  for (j = 0; j < dim; j++)
    {
      double xold;
      double xnew = 0;
      double dw = 0;
      int i = 1;

      for (k = 1; k <= s->bins; k++)
        {
          dw += 1.0;
          xold = xnew;
          xnew = COORD (s, k, j);

          for (; dw > pts_per_bin; i++)
            {
              dw -= pts_per_bin;
              NEW_COORD (s, i) = xnew - (xnew - xold) * dw;
            }
        }

      for (k = 1 ; k < bins; k++)
        {
          COORD(s, k, j) = NEW_COORD(s, k);
        }

      COORD (s, bins, j) = 1;
    }

  s->bins = bins;

    
}

static void
refine_grid (gsl_monte_vegas_state * s)
{
  size_t i, j, k;
  size_t dim = s->dim;
  size_t bins = s->bins;

  for (j = 0; j < dim; j++)
    {
      double grid_tot_j, tot_weight;
      double * weight = s->weight;

      double oldg = VALUE (s, 0, j);
      double newg = VALUE (s, 1, j);

      VALUE (s, 0, j) = (oldg + newg) / 2;
      grid_tot_j = VALUE (s, 0, j);

      /* This implements gs[i][j] = (gs[i-1][j]+gs[i][j]+gs[i+1][j])/3 */

      for (i = 1; i < bins - 1; i++)
        {
          double rc = oldg + newg;
          oldg = newg;
          newg = VALUE (s, i + 1, j);
          VALUE (s, i, j) = (rc + newg) / 3;
          grid_tot_j += VALUE (s, i, j);
        }
      VALUE (s, bins - 1, j) = (newg + oldg) / 2;

      grid_tot_j += VALUE (s, bins - 1, j);

      tot_weight = 0;

      for (i = 0; i < bins; i++)
        {
          weight[i] = 0;

          if (VALUE (s, i, j) > 0)
            {
              oldg = grid_tot_j / VALUE (s, i, j);
              /* damped change */
              weight[i] = pow (((oldg - 1) / oldg / log (oldg)), s->alpha);
            }

          tot_weight += weight[i];

#ifdef DEBUG
          printf("weight[%d] = %g\n", i, weight[i]);
#endif
        }

      {
        double pts_per_bin = tot_weight / bins;
        double xold;
        double xnew = 0;
        double dw = 0;
        i = 1;

        for (k = 0; k < bins; k++)
          {
            dw += weight[k];
            xold = xnew;
            xnew = COORD (s, k + 1, j);

            for (; dw > pts_per_bin; i++)
              {
                dw -= pts_per_bin;
                NEW_COORD (s, i) = xnew - (xnew - xold) * dw / weight[k];
              }
          }

        for (k = 1 ; k < bins ; k++)
          {
            COORD(s, k, j) = NEW_COORD(s, k);
          }

        COORD (s, bins, j) = 1;
      }
    }
  
}


static void
print_lim (gsl_monte_vegas_state * state,
           double xl[], double xu[], unsigned long dim)
{
  unsigned long j;

  fprintf (state->ostream, "The limits of integration are:\n");
  for (j = 0; j < dim; ++j)
    fprintf (state->ostream, "\nxl[%lu]=%f    xu[%lu]=%f", j, xl[j], j, xu[j]);
  fprintf (state->ostream, "\n");
  fflush (state->ostream);
}

static void
print_head (gsl_monte_vegas_state * state,
            unsigned long num_dim, unsigned long calls,
            unsigned int it_num, unsigned int bins, unsigned int boxes)
{
  fprintf (state->ostream,
           "\nnum_dim=%lu, calls=%lu, it_num=%d, max_it_num=%d ",
           num_dim, calls, it_num, state->iterations);
  fprintf (state->ostream,
           "verb=%d, alph=%.2f,\nmode=%d, bins=%d, boxes=%d\n",
           state->verbose, state->alpha, state->mode, bins, boxes);
  fprintf (state->ostream,
           "\n       single.......iteration                   ");
  fprintf (state->ostream, "accumulated......results   \n");

  fprintf (state->ostream,
           "iteration     integral    sigma             integral   ");
  fprintf (state->ostream, "      sigma     chi-sq/it\n\n");
  fflush (state->ostream);

}

static void
print_res (gsl_monte_vegas_state * state,
           unsigned int itr, 
           double res, double err, 
           double cum_res, double cum_err,
           double chi_sq)
{
  fprintf (state->ostream,
           "%4d        %6.4e %10.2e          %6.4e      %8.2e  %10.2e\n",
           itr, res, err, cum_res, cum_err, chi_sq);
  fflush (state->ostream);
}

static void
print_dist (gsl_monte_vegas_state * state, unsigned long dim)
{
  unsigned long i, j;
  int p = state->verbose;
  if (p < 1)
    return;

  for (j = 0; j < dim; ++j)
    {
      fprintf (state->ostream, "\n axis %lu \n", j);
      fprintf (state->ostream, "      x   g\n");
      for (i = 0; i < state->bins; i++)
        {
          fprintf (state->ostream, "weight [%11.2e , %11.2e] = ", 
                   COORD (state, i, j), COORD(state,i+1,j));
          fprintf (state->ostream, " %11.2e\n", VALUE (state, i, j));

        }
      fprintf (state->ostream, "\n");
    }
  fprintf (state->ostream, "\n");
  fflush (state->ostream);

}

static void
print_grid (gsl_monte_vegas_state * state, unsigned long dim)
{
  unsigned long i, j;
  int p = state->verbose;
  if (p < 1)
    return;

  for (j = 0; j < dim; ++j)
    {
      fprintf (state->ostream, "\n axis %lu \n", j);
      fprintf (state->ostream, "      x   \n");
      for (i = 0; i <= state->bins; i++)
        {
          fprintf (state->ostream, "%11.2e", COORD (state, i, j));
          if (i % 5 == 4)
            fprintf (state->ostream, "\n");
        }
      fprintf (state->ostream, "\n");
    }
  fprintf (state->ostream, "\n");
  fflush (state->ostream);

}

