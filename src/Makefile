CXX            			= g++
CXXFLAGS       	    	= -std=c++17 -Wall -Wextra -I. -O3
OMPFLAGS				= -fopenmp
AUTOFLAGS      			= -fopt-info-vec-optimized -fopt-info-vec-missed

CHECKER_TARGETS			= checker
SEQ_TARGETS 			= hashjoin_seq
OMP_LOOP_TARGETS 		= hashjoin_omp_loop hashjoin_omp_loop_opt hashjoin_omp_loop_opt_wb
OMP_TASK_TARGETS 		= hashjoin_omp_task hashjoin_omp_task_opt hashjoin_omp_task_opt_wb
EXECUTABLE_TARGETS		= $(CHECKER_TARGETS) $(SEQ_TARGETS) $(OMP_LOOP_TARGETS) $(OMP_TASK_TARGETS)

GCC_SEQ_REPORT  			= compilation/gcc_seq_report.txt
GCC_OMP_LOOP_REPORT			= compilation/gcc_omp_loop_report.txt
GCC_OMP_LOOP_OPT_REPORT		= compilation/gcc_omp_loop_opt_report.txt
GCC_OMP_LOOP_OPT_WB_REPORT	= compilation/gcc_omp_loop_opt_wb_report.txt
GCC_OMP_TASK_REPORT			= compilation/gcc_omp_task_report.txt
GCC_OMP_TASK_OPT_REPORT		= compilation/gcc_omp_task_opt_report.txt
GCC_OMP_TASK_OPT_WB_REPORT	= compilation/gcc_omp_task_opt_wb_report.txt
GCC_CHECKER_REPORT			= compilation/gcc_checker_report.txt
GCC_REPORTS					= compilation/gcc_*_report.txt

RES_SEQ					= results/hashjoin_seq.csv
RES_OMP_LOOP			= results/hashjoin_omp_loop.csv
RES_OMP_LOOP_OPT		= results/hashjoin_omp_loop_opt.csv
RES_OMP_LOOP_OPT_WB		= results/hashjoin_omp_loop_opt_wb.csv
RES_OMP_TASK			= results/hashjoin_omp_task.csv
RES_OMP_TASK_OPT		= results/hashjoin_omp_task_opt.csv
RES_OMP_TASK_OPT_WB		= results/hashjoin_omp_task_opt_wb.csv
RESULTS_FILES			= results/*.csv

LIB_SOURCES    			= lib/timing.cpp lib/results.cpp
LOG_FILES				= out/*.log err/*.log


.PHONY: all checker clean cleanlogs cleanallseq cleanallpar cleanall

all: $(TARGETS)


checker: checker.cpp
	$(CXX) $(CXXFLAGS) -o $@ checker.cpp 2> $(GCC_CHECKER_REPORT)


hashjoin_seq: hashjoin_seq.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(AUTOFLAGS) -o $@ hashjoin_seq.cpp $(LIB_SOURCES) 2> $(GCC_SEQ_REPORT)

hashjoin_omp_loop: hashjoin_omp_loop.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_loop.cpp $(LIB_SOURCES) 2> $(GCC_OMP_LOOP_REPORT)

hashjoin_omp_loop_opt: hashjoin_omp_loop_opt.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_loop_opt.cpp $(LIB_SOURCES) 2> $(GCC_OMP_LOOP_OPT_REPORT)

hashjoin_omp_loop_opt_wb: hashjoin_omp_loop_opt_wb.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_loop_opt_wb.cpp $(LIB_SOURCES) 2> $(GCC_OMP_LOOP_OPT_WB_REPORT)

hashjoin_omp_task: hashjoin_omp_task.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_task.cpp $(LIB_SOURCES) 2> $(GCC_OMP_TASK_REPORT)

hashjoin_omp_task_opt: hashjoin_omp_task_opt.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_task_opt.cpp $(LIB_SOURCES) 2> $(GCC_OMP_TASK_OPT_REPORT)

hashjoin_omp_task_opt_wb: hashjoin_omp_task_opt_wb.cpp $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(AUTOFLAGS) -o $@ hashjoin_omp_task_opt_wb.cpp $(LIB_SOURCES) 2> $(GCC_OMP_TASK_OPT_WB_REPORT)


clean:
	-rm -fr *.o *~ $(EXECUTABLE_TARGETS)

cleanlogs: clean
	-rm -fr $(LOG_FILES)

cleanall_seq: clean
	-rm -fr hashjoin_seq $(GCC_SEQ_REPORT) $(LOG_FILES) $(RES_SEQ)

cleanall_omp_loop: clean
	-rm -fr hashjoin_omp_loop $(GCC_OMP_LOOP_REPORT) $(LOG_FILES) $(RES_OMP_LOOP)

cleanall_omp_loop_opt: clean
	-rm -fr hashjoin_omp_loop_opt $(GCC_OMP_LOOP_OPT_REPORT) $(LOG_FILES) $(RES_OMP_LOOP_OPT)

cleanall_omp_loop_opt_wb: clean
	-rm -fr hashjoin_omp_loop_opt_wb $(GCC_OMP_LOOP_OPT_WB_REPORT) $(LOG_FILES) $(RES_OMP_LOOP_OPT_WB)

cleanall_omp_task: clean
	-rm -fr hashjoin_omp_task $(GCC_OMP_TASK_REPORT) $(LOG_FILES) $(RES_OMP_TASK)

cleanall_omp_task_opt: clean
	-rm -fr hashjoin_omp_task_opt $(GCC_OMP_TASK_OPT_REPORT) $(LOG_FILES) $(RES_OMP_TASK_OPT)

cleanall_omp_task_opt_wb: clean
	-rm -fr hashjoin_omp_task_opt_wb $(GCC_OMP_TASK_OPT_WB_REPORT) $(LOG_FILES) $(RES_OMP_TASK_OPT_WB)

cleanall: clean
	-rm -fr $(EXECUTABLE_TARGETS) $(GCC_REPORTS) $(LOG_FILES) $(RESULTS_FILES)
