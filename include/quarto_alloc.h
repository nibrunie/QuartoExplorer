#ifndef __QUARTO_ALLOC_H__
#include <signal.h>

#define ALLOCATION_BOUND 2000000000ll
unsigned long long total_allocated = 0;

void print_failure_summary(void);

void* my_calloc(unsigned elt_num, size_t elt_size) {
  total_allocated += elt_num * elt_size;
  if (total_allocated > ALLOCATION_BOUND) {
    printf("total allocated exceeds limites %lld > %lld \n", total_allocated, ALLOCATION_BOUND);
    print_failure_summary();
    //unsigned long long count = count_hash_elts(&main_hash);
    //printf("main hash contains %llu records\n", count);
    raise(SIGINT);
    return NULL;
  }

  return calloc(elt_num, elt_size);
}

void* my_malloc(size_t elt_size) {
  total_allocated += elt_size;
  if (total_allocated > ALLOCATION_BOUND) {
    printf("total allocated exceeds limites %lld > %lld \n", total_allocated, ALLOCATION_BOUND);
    print_failure_summary();
    //unsigned long long count = count_hash_elts(&main_hash);
    //printf("main hash contains %llu records\n", count);
    raise(SIGINT);
    return NULL;
  }

  return malloc(elt_size);
}
#endif /* __QUARTO_ALLOC_H__ */
