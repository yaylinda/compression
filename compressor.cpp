#include "./common.h"
#include "./burrows_wheeler.h"
#include "./dictionary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>


#define NUM_PROGRESS_BARS 20

/*
	- definition of compressed file format
		- magic number: 1 DWORD
		- version number: 1 WORD
		- algorithm: 1 BYTE
		- algorithm data:
			- symbol length in bits: 1 WORD
			- dictionary size in bytes: 1 DWORD
			- dictionary bytes
			- number of remainder bits at last BYTE of the bitstream: 2 bits
			- compressed bitstream
		- crc: 1 DWORD
*/

/////////////////////////////
// Private Structures
struct compressed_stream
{
	BYTE m_number_of_remainder_bits;
	BYTE *m_bytestream;
};

struct compressed_file_format
{
	DWORD m_magic_number;
	WORD m_version_number;
	BYTE m_algorithm_id;
	DICTIONARY m_dictionary;
	struct compressed_stream m_compressed_stream;
	DWORD crc;
};

/////////////////////////////
// Global Variables
static FILE *g_output = NULL;
static int g_total_bits = 0;
static BYTE g_bitstring = 0;
static int g_bit_index = 7;
static struct compressed_file_format g_meta;
static int g_remainder_bits_position_within_source_buffer = 0;

/////////////////////////////
// Private Prototypes
bool open_file(const char* filename, FILE **fp, bool is_source);

int process_update_dictionary(const BYTE *source_buffer, int max_size, int process_size);
int process_compress_buffer(const BYTE *source_buffer, int max_size, int process_size);
int process_decompress_buffer(const BYTE *source_buffer, int max_size, int process_size);
int process_bwt_encode_buffer(const BYTE *source_buffer, int max_size, int process_size);
int process_bwt_decode_buffer(const BYTE *source_buffer, int max_size, int process_size);

bool process_file(FILE *source, int (*lambda)(const BYTE *, int, int), DWORD source_size);

DWORD get_file_size(FILE *source);


