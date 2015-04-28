all: libassert.ll

libassert.ll: assert.ll
	llvm-link -S $^ -o $@

%.ll: %.c
	clang -S -emit-llvm -m32 -O3 $^ -o $@

.PHONY clean:
	rm -f *.ll
