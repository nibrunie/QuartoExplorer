#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <immintrin.h>
#include <assert.h>
#include <math.h>

// local projects include
#include <quarto_alloc.h>
#include <quarto_timer.h>
#include <factoriel.h>

#ifdef VERBOSE
#define PRINTF printf
#else
#define PRINTF
#endif

typedef struct {
  // position || position || position || position
  uint64_t pos_used_mask_x4;

  //  shape || color || hole || height 
  // 1 bit per pawn
  uint64_t pawn_state_mask;
} pos_exp_t;

#define WINMASK_0 0x000f000f000f000full  // first line
#define WINMASK_1 0x00f000f000f000f0ull  // second line
#define WINMASK_2 0x0f000f000f000f00ull  // third line
#define WINMASK_3 0xf000f000f000f000ull  // fourth line
#define WINMASK_4 0x1111111111111111ull  // first column
#define WINMASK_5 0x2222222222222222ull  // first column
#define WINMASK_6 0x4444444444444444ull  // first column
#define WINMASK_7 0x8888888888888888ull  // first column
#define WINMASK_8 0x8421842184218421ull  // first diagonal
#define WINMASK_9 0x1248124812481248ull  // second diagonal

#define ONE_MASK_16x4 ((uint64_t) 0x0001000100010001ull)

#define MASK_X4(x) x, x, x, x
typedef enum {
  S_EXP_WIN0 = 1,
  S_EXP_WIN1 = 2,
  S_EXP_TIE  = 4,
  S_EXP_UNK  = 0
} expanded_status_t;

typedef enum {
  S_RED_WIN0 = 1,
  S_RED_WIN1 = 2,
  S_RED_TIE  = 3,
  S_RED_UNK  = 0
} reduced_status_t;


const uint64_t __attribute__((aligned(32))) WINMASK_U64[10*4] = {
    MASK_X4(WINMASK_0),
    MASK_X4(WINMASK_1),
    MASK_X4(WINMASK_2),
    MASK_X4(WINMASK_3),
    MASK_X4(WINMASK_4),
    MASK_X4(WINMASK_5),
    MASK_X4(WINMASK_6),
    MASK_X4(WINMASK_7),
    MASK_X4(WINMASK_8),
    MASK_X4(WINMASK_9)
};


void print_m256(__m256i v) {
  uint64_t __attribute__((aligned(32))) memo[4];
  _mm256_store_si256((__m256i*) memo, v);
  printf("%"PRIx64" %"PRIx64" %"PRIx64" %"PRIx64"\n", memo[3], memo[2], memo[1], memo[0]);
}

static inline int container_has_quarto(__m256i pos_used_mask4x4, __m256i pawn_state_mask_x4) {
  __m256i state_mask_x4 = _mm256_and_si256(pawn_state_mask_x4, pos_used_mask4x4); 
  // inverse mask
  __m256i state_N_mask_x4 = _mm256_andnot_si256(pawn_state_mask_x4, pos_used_mask4x4); 

  unsigned i;
  for (i = 0; i < 10; ++i) { 
    __m256i winmask_i = _mm256_load_si256((__m256i*) (WINMASK_U64 + 4*i));
    __m256i win_state = _mm256_and_si256(winmask_i, state_mask_x4);
    win_state = _mm256_cmpeq_epi16(win_state, winmask_i);
    __m256i win_state_N = _mm256_and_si256(winmask_i, state_N_mask_x4);
    win_state_N = _mm256_cmpeq_epi16(win_state_N, winmask_i);

    // global result
    // if any 16-bit chunk in win_state or win_state_N is non-zero 
    // then a win mask have been found and global result will
    // be non zero
    win_state = _mm256_or_si256(win_state, win_state_N);
    if (!_mm256_testz_si256(win_state, win_state)) return 1;
  }

  return 0;
}

