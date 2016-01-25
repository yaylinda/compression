#ifndef DICTIONARY__H
#define DICTIONARY__H


#include "./common.h"


typedef void * DICTIONARY;


DICTIONARY create_dictionary();
void destroy_dictonary(DICTIONARY dictionary);

void update_dictionary(DICTIONARY dictionary,struct symbol sym);
bool finalize_dictionary(DICTIONARY dictionary);

void serialize_dictionary_to_bytes(DICTIONARY dictionary,int *num_bytes,BYTE **bytes);
DICTIONARY deserialize_bytes_to_dictionary(int num_bytes,BYTE *bytes);

const char *encode_symbol_to_bitstring(DICTIONARY dictionary,struct symbol sym);
bool decode_consume_bit(DICTIONARY dictionary,char bit_representation, struct symbol *decoded_symbol);


void print_dictionary(DICTIONARY dictionary);



#endif // DICTIONARY__H