# C++用コンパイラ
CXX		= g++
# コンパイルオプション
CFLAGS	= -pipe -std=c++20 -Wall -Wextra -O3 -march=native -DNDEBUG
# デバッグ用コンパイルオプション
DBGFLGS	= -pipe -std=c++20 -Wall -Wextra -fsanitize=undefined

.PONY: all

all: tasm tec

debug: tasm-debug tec-debug

tasm: tasm.cpp
	$(CXX) $(CFLAGS) tasm.cpp -o ../bin/tasm

tec: tec.cpp
	$(CXX) $(CFLAGS) tec.cpp -o ../bin/tec

tasm-debug: tasm.cpp
	$(CXX) $(DBGFLGS) tasm.cpp -o ../bin/tasm-debug

tec-debug: tec.cpp
	$(CXX) $(DBGFLGS) tec.cpp -o ../bin/tec-debug