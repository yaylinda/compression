CC=c++
CFLAGS=-I. -c
LDFLAGS=
SOURCES=compressor.cpp burrows_wheeler.cpp
OBJECTS=compressor.o burrows_wheeler.o
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
	./$(EXECUTABLE) c compressor dest
	./$(EXECUTABLE) d dest dest2
