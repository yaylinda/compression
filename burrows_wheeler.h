#ifndef BURROWS_WHEELER__H
#define BURROWS_WHEELER__H
#include "common.h"

bool bwt_encode(const BYTE *source, int symbol_size, int symbol_count, BYTE *dest, int *index);

bool bwt_decode(const BYTE *source, int symbol_size, int symbol_count, int index, BYTE *dest);

void print_row(const BYTE *row, int symbol_size, int symbol_count);

#endif //BURROWS_WHEELER__H