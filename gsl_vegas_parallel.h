#include <gsl/gsl_monte_vegas.h>

#ifdef MPI
int gsl_monte_vegas_integrate_mpi(gsl_monte_function * f, 
                                  double xl[], double xu[], 
                                  size_t dim, size_t calls,
                                  gsl_rng * r,
                                  gsl_monte_vegas_state *state,
                                  double* result, double* abserr);
#endif

#ifdef _OPENMP
int gsl_monte_vegas_integrate_openmp(gsl_monte_function * f, 
                                     double xl[], double xu[], 
                                     size_t dim, size_t calls,
                                     gsl_rng * r,
                                     gsl_monte_vegas_state *state,
                                     double* result, double* abserr);
#endif
