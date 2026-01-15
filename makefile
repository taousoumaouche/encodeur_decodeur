# Makefile minimaliste : construit libdif.so et diftool
CC = gcc
CFLAGS = -Wall -g -fPIC
LIBDIR = CoDec
LIBSRC = $(LIBDIR)/src/codec.c
LIBOBJ = $(LIBDIR)/codec.o
LIB = $(LIBDIR)/libdif.so
TARGET = encodeur
all: $(LIB) $(TARGET)

$(LIB): $(LIBOBJ)
	$(CC) -shared -o $@ $^

$(LIBOBJ): $(LIBSRC)
	$(CC) $(CFLAGS) -I$(LIBDIR)/include -c $< -o $@

$(TARGET): main.c $(LIB)
	$(CC) $(CFLAGS) -I$(LIBDIR)/include main.c -L$(LIBDIR) -ldif -Wl,-rpath,'$$ORIGIN/CoDec' -o $@

TESTSRC = tests/test_pipeline.c
TESTBIN = test_pipeline
clean:
	rm -f $(LIBOBJ) $(LIB) $(TARGET) $(TESTBIN)
$(TESTBIN): $(TESTSRC) $(LIB)
	$(CC) $(CFLAGS) -I$(LIBDIR)/include $(TESTSRC) -L$(LIBDIR) -ldif -Wl,-rpath,'$$ORIGIN/CoDec' -o $@
test: all $(TESTBIN)
	./$(TESTBIN)
.PHONY: all clean test