/////////////////////////////
// Public Functions
int main(int argc, char *argv[])
{
	int result = 20;

	if (argc != 4) 
	{
		printf("Usage: compressor OPTION source-filename dest-filename.\n");
		printf("OPTION = c -> compress; OPTION = d -> decompress\n");
		result = 0;
	} 
	else
	{
		FILE *source;
		bool open_file_1_test;
		bool open_file_2_test;

		open_file_1_test = open_file(argv[2], &source, true);
		open_file_2_test = open_file(argv[3], &g_output, false);

		if (open_file_1_test && open_file_2_test) 
		{
			bool compress = (argv[1][0] == 'c' || argv[1][0] == 'C');
			if (compress)
			{
				bool process_file_test;

				fseek(source, 0, SEEK_SET);

				g_meta.m_dictionary = create_dictionary();
				process_file_test = process_file(source, process_update_dictionary, get_file_size(source));
				finalize_dictionary(g_meta.m_dictionary);

				if (process_file_test)
				{
					print_dictionary(g_meta.m_dictionary);

					//process the input file to the output file
					g_meta.m_magic_number = MAGIC_NUMBER;
					g_meta.m_version_number = VERSION;
					g_meta.m_algorithm_id = ALGORITHM_HUFFMAN;

					fwrite(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1,g_output);
					fwrite(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1,g_output);
					fwrite(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1,g_output);

					{
						int num_dictionary_bytes;
						BYTE *dictionary_bytes;

						serialize_dictionary_to_bytes(g_meta.m_dictionary,&num_dictionary_bytes,&dictionary_bytes);
						fwrite(&num_dictionary_bytes,sizeof(num_dictionary_bytes),1,g_output);
						fwrite(dictionary_bytes,sizeof(dictionary_bytes[0]),num_dictionary_bytes,g_output);

						free(dictionary_bytes);
					}

					int remainder_bits_position;
					remainder_bits_position = ftell(g_output);

					//write some dummy data to acount for what could be
					g_meta.m_compressed_stream.m_number_of_remainder_bits = 0;
					fwrite(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(BYTE), 1, g_output);

					fseek(source, 0, SEEK_SET);

					process_file(source, process_compress_buffer, get_file_size(source));

					if (g_bit_index != 7)
					{
						g_meta.m_compressed_stream.m_number_of_remainder_bits = 7 - g_bit_index;

						if (g_meta.m_compressed_stream.m_number_of_remainder_bits > 0)
						{
							fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
						}

						fseek(g_output, remainder_bits_position, SEEK_SET);
						fwrite(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(BYTE), 1, g_output);
		//				printf("number of remainder bits written[%d]\n", g_meta.m_compressed_stream.m_number_of_remainder_bits);
						fseek(g_output, 0, SEEK_END);
					}

					printf("total_bits[%d]\n", g_total_bits);
					fclose(source);
					fclose(g_output);
					result = 0;
				}
				else
				{
					result = 1;
				}
			}
			else
			{
				fread(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1,source);
				assert(g_meta.m_magic_number == MAGIC_NUMBER);
				fread(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1,source);
				assert(g_meta.m_version_number == VERSION);
				fread(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1,source);
				assert(g_meta.m_algorithm_id == ALGORITHM_HUFFMAN);

				{
					int num_dictionary_bytes;
					BYTE *dictionary_bytes;

					fread(&num_dictionary_bytes,sizeof(num_dictionary_bytes),1,source);
					dictionary_bytes = (BYTE *)malloc(sizeof(BYTE) * num_dictionary_bytes);
					fread(dictionary_bytes,sizeof(dictionary_bytes[0]),num_dictionary_bytes,source);

					g_meta.m_dictionary = deserialize_bytes_to_dictionary(num_dictionary_bytes,dictionary_bytes);

					free(dictionary_bytes);
				}



				fread(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(g_meta.m_compressed_stream.m_number_of_remainder_bits), 1, source);
	//			printf("number of remainder bits written[%d]\n", g_meta.m_compressed_stream.m_number_of_remainder_bits);

				
				int cur;
				cur = ftell(source);
				fseek(source,0,SEEK_END);
				g_remainder_bits_position_within_source_buffer = ftell(source) - cur;
				fseek(source,cur,SEEK_SET);

				process_file(source, process_decompress_buffer, get_file_size(source)-cur);

				fclose(source);
				fclose(g_output);		
			}
		}
		else
		{
			result = 1;
		}
	} 

	return result;
}

/////////////////////////////
// Private Functions
bool open_file(const char* filename, FILE **fp, bool is_source)
{
	bool result = false;

	if (is_source == true)
	{
		*fp = fopen(filename, "rb");
	}
	else
	{
		*fp = fopen(filename, "wb");
	}

	if (*fp != NULL)
	{
		result = true;
	}
	else
	{
		printf("Problem opening file [%s].\n", filename);
	}

	return result;
}


void write_bit(char c)
{
	if (c == '1')
	{
		g_bitstring = g_bitstring | (1 << g_bit_index);
	}

	g_bit_index--;

	if (g_bit_index == -1)
	{
//		printf("g_bitstring written[%d]\n", g_bitstring);
		fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
		g_bitstring = 0;
		g_bit_index = 7;
	}
}

int process_update_dictionary(const BYTE *source_buffer, int max_size, int process_size)
{
	int i;

//	printf("process_update_dictionary[%d]\n",process_size);
	for (i=0; i<process_size; i++)
	{
		struct symbol sym;
		sym.m_value = source_buffer[i];

		update_dictionary(g_meta.m_dictionary,sym);
	}

	return -1;
}


int process_compress_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	int i;


//	printf("process_compress_buffer called[%d]\n",process_size);
	for (i=0; i<process_size; i++)
	{
		struct symbol sym;
		const char *representation;
		int j;

		sym.m_value = source_buffer[i];
		representation = encode_symbol_to_bitstring(g_meta.m_dictionary,sym);
		assert(representation != NULL);
//		printf("symbol[%c], representation[%s]\n", sym.m_value, representation);

		while (*representation != '\0')
		{
			write_bit(*representation);
			representation++;
			g_total_bits++;
		}
	}	
	return -1;
}


int process_decompress_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	static int s_current_processed_total = 0;
	int i;
	struct symbol decoded_symbol;

