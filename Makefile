CC:=g++
CCOPTS:= -Wall -std=c++11
TARGET:=fs

$(TARGET): FileSystem.o
	g++ -std=c++11 FileSystem.o -o $(TARGET)

compile: FileSystem.o

compress:
	tar -czvf fs-sim.tar.gz FileSystem.* Makefile README.md

%.o: %.cc
	$(CC) $(CCOPTS) $< -c

clean:
	rm -rf *.o $(TARGET)

FileSystem.o: FileSystem.cc FileSystem.h
