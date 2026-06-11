CXX_SEQ  = g++
CXX_MPI  = mpicxx

OPTFLAGS  = -O3
AUTOFLAGS = -march=native -ffast-math
CXXFLAGS  = -std=c++20 -Wall -fopenmp

INCLUDES  = -I. -I./include
LIBS      = -pthread

HEADERS   = $(wildcard *.hpp) $(wildcard include/*.hpp)


TARGETS   = seq mpi hybrid

.PHONY: all clean cleanall 

all: $(TARGETS)

# Sequential Partitioned Hash Join (compiled with standard gcc)
seq: ./cpp/hashjoin_seq.cpp $(HEADERS)
	$(CXX_SEQ) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
		./cpp/hashjoin_seq.cpp -o seq $(LIBS)

# Distributed Partitioned Hash Join (compiled with mpicxx)
mpi: ./cpp/hashjoin_mpi.cpp $(HEADERS)
	$(CXX_MPI) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
		./cpp/hashjoin_mpi.cpp -o mpi $(LIBS)


# Hybrid MPI + openMP Partitioned Hash Join
hybrid: ./cpp/hashjoin_hybrid.cpp $(HEADERS)
	$(CXX_MPI) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
		./cpp/hashjoin_hybrid.cpp -o hybrid $(LIBS)



clean: 
	-rm -fr *.o *~

cleanall: clean
	-rm -fr $(TARGETS) profiling_*.txt out_*.txt tmp/ *.csv 

