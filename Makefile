CC=c++
CFLAGS=-I. -c	
LDFLAGS=
SOURCES=compressor.cpp dictionary.cpp burrows_wheeler.cpp InputStream.cpp OutputStream.cpp FileInputStream.cpp FileOutputStream.cpp
#OBJECTS=compressor.o dictionary.o burrows_wheeler.o
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=compressor

TEST_FILE=test.txt

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm $(EXECUTABLE)
	-rm $(OBJECTS)
	-rm dest
	-rm dest2

run:
	./$(EXECUTABLE) c $(TEST_FILE) dest
	./$(EXECUTABLE) d dest dest2
	diff $(TEST_FILE) dest2

