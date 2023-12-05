# parallel_vegas
Parallel (OpenMP and MPI) version of the GSL VEGAS Monte Carlo integration routine.

This is a parallel implementation of the VEGAS Monte Carlo integration routine of the [GNU Scientific Library](https://www.gnu.org/software/gsl/) (GSL).
The OpenMP or MPI implementation can be chosen at compilation time, by using `-fopenmp` or `-DMPI` as flags. This has been tested with _gcc_ so your mileage may vary.

The interface is the same as the corresponding serial routine from GSL, and it is used in the same way. Please see the [GSL documentation](https://www.gnu.org/software/gsl/doc/html/montecarlo.html#vegas) for details.

## OpenMP version

The integration routine is called `gsl_monte_vegas_integrate_openmp`.

Please notice that this version **needs a [reentrant](https://en.wikipedia.org/wiki/Reentrancy_(computing)) integrand**.

## MPI version

The integration routine is called `gsl_monte_vegas_integrate_mpi`.

This routine *does not* require a re-entrant integrand.  
However, you need to **assign different seeds** to the random number generators allocated in the various MPI instances.

---

A related multi-threaded integration library implementing several modern algorithms, which does not require re-entrant integrands, is [Cuba](https://feynarts.de/cuba/).
