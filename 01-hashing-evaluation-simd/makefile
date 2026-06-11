CXX                = g++ -std=c++20
NVCC      		   = /usr/local/cuda/bin/nvcc -std=c++20 -arch=sm_80
OPTFLAGS	   	   = -O3
AUTOFLAGS          = -march=native -ffast-math
CXXFLAGS          += -Wall 
INCLUDES	   	   = -I. -I./include
HEADERS    			= $(wildcard *.hpp)

# Final executable created from make command
TARGETS    = baseline autovec avx2 avx cuda

.PHONY: all clean cleanall 

all: $(TARGETS)

# Plain c++ version, without auto-vectorization
baseline: main_base_vs_auto.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    -fno-tree-vectorize \
	    main_base_vs_auto.cpp -o $@ 

# Same .cpp as before, but with also auto-vectorization flag
autovec: main_base_vs_auto.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    -fopt-info-vec=vec_report.txt \
	    main_base_vs_auto.cpp -o $@ 

# Manual implementation of murmur3 with avx2
avx2: main_avx2.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    main_avx2.cpp -o avx2 

# Manual implementation of murmur3 with avx
avx: main_avx.cpp $(HEADERS)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) $(AUTOFLAGS) \
	    main_avx.cpp -o avx 


# Cuda version of the plain murmur3
cuda: main_cuda.cu $(HEADERS)
	$(NVCC) $(INCLUDES) $(OPTFLAGS) \
	    -Xcompiler "-Wall" \
	    main_cuda.cu -o cuda
		
clean: 
	-rm -fr *.o *~

cleanall: clean
	-rm -fr $(TARGETS) vec_report.txt avx2_output.txt avx_output.txt autovec_output.txt baseline_output.txt cuda_output.txt 
	