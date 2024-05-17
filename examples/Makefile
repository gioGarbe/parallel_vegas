CC = gcc
MPICC = mpicc

CFLAGS = -ggdb -O2 -Wall
LIBS   = -lgsl -lopenblas -lm
PROGS = unit_cube unit_cube_omp unit_cube_mpi

default: always $(PROGS)

.phony: always 

always:
	(cd ../ ; make)

unit_cube: unit_cube.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) 

unit_cube_omp: unit_cube.c ../gsl_vegas_openmp.o
	$(CC) $(CFLAGS) -fopenmp -o $@ $^ $(LIBS)

unit_cube_mpi: unit_cube.c ../gsl_vegas_mpi.o
	$(MPICC) $(CFLAGS) -DMPI -o $@ $^ $(LIBS)

clean:
	rm -f *.o *dSYM
	rm -f $(PROGS)