static inline int position_is_quarto(uint64_t pos_used_mask_x4, uint64_t pawn_state_mask) {
  uint64_t state_mask   = pawn_state_mask & pos_used_mask_x4;
  uint64_t state_N_mask = pos_used_mask_x4 & (~pawn_state_mask);

  unsigned i;
  for (i = 0; i < 10; ++i) {
    uint64_t winmask = WINMASK_U64[4*i];
    __m64 wineq_mask    = _mm_cmpeq_pi16(_mm_cvtsi64_m64(state_mask & winmask),    _mm_cvtsi64_m64(winmask));
    __m64 wineq_invmask = _mm_cmpeq_pi16(_mm_cvtsi64_m64(state_N_mask & winmask), _mm_cvtsi64_m64(winmask));
    if (_mm_cvtm64_si64(wineq_mask) | _mm_cvtm64_si64(wineq_invmask)) return 1;
  }

  return 0;
}


#define PAWN(n) ((((uint64_t) n & 0x1) << 0) | \
                (((uint64_t) n & 0x2) << 15)| \
                (((uint64_t) n & 0x4) << 30)| \
                (((uint64_t) n & 0x8) << 45))


const uint64_t PAWN_ARRAY[16] = {
  PAWN(0x0),
  PAWN(0x1),
  PAWN(0x2),
  PAWN(0x3),
  PAWN(0x4),
  PAWN(0x5),
  PAWN(0x6),
  PAWN(0x7),
  PAWN(0x8),
  PAWN(0x9),
  PAWN(0xa),
  PAWN(0xb),
  PAWN(0xc),
  PAWN(0xd),
  PAWN(0xe),
  PAWN(0xf)
};

#define HASH_TYPE uint32_t
#define HASH_SIZE 18

HASH_TYPE compute_hash(uint64_t pos_used_maskx4, uint64_t pawn_state_mask) {
  HASH_TYPE hash;
  hash  = ((uint64_t) (pos_used_maskx4 & 0xffffull));
  hash ^= ((uint64_t) (pawn_state_mask >> 46) & 0x3fffcull);
  hash ^= (uint64_t) ((pawn_state_mask >> 30) & 0x3fffcull);
  hash ^= (uint64_t) ((pawn_state_mask >> 16) & 0x0ffffull);
  hash ^= (uint64_t) ((pawn_state_mask >>  0) & 0x0ffffull);
  return hash;
};

#define HASH_TABLE_SIZE (1 << HASH_SIZE)
#define HASH_CELL_INIT_SIZE 32
#define HASH_SIZE_INCREMENT 256
typedef struct {
  uint64_t* table[HASH_TABLE_SIZE];
  unsigned cell_size[HASH_TABLE_SIZE];
  unsigned cell_first_free[HASH_TABLE_SIZE];
} hash_t;

hash_t main_hash;

void init_hash(void) {
  unsigned i;
  for (i = 0; i < HASH_TABLE_SIZE; ++i) {
    main_hash.table[i] = (uint64_t*) my_calloc(HASH_CELL_INIT_SIZE, sizeof(uint64_t));
    main_hash.cell_size[i] = HASH_CELL_INIT_SIZE;
    main_hash.cell_first_free[i] = 0;
  };
}


void  store_position_in_hash(uint64_t pos_used_maskx4, uint64_t pawn_state_mask, reduced_status_t status, int current_player) {
  // compute hash
  HASH_TYPE hash = compute_hash(pos_used_maskx4, pawn_state_mask); 
  assert(status == S_RED_TIE || status == S_RED_WIN0 || status == S_RED_WIN1);

  // check that hash has enough room for the new record
  if (main_hash.cell_first_free[hash] >= main_hash.cell_size[hash]) {
    uint64_t* old_cell = main_hash.table[hash];
    unsigned old_size = main_hash.cell_size[hash];
    unsigned old_first_free = main_hash.cell_first_free[hash];

    main_hash.table[hash] = (uint64_t*) my_calloc(old_size + HASH_SIZE_INCREMENT, sizeof(uint64_t));
    main_hash.cell_size[hash] = old_size + HASH_SIZE_INCREMENT;
    main_hash.cell_first_free[hash] = old_first_free;
    // copying old datas
    memcpy(main_hash.table[hash], old_cell, old_size * sizeof(uint64_t));
    free(old_cell);
  };

  assert(((status & 0x3) == status ) && status);

  uint64_t stored_mask = (pawn_state_mask << 2) | (uint64_t) status;
  main_hash.table[hash][main_hash.cell_first_free[hash]] = stored_mask;
  main_hash.cell_first_free[hash]++;

  PRINTF("storing %llx\n", pawn_state_mask);

};

