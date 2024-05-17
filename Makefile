CC =    gcc
MPICC  = mpicc
CFLAGS = -ggdb -O2 -Wall

default: gsl_vegas_openmp.o gsl_vegas_mpi.o

gsl_rng_aligned.o: gsl_rng_aligned.c gsl_rng_aligned.h
	$(CC) $(CFLAGS) -c -o $@ $<

gsl_vegas_openmp.o: gsl_vegas_parallel.c gsl_rng_aligned.o 
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<
	ld -r -o $@ $@ gsl_rng_aligned.o

gsl_vegas_mpi.o: gsl_vegas_parallel.c 
	$(MPICC) $(CFLAGS) -DMPI -c -o $@ $<

clean:
	rm -f *.o