//	printf("**********decompress buffer\n");

//	printf("s_current_processed_total [%d]\n", s_current_processed_total);

	for (i = 0; i < process_size; i++)
	{
		BYTE cur_byte;
		int j;
		s_current_processed_total++;

		cur_byte = source_buffer[i];

		// printf("cur_byte[%d][%c]\n", cur_byte, cur_byte);


		for (j = 7;j >= 0;j--)
		{
			char bit;
			bool test;

			if (cur_byte & (1 << j))
			{
				bit = '1';
			}
			else
			{
				bit = '0';
			}

			test = decode_consume_bit(g_meta.m_dictionary,bit,&decoded_symbol);

			if (test == true)
			{
//				printf("decoded symbol[%c]\n", decoded_symbol);
				
				fwrite(&decoded_symbol,sizeof(decoded_symbol),1,g_output);
			}

			if (s_current_processed_total == g_remainder_bits_position_within_source_buffer)
			{
				if (j + g_meta.m_compressed_stream.m_number_of_remainder_bits == 8)
				{
	//				printf("[%d][%d][%d]found our last byte and the last legitimate bit\n",j,s_current_processed_total,g_remainder_bits_position_within_source_buffer);
					break;
				}
			}
		}
	}

	return 0;
}


int process_bwt_encode_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	int symbols_written;
	int bytes_read;
	int symbols_pos;

	symbols_pos = 0;

	do
	{
		BYTE read_buffer[2048];

		bwt_encoding_write_symbols(&(source_buffer[symbols_pos]),process_size,&symbols_written);
		symbols_pos += symbols_written;
		process_size -= symbols_written;

		bwt_encoding_read_bytes(read_buffer,&bytes_read,sizeof(read_buffer));

		fwrite(read_buffer,sizeof(BYTE),bytes_read,g_output);
	}
	while (symbols_written > 0 || bytes_read > 0);

	return 0;
}


int process_bwt_decode_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	int bytes_written;
	int symbols_read;
	int bytes_pos;

	bytes_pos = 0;

//	printf("enter decode\n");
	do
	{
		BYTE read_buffer[2048];

		bwt_decoding_write_bytes(&(source_buffer[bytes_pos]),process_size,&bytes_written);
		bytes_pos += bytes_written;
		process_size -= bytes_written;

		bwt_decoding_read_symbols(read_buffer,&symbols_read,sizeof(read_buffer));

		fwrite(read_buffer,sizeof(BYTE),symbols_read,g_output);
	//	printf("\t[%d][%d]\n",bytes_written,symbols_read);

	}
	while (bytes_written > 0 || symbols_read > 0);

//	printf("leave decode\n");

	return 0;
}



bool process_file(FILE *source, int (*lambda)(const BYTE *, int, int), DWORD source_size)
{
	bool result;
	BYTE source_buffer[64];
	DWORD amount_left;
	float current_bar_percentile;

	result = true;
	current_bar_percentile = 0.0f;
	amount_left = source_size;

	printf("[");

	while (amount_left > 0)
	{
		int amount_read;
		int processed_status;

		amount_read = fread(source_buffer, sizeof(source_buffer[0]), sizeof(source_buffer), (FILE*)source);
		processed_status = lambda(source_buffer, sizeof(source_buffer), amount_read);

		amount_left -= amount_read;

		// printf("amount_read[%d] amount_left[%llu]\n", amount_read, amount_left);

		current_bar_percentile += ((float)amount_read / source_size) * NUM_PROGRESS_BARS;
		
		int num_bars = (int)floor(current_bar_percentile+EPSILON);

		int i;
		for (i=0; i<num_bars; i++)
		{
			printf("-");
		}

		current_bar_percentile -= num_bars;

		if ((amount_read == 0) && (amount_left != 0))
		{
			result = false;
			break;
		}
	}

	printf("]\n");

	return result;
}



DWORD get_file_size(FILE *source)
{
	DWORD result;
	DWORD org_pos;

	org_pos = ftell(source);
	fseek(source, 0, SEEK_END);
	result = ftell(source);
	fseek(source, org_pos, SEEK_SET);

	return result;
}











