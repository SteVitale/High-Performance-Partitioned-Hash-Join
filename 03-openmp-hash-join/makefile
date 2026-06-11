CXX                = g++ -std=c++20
OPTFLAGS	   	   = -O3
AUTOFLAGS          = -march=native -ffast-math 
CXXFLAGS          += -Wall 
INCLUDES	   	   = -I. -I./include
LIBS               = -pthread -fopenmp
HEADERS    		   = $(wildcard *.hpp)

# Final executable created from make command
TARGETS    = seq par omp_loop omp_task skew omp_nested

.PHONY: all clean cleanall 

all: $(TARGETS)

# Sequential Partitioned Hash Join
seq: ./cpp/hashjoin_seq.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./cpp/hashjoin_seq.cpp -o seq 

# Parallel Partitioned Hash Join
par: ./cpp/hashjoin_par.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./cpp/hashjoin_par.cpp -o par 

# OpenMP Partitioned Hash Join (Loop Version)
omp_loop: ./cpp/hashjoin_omp_loop.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./cpp/hashjoin_omp_loop.cpp -o omp_loop $(LIBS)

# OpenMP Partitioned Hash Join (Task Version)
omp_task: ./cpp/hashjoin_omp_task.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./cpp/hashjoin_omp_task.cpp -o omp_task $(LIBS)	

# Skew main 
skew: ./utils/distribution_analysis.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./utils/distribution_analysis.cpp -o skew

# OpenMP Partitioned Hash Join (NestedTask Version)
omp_nested: ./cpp/hashjoin_omp_nested.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    ./cpp/hashjoin_omp_nested.cpp -o omp_nested $(LIBS)


clean: 
	-rm -fr *.o *~

cleanall: clean
	-rm -fr $(TARGETS) profiling_*.txt out_*.txt tmp/ *.csv
	