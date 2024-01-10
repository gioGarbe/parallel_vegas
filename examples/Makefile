CC = gcc
MPICC = mpicc

CFLAGS = -O2

PROGS = unit_cube unit_cube_omp unit_cube_mpi

default: $(PROGS)

unit_cube: unit_cube.c
	$(CC) $(CFLAGS) -o $@ $< -lgsl -lgslcblas -lm

gsl_vegas_openmp.o: ../gsl_vegas_parallel.c
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<

unit_cube_omp: unit_cube.c gsl_vegas_openmp.o
	$(CC) $(CFLAGS) -fopenmp -o $@ $< gsl_vegas_openmp.o -lgsl -lgslcblas -lm

gsl_vegas_mpi.o: ../gsl_vegas_parallel.c
	$(MPICC) $(CFLAGS) -DMPI -c -o $@ $<

unit_cube_mpi: unit_cube.c gsl_vegas_mpi.o
	$(MPICC) $(CFLAGS) -DMPI -o $@ $< gsl_vegas_mpi.o -lgsl -lgslcblas -lm

clean:
	rm -f *.o
	rm -f $(PROGS)
