CXX            			= g++
CXXFLAGS       	    	= -std=c++17 -Wall -Wextra -I. -O3
OMPFLAGS				= -fopenmp
AUTOFLAGS      			= -fopt-info-vec-optimized -fopt-info-vec-missed

SEQ_TARGETS 			= hashjoin_seq
PAR_OMP_LOOP_TARGETS 	= hashjoin_omp_loop
TARGETS					= hashjoin_* weak_scaling

GCC_SEQ_REPORT  		= compilation/gcc_seq_report.txt
GCC_OMP_LOOP_REPORT		= compilation/gcc_omp_loop_report.txt
GCC_OMP_TASK_REPORT		= compilation/gcc_omp_task_report.txt
GCC_REPORTS				= compilation/gcc_*_report.txt

RES_SEQ					= results/hashjoin_seq.csv
RES_OMP_LOOP			= results/hashjoin_omp_loop.csv
RES_OMP_TASK			= results/hashjoin_omp_task.csv
RESULTS_FILES			= results/*.csv

LIB_SOURCES    			= lib/timing.cpp lib/results.cpp
LOG_FILES				= out/*.log err/*.log


.PHONY: all clean cleanlogs cleanallseq cleanallpar cleanall

all: $(TARGETS)

checker: checker.cpp
	$(CXX) $(CXXFLAGS) -o $@ checker.cpp

hashjoin_seq: hashjoin_seq.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(AUTOFLAGS) -o $@ hashjoin_seq.cpp $(LIB_SOURCES) 2> $(GCC_SEQ_REPORT)

hashjoin_omp_loop: hashjoin_omp_loop.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_loop.cpp $(LIB_SOURCES) 2> $(GCC_OMP_LOOP_REPORT)

hashjoin_omp_task: hashjoin_omp_task.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_task.cpp $(LIB_SOURCES) 2> $(GCC_OMP_TASK_REPORT)

clean:
	-rm -fr *.o *~

cleanlogs: clean
	-rm -fr $(LOG_FILES)

cleanall_seq: clean
	-rm -fr $(SEQ_TARGETS) $(GCC_SEQ_REPORT) $(LOG_FILES) $(RES_SEQ)

cleanall_omp_loop: clean
	-rm -fr $(OMP_LOOP_TARGETS) $(GCC_OMP_LOOP_REPORT) $(LOG_FILES) $(RES_OMP_LOOP)

cleanall_omp_task: clean
	-rm -fr $(OMP_TASK_TARGETS) $(GCC_OMP_TASK_REPORT) $(LOG_FILES) $(RES_OMP_TASK)

cleanall: clean
	-rm -fr $(TARGETS) $(GCC_REPORTS) $(LOG_FILES) $(RESULTS_FILES)
