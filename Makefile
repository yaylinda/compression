CC=c++
CFLAGS=-I. -c	
LDFLAGS=
SOURCES=compressor.cpp dictionary.cpp burrows_wheeler.cpp InputStream.cpp OutputStream.cpp FileInputStream.cpp FileOutputStream.cpp
#OBJECTS=compressor.o dictionary.o burrows_wheeler.o
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=compressor

TEST_FILE=test.txt
PYTEST_FILE=pytest.txt

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
	-rm pydest
	-rm pydest2

run:
	./$(EXECUTABLE) c $(TEST_FILE) dest
	echo
	./$(EXECUTABLE) d dest dest2
	echo
	diff $(TEST_FILE) dest2

gold:
	python arithmetic_encoding.py c $(PYTEST_FILE) pydest
	echo
	python arithmetic_encoding.py d pydest pydest2
	echo
	diff $(PYTEST_FILE) pydest2

