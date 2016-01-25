#ifndef BURROWS_WHEELER__H
#define BURROWS_WHEELER__H
#include "common.h"


void bwt_initialize(int symbol_size,bool encode);

void bwt_encoding_write_symbols(const BYTE *source,int symbol_count,int *symbols_written);
void bwt_encoding_read_bytes(BYTE *dest,int *byte_count,int buffer_size);

void bwt_decoding_write_bytes(const BYTE *source,int byte_count,int *bytes_written);
void bwt_decoding_read_symbols(BYTE *dest,int *symbol_count,int buffer_size);

bool bwt_flush();
bool bwt_finish();


#endif //BURROWS_WHEELER__H