int position_in_hash(uint64_t pos_used_maskx4, uint64_t pawn_state_mask, expanded_status_t *pknown_status) {
  // compute hash
  HASH_TYPE hash = compute_hash(pos_used_maskx4, pawn_state_mask); 

  uint64_t searched_mask = (pawn_state_mask << 2);

  unsigned i;
  for (i = 0; i < main_hash.cell_first_free[hash]; ++i) {
    if ((main_hash.table[hash][i] & ~UINT64_C(0x3)) == searched_mask) {
      reduced_status_t status = main_hash.table[hash][i] & 0x3;
      switch(status) {
        default: {
          printf("%"PRIx64" %"PRIx64"\n", pos_used_maskx4, pawn_state_mask);
          printf("%"PRIx64"\n", main_hash.table[hash][i]);
          printf("%d %d\n", i, main_hash.cell_first_free[hash]);
          printf("hash=%x\n", compute_hash(pos_used_maskx4, pawn_state_mask));
          print_failure_summary();
          assert(0 && "invalid status during record hash extraction"); 
        };
        case S_RED_TIE:  *pknown_status = S_EXP_TIE; return 1;
        case S_RED_WIN0: *pknown_status = S_EXP_WIN0; return 1;
        case S_RED_WIN1: *pknown_status = S_EXP_WIN1; return 1;
        // case S_RED_UNK:  *pknown_status = S_EXP_UNK; return 1;
      };
      assert(0 && "invalid status during hash extraction");
    }
  }

  return 0;
};

#ifndef STEP_HASH_LIMIT
#define STEP_HASH_LIMIT 6
#endif

#define STEP_LOG 13
double start_time = 0;
unsigned long long step_counter[16];

unsigned long long explore_count = 0;


/* current_player is the player positioning the totem
 * given by the other player */
