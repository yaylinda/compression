#include "common.h"
#include "burrows_wheeler.h"
#include "dictionary.h"
#include "FileInputStream.hpp"
#include "FileOutputStream.hpp"

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
static int g_total_bits = 0;
static BYTE g_bitstring = 0;
static int g_bit_index = 7;
static struct compressed_file_format g_meta;
static int g_remainder_bits_position_within_source_buffer = 0;

/////////////////////////////
// Private Prototypes
bool perform_compression(BYTE algorithm_id, InputStream *source, OutputStream *dest);
bool perform_decompression(InputStream *source, OutputStream *dest);


void write_bits_representation(OutputStream *fp, const char *representation);


int process_update_dictionary(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size);

int process_compress_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size);
int process_decompress_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size);
int process_bwt_encode_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size);
int process_bwt_decode_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size);

bool process_file(InputStream *source, OutputStream *outputFile,  int (*lambda)(OutputStream *outputFile, const BYTE *, int, int), DWORD source_size);

DWORD get_file_size(InputStream *source);


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
		FileInputStream *source;
		FileOutputStream *dest;
		bool open_file_1_test;
		bool open_file_2_test;

		source = new FileInputStream();
		dest = new FileOutputStream();
		open_file_1_test = source->initialize(argv[2]);
		open_file_2_test = dest->initialize(argv[3]);

		if (open_file_1_test && open_file_2_test) 
		{
			bool performance_test;
			bool compress = (argv[1][0] == 'c' || argv[1][0] == 'C');

			if (compress)
			{
				performance_test = perform_compression(ALGORITHM_ARITHMETIC,source,dest);

			}
			else
			{
				performance_test = perform_decompression(source,dest);
			}

			if (performance_test == true)
			{
				result = 0;
			}
			else
			{
				result = 1;
			}

		}
		else
		{
			result = 1;
		}

		source->shutdown();
		dest->shutdown();

		delete source, source = NULL;
		delete dest, dest = NULL;

	} 

	return result;
}

/////////////////////////////
// Private Functions
bool perform_compression(BYTE algorithm_id, InputStream *source, OutputStream *dest)
{
	bool result;
	bool process_file_test;

	source->seek(0, SEEK_BEGINNING);

	g_meta.m_dictionary = create_dictionary(algorithm_id);
	process_file_test = process_file(source, NULL, process_update_dictionary, get_file_size(source));
	finalize_dictionary(g_meta.m_dictionary);

	if (process_file_test)
	{
		printf("Dictionary built..\n");
	//					print_dictionary(g_meta.m_dictionary);

		//process the input file to the output file
		g_meta.m_magic_number = MAGIC_NUMBER;
		g_meta.m_version_number = VERSION;
		g_meta.m_algorithm_id = algorithm_id;

		dest->write(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1);
		dest->write(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1);
		dest->write(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1);

		{
			int num_dictionary_bytes;
			BYTE *dictionary_bytes;

			serialize_dictionary_to_bytes(g_meta.m_dictionary,&num_dictionary_bytes,&dictionary_bytes);
			dest->write(&num_dictionary_bytes,sizeof(num_dictionary_bytes),1);
			dest->write(dictionary_bytes,sizeof(dictionary_bytes[0]),num_dictionary_bytes);

			free(dictionary_bytes);
		}

		int remainder_bits_position;
		remainder_bits_position = dest->tell();

		//write some dummy data to acount for what could be
		g_meta.m_compressed_stream.m_number_of_remainder_bits = 0;
		dest->write(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(BYTE), 1);

		source->seek(0, SEEK_BEGINNING);

		process_file(source, dest, process_compress_buffer, get_file_size(source));

		// in case the compressor in question requires a final flush
		{
			const char *flush_representation;

			flush_representation = encode_symbol_to_bitstring_flush(g_meta.m_dictionary);
			write_bits_representation(dest, flush_representation);
		}


		if (g_bit_index != 7)
		{
			g_meta.m_compressed_stream.m_number_of_remainder_bits = 7 - g_bit_index;

			if (g_meta.m_compressed_stream.m_number_of_remainder_bits > 0)
			{
				dest->write(&g_bitstring, sizeof(BYTE), 1);
			}

			dest->seek(remainder_bits_position,SEEK_BEGINNING);
			dest->write(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(BYTE), 1);
	//				printf("number of remainder bits written[%d]\n", g_meta.m_compressed_stream.m_number_of_remainder_bits);
			dest->seek(0, SEEK_ENDING);
		}

		printf("total_bits[%d]\n", g_total_bits);

		result = true;
	}
	else
	{
		result = false;
	}

	return result;
}


