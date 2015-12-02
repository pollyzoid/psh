# Project and executable name
TARGET=psh

CC=gcc
# Flags to compile with
CFLAGS=-g -Wall -Wextra -std=gnu99
LFLAGS=-ldl

SRCDIR=src
OBJDIR=obj
BINDIR=bin

SOURCES=$(wildcard $(SRCDIR)/*.c)
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(BINDIR)/$(TARGET)

# Linker
$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(LFLAGS) $(OBJECTS) -o $(BINDIR)/$(TARGET)
	@echo "Linking done!"

# Compiler
$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiling done!"

# Create dirs if needed
$(OBJDIR):
	mkdir -p $(OBJDIR)
$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f $(OBJECTS)
