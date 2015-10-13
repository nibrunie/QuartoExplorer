SOURCES=./src/quarto_v2.c
OBJECT=$(SOURCES:.c=.o)

STEP_HASH_LIMIT := 6

#set -DVERBOSE in EXTRA_FLAGS to enable more verbosity

# comment the following line to disable avx2 use
SPE_INSN := -mavx2

%.o:%.c 
	$(CC) $(SPE_INSN) -DSTEP_HASH_LIMIT=$(STEP_HASH_LIMIT) $(EXTRA_FLAGS)  -I./include -c -O3 -g $^ -o $@

quarto_ia: $(OBJECT)
	$(CC) -g $^ -o $@ -lm


clean:
	rm $(OBJECT) quarto_ia
