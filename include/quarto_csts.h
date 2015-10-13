#ifndef __QUARTO_CSTS_H__

//#define HASH_CONTAINER _int128
#define HASH_CONTAINER_BYTE_SIZE 16
#define COMPRESSED_SIZE 64 
#define DEFAULT_CONTAINER_SIZE 256
#define DEFAULT_SIZE_INC       16
#define ELT_PER_CONTAINER ((HASH_CONTAINER_BYTE_SIZE * 8) / COMPRESSED_SIZE)

// status constant (enum  byte ?)
#define S_EXP_WIN0 1
#define S_EXP_WIN1 2
#define S_EXP_TIE  4
#define S_EXP_UNK  8

#define S_RED_WIN0 1
#define S_RED_WIN1 2
#define S_RED_TIE  3

#define S_UNK  0// unknown

#endif /* __QUARTO_CSTS_H__ */
