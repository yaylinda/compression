#ifndef BURROWS_WHEELER__H
#define BURROWS_WHEELER__H
#include "common.h"


void bwt_initialize(int symbol_size,bool encode);
bool bwt_write_symbols(const BYTE *source,int symbol_count);
void bwt_read_bytes(BYTE *dest,int *count,int buffer_size);
bool bwt_flush();



#endif //BURROWS_WHEELER__H