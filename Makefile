PNAME = pdfcook
CXX = g++
SRCS = src
INCS = include
CXXFLAGS = -Wall -O2 -DDEBUG -std=c++11 -I${SRCS}/${INCS} -D_GNU_SOURCE
LDFLAGS = -lm -lz -s
PREFIX = /usr/local
AR = ar
INSTALL = install
RM = rm -f
VER           = 0
VERB          = 1
ifeq ($(OS),Windows_NT)
PLIBA         = $(PNAME).a
PLIBSO        = $(PNAME).$(VER).dll
else
PLIBA         = lib$(PNAME).a
PLIBSO        = lib$(PNAME).so.$(VER)
endif
PLIB = ${PLIBA} ${PLIBSO}

SOURCESLIB = $(wildcard ${SRCS}/lib/*.cpp)
SOURCES = $(wildcard ${SRCS}/*.cpp)
INCLUDESLIB = $(wildcard ${SRCS}/${INCS}/*.h)
OBJSLIB = $(SOURCESLIB:%.cpp=%.o)
OBJS = $(SOURCES:%.cpp=%.o)

all: ${PNAME} ${PNAME}-static

${PLIBSO}: ${OBJSLIB}
	$(AR) crs ${PLIBA} $^
	$(CXX) ${CXXFLAGS} -shared -Wl,-soname,$@ $^ -o $@  ${LDFLAGS}

${PNAME}: ${OBJS} ${PLIBSO}
	$(CXX) ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

${PNAME}-static: ${OBJS} ${PLIBA}
	$(CXX) ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

clean:
	$(RM) ${PNAME} ${PNAME}-static ${PLIB} ${OBJS} ${OBJSLIB}

install:
	$(INSTALL) -d ${PREFIX}/bin
	$(INSTALL) -m 0755 ${PNAME} ${PREFIX}/bin
	$(INSTALL) -d ${PREFIX}/lib
	$(INSTALL) -m 0644 ${PLIB} ${PREFIX}/lib
	$(INSTALL) -d ${PREFIX}/include/${PNAME}
	$(INSTALL) -m 0644 ${INCLUDESLIB} ${PREFIX}/include/${PNAME}
	$(INSTALL) -d ${PREFIX}/share/man/man1
	$(INSTALL) -m 0644 man/${PNAME}.1 ${PREFIX}/share/man/man1

uninstall:
	$(RM) ${PREFIX}/bin/${PNAME}
	$(RM) ${PREFIX}/lib/${PLIBA} ${PREFIX}/lib/${PLIBSO}
	$(RM) -r ${PREFIX}/include/${PNAME}
	$(RM) ${PREFIX}/share/man/man1/${PNAME}.1 ${PREFIX}/share/man/man1/${PNAME}.1.gz
