CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SRC      := main.cpp Question.cpp Player.cpp Quiz.cpp Leaderboard.cpp

codequest: $(SRC) $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -municode -mwindows -o $@ $(SRC)

run: codequest
	./codequest.exe

clean:
	rm -f codequest.exe

.PHONY: run clean
