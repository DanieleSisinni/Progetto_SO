CC=gcc
COMPILATION_FLAGS= -std=c89 -pedantic -Wall -Werror
DEPENDENCIES = Shared.h

OBJTAXI = Taxi.o Shared.o 
OBJSOURCE = Source.o Shared.o 
OBJMASTER = Master.o Shared.o 

%.o: %.c $(DEPENDENCIES)
	$(CC) -c -o $@ $< $(CFLAGS)

all: Taxi Source Master

Taxi: $(OBJTAXI)
	$(CC) $(COMPILATION_FLAGS) -o Taxi $(OBJTAXI) 

Source: $(OBJSOURCE)
	$(CC) $(COMPILATION_FLAGS) -o Source $(OBJSOURCE) 

Master: $(OBJMASTER)
	$(CC) $(COMPILATION_FLAGS) -o Master $(OBJMASTER) 

run:
	./Master

clean:
	rm –f *.o *.so Taxi Source Master *~