expanded_status_t explore_play_from_position(uint64_t pos_used_maskx4, uint64_t pawn_state_mask, int current_player, uint16_t possible_pos, uint16_t possible_indexes, int step) {
  explore_count++;
  unsigned i, j;

  expanded_status_t known_status = -1;
  if (step > STEP_HASH_LIMIT && position_in_hash(pos_used_maskx4, pawn_state_mask, &known_status)) {
    PRINTF("position found in hash\n");
    return known_status;
  }

  // quarto cases have been previously excluded 
  // (by called to explore_play_from_position)
  if (possible_indexes == 0) return S_EXP_TIE;

  /*if (position_is_quarto(posexp)) {
    uint8_t pos_status = (current_player ? S_RED_WIN1 : S_RED_WIN0);
    uint8_t pos_exp_status = (current_player ? S_EXP_WIN1 : S_EXP_WIN0);
    pos_set_red_status(pos, pos_status, current_player);
    return pos_exp_status;
  } else if (pos_get_position(posexp) == 0xffff) {
    uint8_t pos_status = S_RED_TIE;
    pos_set_red_status(pos, pos_status, current_player);
    return S_EXP_TIE;
  }*/

  static unsigned step_10 =0;
  if (step >= STEP_LOG) {
    step_counter[step]++;
    double delta = RDTSC64() - start_time;
    printf("step_%d %.3e[%.3f]\n", step, step_counter[step] / factoriel_square(15) * factoriel_square(step) , delta / 1.0e9);
  };

  uint8_t local_status = S_EXP_WIN0 | S_EXP_WIN1 | S_EXP_TIE;

  for (j = 0; j < 16; j++) {
    if (!(possible_indexes & (1 << j))) continue;
    uint16_t local_possible_indexes = possible_indexes ^ (1 << j);
    uint64_t new_pawn_state_mask = pawn_state_mask | (PAWN_ARRAY[j] << j);
    // status for the local totem
    uint8_t totem_status = 0;
    // index in the 4-chunk container
    unsigned container_id = 0;
    uint64_t __attribute__((aligned(32))) container_pos_mask[4] = {0};
    uint64_t __attribute__((aligned(32))) container_pawn_state[4] = {0};
    
    uint64_t list_pos_mask[16] = {0};
    uint16_t list_possible_pos[16] = {0};
    // current index in the list
    // for post quarto search processing
    unsigned list_pos_id = 0;

    for (i = 0; i < 16; ++i) {
      if (!(possible_pos & (1 << i))) continue;
      // remaining possible position
      uint16_t local_possible_pos = possible_pos ^ (1 << i);
      // local position used mask
      uint64_t new_pos_used_maskx4 = pos_used_maskx4 | (ONE_MASK_16x4 << i); 

      container_pos_mask[container_id]   = new_pos_used_maskx4;
      // TODO can be factorized outside innermost loop
      container_pawn_state[container_id] = new_pawn_state_mask;
      container_id++;

      list_pos_mask[list_pos_id] = new_pos_used_maskx4;
      list_possible_pos[list_pos_id] = local_possible_pos;
      list_pos_id++;

      if (container_id == 4) {
        container_id = 0;
        if (container_has_quarto(_mm256_load_si256((__m256i*) container_pos_mask), _mm256_load_si256((__m256i*) container_pawn_state))) {
            list_pos_id = 0;
            PRINTF("container has quarto\n");
            // at least one wining position has been found for this totem
            totem_status = (current_player ? S_EXP_WIN1 : S_EXP_WIN0);
            break;
        }
      }
    }

    if (list_pos_id == 0) {
      // wining position found

      break;
    };
    if (container_id != 0) { 
      // some positions remains in container
      // to be tested for quarto condition
      if (container_has_quarto(_mm256_load_si256((__m256i*) container_pos_mask), _mm256_load_si256((__m256i*) container_pawn_state))) {
            PRINTF("container has quarto\n");
        // at least one wining position has been found for this totem
        totem_status = (current_player ? S_EXP_WIN1 : S_EXP_WIN0);
        list_pos_id = 0;
      }
    }
      // must explore positions listed in list_pos_mask
      unsigned k;
      for (k = 0; k < list_pos_id; ++k) {
        totem_status |= explore_play_from_position(list_pos_mask[k], new_pawn_state_mask, 1 - current_player, list_possible_pos[k], local_possible_indexes, step - 1); 
      }

    // if it exist one totem which make the non current player win then 
    // he will pick it
    if (totem_status == S_EXP_WIN0 && current_player == 1) {
      local_status = S_EXP_WIN0; break;
    } else if (totem_status == S_EXP_WIN1 && current_player == 0) {
      local_status = S_EXP_WIN1; break;
    };
    local_status &= totem_status;
  }

  reduced_status_t stored_status = -1;
  switch (local_status) {
  case S_EXP_WIN0:
    stored_status = S_RED_WIN0; break;
  case S_EXP_WIN1:
    stored_status = S_RED_WIN1; break;
  case S_EXP_TIE:
    stored_status = S_RED_TIE; break;
  default:
    {
      if ((local_status & S_EXP_WIN0) && current_player == 0) {
        stored_status = S_RED_WIN0; 
        local_status  = S_EXP_WIN0;  
      } else if ((local_status & S_EXP_WIN1) && current_player == 1) {
        stored_status = S_RED_WIN1;
        local_status  = S_EXP_WIN1;
      } else {
        stored_status = S_RED_TIE;
        local_status  = S_EXP_TIE;
      };
      break;
    }
  };

  if (step > STEP_HASH_LIMIT) {
    store_position_in_hash(pos_used_maskx4, pawn_state_mask, stored_status, current_player);
    reduced_status_t check_status = 0;
    //int found_status = position_in_hash(pos_used_maskx4, pawn_state_mask, &check_status);
    //assert(found_status && (check_status == stored_status));
  }

  return local_status;
}

