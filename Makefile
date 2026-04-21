CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lcrypto

ifeq ($(OS),Windows_NT)
CLEAN_CMD = powershell -NoProfile -Command '$$ErrorActionPreference="SilentlyContinue"; Remove-Item -Force pes,test_objects,test_tree,object.o,tree.o,index.o,commit.o,pes.o,test_objects.o,test_tree.o; Remove-Item -Recurse -Force .pes; exit 0'
else
CLEAN_CMD = rm -f pes test_objects test_tree $(OBJS) test_objects.o test_tree.o; rm -rf .pes
endif

# ─── Main binary ─────────────────────────────────────────────────────────────

SRCS = object.c tree.c index.c commit.c pes.c
OBJS = $(SRCS:.c=.o)

pes: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c pes.h
	$(CC) $(CFLAGS) -c $< -o $@

# ─── Test binaries ───────────────────────────────────────────────────────────

test_objects: test_objects.o object.o
	$(CC) -o $@ $^ $(LDFLAGS)

test_tree: test_tree.o object.o tree.o index.o
	$(CC) -o $@ $^ $(LDFLAGS)

# ─── Convenience targets ────────────────────────────────────────────────────

.PHONY: all clean test test-unit test-integration

all: pes test_objects test_tree

clean:
	$(CLEAN_CMD)

test: test-unit test-integration

test-unit: test_objects test_tree
	@echo "=== Running Phase 1 tests ==="
	./test_objects
	@echo ""
	@echo "=== Running Phase 2 tests ==="
	./test_tree

test-integration: pes
	@echo "=== Running integration tests ==="
	bash test_sequence.sh