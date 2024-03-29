
CC = gcc
CXX = g++
CFLAGS = -Wall -O2
CXXFLAGS = -Wall -O2 -std=c++11 -DDEBUG
INCLUDES =
LFLAGS = -s
LIBS = -lm -lz

BUILD_DIR = ../build
SOURCES = $(wildcard *.cpp)
OBJS = $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

pdfcook: ${OBJS}
	${CXX} ${LFLAGS} -o $@ ${OBJS} ${LIBS}

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

# requires full groff package installed
manual:
	groff -m man -T pdf ../pdfcook.1 > ../manual.pdf

install: pdfcook
	install pdfcook /usr/local/bin

uninstall:
	rm /usr/local/bin/pdfcook

installman:
	cp ../pdfcook.1 /usr/share/man/man1

removeman:
	rm /usr/share/man/man1/pdfcook.1