uint16_t build_used_indexes(uint64_t pos_used_maskx4, uint64_t pawn_state_mask) {
  unsigned i;
  uint16_t result_mask = 0;
  for (i = 0; i < 16; ++i) {
    if (!((pos_used_maskx4 >> i) & 0x1)) continue;
    uint64_t psm = pawn_state_mask >> i;
    uint64_t pawn = (psm & 1) | ((psm >> 15) & 0x2) | ((psm >> 30) & 0x4) | ((psm >> 45) & 0x8);
    result_mask |= 1 << pawn;
  };
  return result_mask;
}

/* player_id is the player index positioning <next_pawn> */
int get_next_position_to_play(unsigned* next_position, unsigned player_id, uint64_t pos_used_maskx4, uint64_t pawn_state_mask, unsigned next_pawn) {
  expanded_status_t goal_status = player_id ? S_EXP_WIN1 : S_EXP_WIN0;
  assert(__builtin_popcountll(pos_used_maskx4) % 4 == 0);
  unsigned step = 16 - __builtin_popcountll(pos_used_maskx4) / 4;
  uint16_t possible_pos = ~(pos_used_maskx4 & 0xffff);
  // adding color,shape, height, hole mask to find used pawns (0th pawn is always used)
  uint16_t possible_indexes = ~build_used_indexes(pos_used_maskx4, pawn_state_mask); //& ~((pawn_state_mask >> 48) | (pawn_state_mask >> 32) | (pawn_state_mask >> 16) | (pawn_state_mask));
  unsigned j;
  for (j = 0; j < 16; j++) {
    // exploring position i
    if (!(possible_pos & (1 << j))) continue;

    uint64_t new_pos_used_maskx4 = pos_used_maskx4 | (ONE_MASK_16x4 << j);
    uint64_t new_pawn_state_mask = pawn_state_mask | (PAWN_ARRAY[next_pawn] << j);
    expanded_status_t pos_status = explore_play_from_position(new_pos_used_maskx4, new_pawn_state_mask, 1 - player_id, possible_pos ^ (1 << j), possible_indexes ^ (1 << next_pawn), step - 1); 

    if (pos_status == goal_status) {
      *next_position = j;
      return 1;
    };
  }
  return 0;
}

void play_random(uint64_t pos_used_maskx4, uint64_t pawn_state_mask, unsigned *position, unsigned *pawn) {
  uint16_t possible_pos = ~(pos_used_maskx4 & 0xffff);
  // adding color,shape, height, hole mask to find used pawns (0th pawn is always used)
  uint16_t possible_indexes = ~build_used_indexes(pos_used_maskx4, pawn_state_mask); 
  printf("pi %"PRIx16"\n", possible_indexes);

  
  unsigned pawn_rel_id = rand() % 16;
  unsigned pos_rel_id = rand() % 16;

  while (!(possible_indexes & (1 << pawn_rel_id))) pawn_rel_id = rand() % 16;
  while (!(possible_pos & (1 << pos_rel_id))) pos_rel_id = rand() % 16;

  *position = pos_rel_id;
  *pawn = pawn_rel_id;

}

/* player_id is the player selectioning <next_pawn> */
int get_next_pawn_to_give(unsigned* next_pawn, unsigned player_id, uint64_t pos_used_maskx4, uint64_t pawn_state_mask) {
  printf("searching next pawn to give by playerd %d on %"PRIx64" %"PRIx64"\n", player_id, pos_used_maskx4, pawn_state_mask);

  expanded_status_t goal_status      = player_id ? S_EXP_WIN1 : S_EXP_WIN0;
  expanded_status_t anti_goal_status = (!player_id) ? S_EXP_WIN1 : S_EXP_WIN0;
  assert(__builtin_popcountll(pos_used_maskx4) % 4 == 0);
  unsigned step = 16 - __builtin_popcountll(pos_used_maskx4) / 4;
  uint16_t possible_pos = ~(pos_used_maskx4 & 0xffff);
  // adding color,shape, height, hole mask to find used pawns (0th pawn is always used)
  uint64_t possible_indexes = 0xfffe & ~((pawn_state_mask >> 48) | (pawn_state_mask >> 32) | (pawn_state_mask >> 16) | (pawn_state_mask));
  unsigned j;
  for (j = 0; j < 16; j++) {
    // exploring pawn j
    if (!(possible_indexes & (1 << j))) continue;

    expanded_status_t pawn_status = 0;

    // try every possible position with this pawn
    // (if (1 - player_id) can not find a wining position
    // then we must offer him this pawn
    unsigned i;
    for (i = 0; i < 16; ++i) {
      if (!(possible_pos & (1 << i))) continue;
      uint64_t new_pos_used_maskx4 = pos_used_maskx4 | (ONE_MASK_16x4 << i);
      uint64_t new_pawn_state_mask = pawn_state_mask | (PAWN_ARRAY[j] << i);
      // if position is quarto, skip this pawn
      if (position_is_quarto(new_pos_used_maskx4, new_pawn_state_mask)) continue;
      expanded_status_t known_status;
      int hash_status =  position_in_hash(new_pos_used_maskx4, new_pawn_state_mask, &known_status);
      if (hash_status) pawn_status |= known_status;
      //pawn_status |= explore_play_from_position(new_pos_used_maskx4, new_pawn_state_mask, player_id, possible_pos ^ (1 << i), possible_indexes ^ (1 << j), step - 1); 
    };

    if (pawn_status == goal_status) {
      *next_pawn = j;
      return 1;
    }

  }
  return 0;
}

