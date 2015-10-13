#include <stdint.h>

#ifndef __QUARTO_TYPES_H__
/*typedef struct {
  uint64_t value : 64;
} position_compressed_t;*/



typedef union {
  uint64_t nature;
  struct { 
    uint16_t height, shape, hole, color;
    } fields;
} nature_t;

typedef struct {
  uint16_t height_mask, shape_mask, hole_mask, color_mask;
  uint16_t position_occupied_mask;
  uint16_t totem_used_mask;
  uint8_t status;
} position_expanded_t;

typedef union {
  uint64_t description;
  struct {
    uint8_t status : 2;
    uint8_t pos_totem0 : 2;
    uint64_t totem_pos1_15_table : 60;
  } fields;
  struct {
    uint8_t status : 2;
    uint64_t main : 62;
  } reduced;
} position_t;


typedef union {
  __int128 i128;
  struct {
    position_t pos0, pos1;
  } _;
} HASH_CONTAINER;


typedef struct {
  HASH_CONTAINER**               table;
  unsigned*             container_size;
  size_t*         container_free_index;
  size_t                          size;
} opt_hash_t;
#define position_compressed_t position_t

#define HASH_INDEX_SIZE 16
#define HASH_SIZE (1 << HASH_INDEX_SIZE)
#define HASH_INDEX_TYPE uint32_t
#define LIST_SIZE 2048


typedef struct {
  position_t array[LIST_SIZE];
  size_t size;
  size_t next_index;
  void *next;
} position_list_t;

#define HASH_TYPE position_list_t

typedef struct {
  HASH_TYPE **table, **first_free;
  size_t size;
  unsigned long long hash_elts;
} position_hash_t;

// #define position_hash_t opt_hash_t
#endif /* __QUARTO_TYPES_H__ */
