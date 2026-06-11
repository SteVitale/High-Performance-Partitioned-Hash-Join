CXX                = g++ -std=c++20
OPTFLAGS	   	   = -O3
AUTOFLAGS          = -march=native -ffast-math
CXXFLAGS          += -Wall 
INCLUDES	   	   = -I. -I./include
HEADERS    		   = $(wildcard *.hpp)

# Final executable created from make command
TARGETS    = seq par seq_original

.PHONY: all clean cleanall 

all: $(TARGETS)

# Sequential Partitioned Hash Join
seq: hashjoin_seq.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    hashjoin_seq.cpp -o seq 

seq_original: hashjoin_seq_original.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    hashjoin_seq_original.cpp -o seq_original

# Parallel Partitioned Hash Join
par: hashjoin_par.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    hashjoin_par.cpp -o par 

clean: 
	-rm -fr *.o *~

cleanall: clean
	-rm -fr $(TARGETS) profiling_*.txt out_*.txt tmp/
	