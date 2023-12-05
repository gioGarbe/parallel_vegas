CC =    gcc
MPICC  = mpicc
CFLAGS = -O2

default: gsl_vegas_openmp.o gsl_vegas_mpi.o

gsl_vegas_openmp.o: gsl_vegas_parallel.c
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<

gsl_vegas_mpi.o: gsl_vegas_parallel.c
	$(MPICC) $(CFLAGS) -DMPI -c -o $@ $<

clean:
	rm -f *.o
