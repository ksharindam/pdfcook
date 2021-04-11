
CC = gcc
CXX = g++
CFLAGS = -Wall -O2
#CXXFLAGS = -Wall -O2 -g -DDEBUG
CXXFLAGS = -Wall -O2 -DDEBUG
INCLUDES =
LIBS = -lm -lz

BUILD_DIR = ../build
SOURCES = $(wildcard *.cpp)
OBJS = $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

pdfcook: ${OBJS}
	${CXX} -s -o $@ ${OBJS} ${LIBS}

clean:
	rm -f $(BUILD_DIR)/*.o pdfcook

# c
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${INCLUDES} -c $< -o $@

# c++
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	${CXX} ${CXXFLAGS} ${INCLUDES} -c $< -o $@


install:
	install pdfcook /usr/local/bin

uninstall:
	rm /usr/local/bin/pdfcook

installman:
	cp ../pdfcook.1 /usr/share/man/man1

removeman:
	rm /usr/share/man/man1/pdfcook.1
