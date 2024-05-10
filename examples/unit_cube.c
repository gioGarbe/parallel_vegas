/*
  https://cran.r-project.org/web/packages/multIntTestFunc/vignettes/MC_Integration_Test_Unit_Hypercube.html
 */
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include <gsl/gsl_pow_int.h>
#include <gsl/gsl_monte_vegas.h>
#include "../gsl_vegas_parallel.h"

#ifdef _OPENMP
#include <omp.h>
#define GSL_VEGAS_INTEGRATE gsl_monte_vegas_integrate_openmp
#else
#ifdef MPI
#include <mpi.h>
#define GSL_VEGAS_INTEGRATE gsl_monte_vegas_integrate_mpi
#else
#define GSL_VEGAS_INTEGRATE gsl_monte_vegas_integrate
#endif
#endif

double exact_integral(size_t dim, double *a)
{
  int i;
  
  double arg = 0.0, sin_ratio = 1.0;
  for(i=0; i<dim; i++)
    {
      arg += a[i];
      sin_ratio *= sin(a[i])/a[i];
    }

  double ret = 0.5 + 0.5 * cos(arg) * sin_ratio;
  return(ret);
}

double f(double *x, size_t dim, void *params)
{
  double *a = (double *)params;
  int i;
  double arg;
  for(arg=0.0, i=0; i<dim; i++)
    arg += a[i] * x[i];

  return(gsl_pow_2( cos(arg) ));  
}

int main(int argc, char *argv[])
{ 
  if(argc != 3)
    {
      printf("usage: %s dimension vegas_calls\n");
      exit(3);
    }
  int rank = 0, p=1;  
#ifdef MPI
  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&p);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  if(!rank) printf("MPI version runnning on %d nodes\n",p);
#endif

#ifdef _OPENMP
  printf("OpenMP version using %d threads\n",omp_get_max_threads());
#endif
  int i;
  int      dim = atoi(argv[1]);
  size_t calls = atoi(argv[2]);
  /* set up the random number generator */
  const gsl_rng_type *T;
  gsl_rng *r;
  gsl_rng_env_setup ();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);

  /* different seed for different MPI instances, if any */
  gsl_rng_set(r, gsl_rng_default_seed + rank);
  
  /* set up the integration limits */
  double *xl = (double *)malloc(dim * sizeof(double));
  double *xu = (double *)malloc(dim * sizeof(double));  
  for(i=0; i<dim; i++)
    {
      xl[i] = 0.0;
      xu[i] = 1.0;
    }

  /* set up the function to integrate - same in all MPI instances if any */
  if(!rank) printf("seed: %lu\n",gsl_rng_default_seed);
  srand(gsl_rng_default_seed);
  double *a = (double *)malloc(dim * sizeof(double));
  for(i=0; i<dim; i++) a[i] = (double)rand()/(double)RAND_MAX;
  
  gsl_monte_function F;
  F.f      = &f;
  F.dim    = dim;
  F.params = (void *)a;

  gsl_monte_vegas_state *s = gsl_monte_vegas_alloc(dim);
  double res, err;

  struct timeval tv;  
  gettimeofday(&tv, NULL);  
  double s_start =
    (double)(tv.tv_sec) +
    (double)(tv.tv_usec) / 1000000.0;
  
  
  GSL_VEGAS_INTEGRATE (&F, xl, xu, dim, calls/5, r, s,
                             &res, &err);

  if(!rank)
    {
      printf("vegas warm-up: %g %g\n", res, err);  
      printf ("converging...\n");
    }

  int cycles = 0;
  do
    {
      GSL_VEGAS_INTEGRATE (&F, xl, xu, dim, calls, r, s,
                                 &res, &err);
      if(!rank)
        printf ("result = % .6f sigma = % .6f "
                "chisq/dof = %.1f\n", res, err, gsl_monte_vegas_chisq (s));
      cycles++;
    }
  while (fabs (gsl_monte_vegas_chisq (s) - 1.0) > 0.5);

  double res_exact = exact_integral(dim, a);

  if(!rank)
    {
      printf("VEGAS: %g ± %g\n",res,err);      
      printf("EXACT: %g\n",res_exact);
    }

  gettimeofday(&tv, NULL);  
  double s_end =
    (double)(tv.tv_sec) +
    (double)(tv.tv_usec) / 1000000.0;
  
  if(!rank)
    {
      printf("Wall-clock time per cycle: %.3f s\n",(s_end-s_start)/(cycles+0.2));
    }
#ifdef MPI  
  MPI_Finalize();
#endif  
}
