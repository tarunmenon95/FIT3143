# C compiler
CC = mpicc
# compiler flags
CFLAGS = -g -Wall
LIBS = -lm -pthread

TARGET = prog

default: $(TARGET)

$(TARGET): main.o common.o base.o ground.o
	$(CC) $(CFLAGS) $(LIBS) -o $(TARGET) main.o common.o base.o ground.o

main.o: main.c common.h base.h ground.h
	$(CC) $(CFLAGS) -c main.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

base.o: base.c base.h common.h
	$(CC) $(CFLAGS) -c base.c

ground.o: ground.c ground.h common.h
	$(CC) $(CFLAGS) -c ground.c

clean:
	rm $(TARGET) *.o