void print_failure_summary(void) {
  unsigned max, min, i;
  unsigned long long count;
  double variance;
  count = max = min = main_hash.cell_first_free[0];
  variance = count * (double) count;

  for (i = 1; i < HASH_TABLE_SIZE; ++i) {
    unsigned local_count = main_hash.cell_first_free[i];
    variance += local_count * (double) local_count;
    count += local_count;
    if (local_count < min) min = local_count;
    if (local_count > max) max = local_count;
  };

  double avg = count / (double) HASH_TABLE_SIZE;

  printf("exploration stopped with %llu records, min=%d, max=%d, avg=%.3f\n", count, min, max, avg);
  printf("%llu position explored\n", explore_count);
  printf("variance %.3e %.2f \n", variance, sqrt(variance));
}

void build_new_position(uint64_t* ppos_used_maskx4, uint64_t* ppawn_state_mask, unsigned position, unsigned totem) {
  *ppos_used_maskx4 |= (ONE_MASK_16x4 << position);
  *ppawn_state_mask |= (PAWN_ARRAY[totem] << position);
};

unsigned extract_pawn(uint64_t pawn_state_mask, unsigned pawn_index) {
  uint64_t pawn_0 = (pawn_state_mask >> (pawn_index + 0 * 16)) & 0x1;
  uint64_t pawn_1 = (pawn_state_mask >> (pawn_index + 1 * 16)) & 0x1;
  uint64_t pawn_2 = (pawn_state_mask >> (pawn_index + 2 * 16)) & 0x1;
  uint64_t pawn_3 = (pawn_state_mask >> (pawn_index + 3 * 16)) & 0x1;
  return (pawn_0) | (pawn_1 << 1) | (pawn_2 << 2) | (pawn_3 << 3);
}

void display_position(uint64_t pos_used_maskx4, uint64_t pawn_state_mask, unsigned player_id) {
  unsigned i, j;
  printf("player_id=%u\n", player_id);
  printf("pos_used_maskx4=%016"PRIx64"\n", pos_used_maskx4);
  printf("pawn_state_mask=%016"PRIx64"\n", pawn_state_mask);
  uint16_t pos_mask = pos_used_maskx4;
  for (i = 0; i < 4; ++i ) printf("-----"); printf("\n");
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; ++j) {
      if (pos_mask & (1 << (4*i+j))) {
        unsigned pawn = extract_pawn(pawn_state_mask, 4*i+j);
        printf("| %x |", pawn);
      } else {
        printf("| - |");
      }
    }
    printf("\n");
    for (j = 0; j < 4; ++j ) printf("-----"); printf("\n");
  }

}



