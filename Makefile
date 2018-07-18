CC = g++
OPT = -O3
#LIBS = -lboost_iostreams -lcvp
LIBS = -lcvp
FLAGS = -std=c++11 -L. $(LIBS) $(OPT)

OBJ = mypredictor.o
DEPS = cvp.h mypredictor.h

all: cvp

cvp: $(OBJ)
	$(CC) $(FLAGS) -o $@ $^

%.o: %.cc $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f *.o cvp
