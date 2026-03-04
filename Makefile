CC ?= gcc

ifeq ($(OS),Windows_NT)
EXE := .exe
RUN_PREFIX :=
else
EXE :=
RUN_PREFIX := ./
endif

TARGET ?= memory_sim$(EXE)
SRC := memory_sim.c
OBJ := $(SRC:.c=.o)

CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic
OPT ?= -O2
CFLAGS ?= $(CSTD) $(WARN) $(OPT)
LDFLAGS ?=

.PHONY: all run debug sanitize clean help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(RUN_PREFIX)$(TARGET)

debug: CFLAGS := $(CSTD) $(WARN) -O0 -g3 -DDEBUG
debug: clean $(TARGET)

sanitize: CFLAGS := $(CSTD) $(WARN) -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
sanitize: LDFLAGS := -fsanitize=address,undefined
sanitize: clean $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJ)

help:
	@echo "Available targets:"
	@echo "  make            Build release binary"
	@echo "  make run        Build and run"
	@echo "  make debug      Build with debug symbols"
	@echo "  make sanitize   Build with ASan/UBSan"
	@echo "  make clean      Remove build artifacts"
