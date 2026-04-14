CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRCS = main.cpp workloads.cpp algo_zero.cpp algo_delta.cpp algo_bdi.cpp algo_fpc.cpp algo_lz4.cpp algo_bpc.cpp algo_bpc_spec.cpp algo_snappy.cpp algo_cascade.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = bench
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
clean:
	rm -f $(OBJS) $(TARGET) workload_*.bin
.PHONY: all clean