bool perform_decompression(InputStream *source, OutputStream *dest)
{
	bool result;

	result = true; //TODO add error handling, haha


	source->read(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1);
	assert(g_meta.m_magic_number == MAGIC_NUMBER);
	source->read(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1);
	assert(g_meta.m_version_number == VERSION);
	source->read(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1);
	assert(g_meta.m_algorithm_id == ALGORITHM_HUFFMAN || g_meta.m_algorithm_id == ALGORITHM_ARITHMETIC);


	{
		int num_dictionary_bytes;
		BYTE *dictionary_bytes;

		source->read(&num_dictionary_bytes,sizeof(num_dictionary_bytes),1);
		dictionary_bytes = (BYTE *)malloc(sizeof(BYTE) * num_dictionary_bytes);
		source->read(dictionary_bytes,sizeof(dictionary_bytes[0]),num_dictionary_bytes);

		g_meta.m_dictionary = deserialize_bytes_to_dictionary(num_dictionary_bytes,dictionary_bytes);

		free(dictionary_bytes);
	}



	source->read(&g_meta.m_compressed_stream.m_number_of_remainder_bits, sizeof(g_meta.m_compressed_stream.m_number_of_remainder_bits), 1);
//			printf("number of remainder bits written[%d]\n", g_meta.m_compressed_stream.m_number_of_remainder_bits);

	
	int cur;

	cur = source->tell();
	source->seek(0,SEEK_ENDING);
	g_remainder_bits_position_within_source_buffer = source->tell() - cur;
	source->seek(cur,SEEK_BEGINNING);


	process_file(source, dest, process_decompress_buffer, get_file_size(source)-cur);

	//do flusing if we should need to
	{
		bool test;
		struct symbol decoded_symbol;

		test = decode_consume_bit_flush(g_meta.m_dictionary,&decoded_symbol);

		if (test == true)
		{
//				printf("decoded symbol[%c]\n", decoded_symbol);
			dest->write(&decoded_symbol,sizeof(decoded_symbol),1);
		}
	}

	return result;
}




void write_bit(OutputStream *fp, char c)
{
	if (c == '1')
	{
		g_bitstring = g_bitstring | (1 << g_bit_index);
	}

	g_bit_index--;

	if (g_bit_index == -1)
	{
//		printf("g_bitstring written[%d]\n", g_bitstring);
		fp->write(&g_bitstring, sizeof(BYTE), 1);
		g_bitstring = 0;
		g_bit_index = 7;
	}
}

void write_bits_representation(OutputStream *fp, const char *representation)
{
	if (representation != NULL)
	{
//		printf("symbol[%c], representation[%s]\n", sym.m_value, representation);

		while (*representation != '\0')
		{
			write_bit(fp,*representation);
			representation++;
			g_total_bits++;
		}
	}
}

int process_update_dictionary(OutputStream *fp, const BYTE *source_buffer, int max_size, int process_size)
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


int process_compress_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size)
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
		write_bits_representation(outputFile,representation);

//		printf("symbol [%c] is represented as [%s]\n\n",sym.m_value,representation == NULL ? "NULL" : representation);
	}

	return -1;
}


int process_decompress_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size)
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
				
				outputFile->write(&decoded_symbol,sizeof(decoded_symbol),1);
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


int process_bwt_encode_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size)
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

		outputFile->write(read_buffer,sizeof(BYTE),bytes_read);
	}
	while (symbols_written > 0 || bytes_read > 0);

	return 0;
}


int process_bwt_decode_buffer(OutputStream *outputFile, const BYTE *source_buffer, int max_size, int process_size)
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

		outputFile->write(read_buffer,sizeof(BYTE),symbols_read);
	//	printf("\t[%d][%d]\n",bytes_written,symbols_read);

	}
	while (bytes_written > 0 || symbols_read > 0);

//	printf("leave decode\n");

	return 0;
}



bool process_file(InputStream *source, OutputStream *dest, int (*lambda)(OutputStream *fp, const BYTE *, int, int), DWORD source_size)
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

		amount_read = source->read(source_buffer, sizeof(source_buffer[0]), sizeof(source_buffer));
		processed_status = lambda(dest, source_buffer, sizeof(source_buffer), amount_read);

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



DWORD get_file_size(InputStream *source)
{
	DWORD result;
	DWORD org_pos;

	org_pos = source->tell();
	source->seek(0, SEEK_ENDING);
	result = source->tell();
	source->seek(org_pos, SEEK_BEGINNING);

	return result;	
}










