CC=c++
CFLAGS=-I. -c
LDFLAGS=
SOURCES=compressor.cpp dictionary.cpp burrows_wheeler.cpp
OBJECTS=compressor.o dictionary.o burrows_wheeler.o
EXECUTABLE=compressor

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) 

%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm $(EXECUTABLE)
	-rm $(OBJECTS)

run:
	./$(EXECUTABLE) c test.txt dest
	./$(EXECUTABLE) d dest dest2