int main(void) {
  printf("hash init\n");
  init_hash();

  printf("launching exploration\n");
  start_time = RDTSC64();

  uint64_t pos0_pos_used_maskx4 = 0x0001000100010001ull;
  uint64_t pos0_pawn_state_mask = 0x0;

  uint64_t pos1_pos_used_maskx4 = 0x0002000200020002ull;
  uint64_t pos1_pawn_state_mask = 0x0;

  uint64_t pos2_pos_used_maskx4 = 0x0020002000200020ull;
  uint64_t pos2_pawn_state_mask = 0x0;


  print_failure_summary();

  printf("starting play\n");
  uint64_t pos_used_maskx4 = 0;
  uint64_t pawn_state_mask = 0;

  int player_id = 1;

  while (1) {
    printf("commands\n");
    printf(" 1   : display current position \n");
    printf(" 2   : get next pawn to play    \n");
    printf(" 3   : get next position to play    \n");
    printf(" 4   : play position\n");
    printf(" 5   : set player id\n");
    printf(" 6   : play optimal game \n");
    printf(" 7   : request hash for information on current position\n");
    printf(" 8   : request hash for information on arg position\n");
    printf(" 9   : clear position and pawn masks\n");
    printf(" 10  : explore plays from position 0\n");
    printf(" 11  : explore plays from position 1\n");
    printf(" 12  : explore plays from position 2\n");
    printf(" 13  : play random                  \n");
    printf(" 14  : check position for quarto    \n");
    printf(" 17  : stop \n");
    int command = -1;
    while (1 != scanf("%d", &command)) printf("1 argument expected: <command>\n");

    switch(command) {
    case 1: 
      printf("player id: %d\n", player_id);
      display_position(pos_used_maskx4, pawn_state_mask, player_id); 
      break;
    case 2: {
      unsigned next_pawn;
      unsigned status = get_next_pawn_to_give(&next_pawn, 1 - player_id, pos_used_maskx4, pawn_state_mask);
      printf("status=%d, pawn=%d\n", status, next_pawn);
      break;
    };
    case 3: {
      unsigned next_position;
      printf("next pawn ?");
      unsigned next_pawn;
      while (1 != scanf("%d", &next_pawn)) printf("1 argument expected: <next_pawn>\n");
      unsigned status = get_next_position_to_play(&next_position, player_id, pos_used_maskx4, pawn_state_mask, next_pawn);
      printf("status=%d, next_position=%d\n", status, next_position);
      break;
    };
    case 4: {
      // play pawn <pawn> at <position>
      int pawn, position;
      while (2 != scanf("%d %d", &pawn, &position)) printf("2 arguments expected: <pawn> <position>\n");
      // update position
      build_new_position(&pos_used_maskx4, &pawn_state_mask, position, pawn);
      player_id = 1 - player_id;
      break;
    };
    case 5:
      // manual change of current player_id
      while (1 != scanf("%d", &player_id)) printf("player_id expected\n");
      break;
    case 6:
    {
      // play optimal game from here
      printf("starting position\n");
      printf("player id: %d\n", player_id);
      display_position(pos_used_maskx4, pawn_state_mask, player_id); 

      uint64_t history_pos_used_maskx4[20];
      uint64_t history_pawn_state_mask[20];
      unsigned history_player_id[20];
      unsigned history_pawn[20];
      unsigned history_position[20];

      history_pos_used_maskx4[0] = pos_used_maskx4;
      history_pawn_state_mask[0] = pawn_state_mask;
      unsigned history_index = 1;

      while (pos_used_maskx4 != 0xffffffffffffffffull) {
        unsigned pawn, position;
        unsigned pawn_status = get_next_pawn_to_give(&pawn, 1 - player_id, pos_used_maskx4, pawn_state_mask);
        if (!pawn_status) { printf("no winning pawn to give: player %d loses \n", 1 - player_id); break; };
        unsigned position_status = get_next_position_to_play(&position, player_id, pos_used_maskx4, pawn_state_mask, pawn);
        if (!position_status)  { printf("no winning position for pawn: player %d loses \n", player_id); break; };
        build_new_position(&pos_used_maskx4, &pawn_state_mask, position, pawn);
        printf("player id: %d\n", player_id);
        display_position(pos_used_maskx4, pawn_state_mask, player_id); 

        //recording new state
        history_pos_used_maskx4[history_index] = pos_used_maskx4;
        history_pawn_state_mask[history_index] = pawn_state_mask;
        history_pawn[history_index] = pawn;
        history_position[history_index] = position;
        history_player_id[history_index] = player_id;
        history_index++;

        player_id = 1 - player_id;
      }

      // replaying game
      printf("starting position: \n");
      display_position(history_pos_used_maskx4[0], history_pawn_state_mask[0], player_id);
      unsigned k;
      for (k = 1; k < history_index; ++k) {
        printf("player %d was given pawn %d and set it at %d\n", history_player_id[k], history_pawn[k], history_position[k]);
        display_position(history_pos_used_maskx4[k], history_pawn_state_mask[k], player_id);

      }
      
      break;
    }
    case 7: {
      expanded_status_t known_status;
      int hash_status =  position_in_hash(pos_used_maskx4, pawn_state_mask, &known_status);
      if (!hash_status) printf("position not found in hash\n");
      else {
        display_position(pos_used_maskx4, pawn_state_mask, player_id);
        printf("STATUS: %d \n", known_status);
      };
      break;
    };
    case 8: {
      expanded_status_t known_status = -1;
      uint64_t local_pos_used_maskx4, local_pawn_state_mask;

      printf("pos_used_maskx4 pawn_state_mask" );
      while (2 != scanf("%"PRIx64" %"PRIx64"", &local_pos_used_maskx4, &local_pawn_state_mask)) {
        printf("two arguments expected: <pos_used_maskx4> <pawn_state_mask>");
      };
      
      local_pos_used_maskx4 |= (local_pos_used_maskx4 << 48) | (local_pos_used_maskx4 << 32) | (local_pos_used_maskx4 << 16);
      int hash_status =  position_in_hash(local_pos_used_maskx4, local_pawn_state_mask, &known_status);
      if (!hash_status) printf("position not found in hash\n");
      else {
        display_position(local_pos_used_maskx4, local_pawn_state_mask, player_id);
        printf("STATUS: %d \n", known_status);
      };
      break;
    };
    case 9: {
      printf("clearing position and pawn masks + player_id\n");
      pos_used_maskx4 = pawn_state_mask = 0;
      player_id = 1;
      display_position(pos_used_maskx4, pawn_state_mask, player_id);
      break;
    };
    case 10: {
      printf("exploring play from position\n");
      display_position(pos0_pos_used_maskx4, pos0_pawn_state_mask, 0);
      expanded_status_t status_from_pos0 = explore_play_from_position(pos0_pos_used_maskx4, pos0_pawn_state_mask, 0, 0xfffe, 0xfffe, 15);
      printf("status from pos0: %d\n", status_from_pos0);
      print_failure_summary();
      break;
    };
    case 11: {
      printf("exploring play from position\n");
      display_position(pos1_pos_used_maskx4, pos1_pawn_state_mask, 0);
      expanded_status_t status_from_pos1 = explore_play_from_position(pos1_pos_used_maskx4, pos1_pawn_state_mask, 0, 0xfffd, 0xfffe, 15);
      printf("status from pos0: %d\n", status_from_pos1);
      print_failure_summary();
      break;
    };
    case 12: {
      printf("exploring play from position\n");
      display_position(pos2_pos_used_maskx4, pos2_pawn_state_mask, 0);
      expanded_status_t status_from_pos2 = explore_play_from_position(pos2_pos_used_maskx4, pos2_pawn_state_mask, 0, 0xfffe, 0xfffe, 15);
      printf("status from pos0: %d\n", status_from_pos2);
      print_failure_summary();
      break;
    };
    case 13: {
      // play pawn <pawn> at <position>
      int pawn, position;
      play_random(pos_used_maskx4, pawn_state_mask, &position, &pawn);
      // update position
      printf("random play: pawn %d at position %d\n", pawn, position);
      build_new_position(&pos_used_maskx4, &pawn_state_mask, position, pawn);
      player_id = 1 - player_id;
      break;
    };
    case 14: {
      if (position_is_quarto(pos_used_maskx4, pawn_state_mask)) 
        printf("position is quarto, player %d wins !\n", 1 - player_id);
      break;
    };
    case 17: goto _stop;
    };
    

  }

  _stop:

  return 0;
}
