SOURCES=./src/quarto_v2.c
OBJECT=$(SOURCES:.c=.o)

STEP_HASH_LIMIT := 6


%.o:%.c 
	gcc -DSTEP_HASH_LIMIT=$(STEP_HASH_LIMIT) $(EXTRA_FLAGS) -mavx2 -I./include -c -O3 -g $^ -o $@

quarto_ia: $(OBJECT)
	gcc -g $^ -o $@ -lm


clean:
	rm $(OBJECT) quarto_ia
