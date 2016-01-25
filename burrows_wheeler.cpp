#include "burrows_wheeler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>


#define SYMBOL_BATCH_COUNT 10
#define WORKING_SIZE_IN_BYTES 512
#define MAX_SYMBOL_COUNT 512


/////////////////////////////
// Private Structures

/////////////////////////////
// Global Variables
static int g_encoding_input_symbols_current;
static BYTE *g_encoding_input_symbols_buffer;

static int g_encoding_output_bytes_current;
static BYTE *g_encoding_output_bytes_buffer;

static int g_decoding_input_bytes_current;
static BYTE *g_decoding_input_bytes_buffer;

static int g_decoding_output_symbols_current;
static BYTE *g_decoding_output_symbols_buffer;


static bool g_encoding;

static int g_symbol_size;



/////////////////////////////
// Private Prototypes
bool bwt_flush_batchs(bool require_batch_count);
bool bwt_encode(const BYTE *source, int symbol_size, int symbol_count, BYTE *dest, int *index,int *bytes_written);
bool bwt_decode(const BYTE *source, int symbol_size, int symbol_count, int index, BYTE *dest,int *symbols_written);

void print_row(const BYTE *row, int symbol_size, int symbol_count);
void print_matrix(BYTE **matrix, int symbol_size, int symbol_count);
void in_place_quicksort(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count);
int partition(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count);
int row_compare(const BYTE *row1, const BYTE *row2, int symbol_size, int symbol_count);



/////////////////////////////
// Public Functions
void bwt_initialize(int symbol_size,bool encode)
{
	g_symbol_size = symbol_size;
	g_encoding = encode;
	g_encoding_input_symbols_buffer = (BYTE *)malloc(MAX_SYMBOL_COUNT * g_symbol_size);
	g_encoding_output_bytes_buffer = (BYTE *)malloc(WORKING_SIZE_IN_BYTES);

	g_decoding_input_bytes_buffer = (BYTE *)malloc(WORKING_SIZE_IN_BYTES);
	g_decoding_output_symbols_buffer = (BYTE *)malloc(MAX_SYMBOL_COUNT * g_symbol_size);


	g_encoding_input_symbols_current = 0;
	g_encoding_output_bytes_current = 0;

	g_decoding_input_bytes_current = 0;
	g_decoding_output_symbols_current = 0;

}


void bwt_encoding_write_symbols(const BYTE *source,int symbol_count,int *symbols_written)
{
//	printf("bwt encoding symbols[%d][%d]\n",g_encoding_input_symbols_current,symbol_count);
	*symbols_written = symbol_count;
	if (MAX_SYMBOL_COUNT - g_encoding_input_symbols_current < symbol_count)
	{
		*symbols_written = MAX_SYMBOL_COUNT - g_encoding_input_symbols_current;
	}

	if (*symbols_written < symbol_count)
	{
		bwt_flush();

		if (MAX_SYMBOL_COUNT - g_encoding_input_symbols_current < symbol_count)
		{
			*symbols_written = MAX_SYMBOL_COUNT - g_encoding_input_symbols_current;
		}

	}

	memcpy(&(g_encoding_input_symbols_buffer[g_encoding_input_symbols_current]),source,g_symbol_size*(*symbols_written));
	g_encoding_input_symbols_current += *symbols_written;
}




void bwt_encoding_read_bytes(BYTE *dest,int *byte_count,int buffer_size)
{
	*byte_count = g_encoding_output_bytes_current;
	if (*byte_count > buffer_size)
	{
		*byte_count = buffer_size;

	}

	memcpy(dest,g_encoding_output_bytes_buffer,*byte_count);

	memmove(g_encoding_output_bytes_buffer,&(g_encoding_output_bytes_buffer[*byte_count]),WORKING_SIZE_IN_BYTES - (*byte_count));
	g_encoding_output_bytes_current -= *byte_count;
//	printf("bwt encoding bytes[%d] [%d]\n",g_encoding_input_symbols_current,*byte_count);
}


void bwt_decoding_write_bytes(const BYTE *source,int byte_count,int *bytes_written)
{
	*bytes_written = byte_count;

	if (WORKING_SIZE_IN_BYTES - g_decoding_input_bytes_current < byte_count)
	{
		*bytes_written = WORKING_SIZE_IN_BYTES - g_decoding_input_bytes_current;
	}

	if (*bytes_written < byte_count)
	{
		bwt_flush();

		if (WORKING_SIZE_IN_BYTES - g_decoding_input_bytes_current < byte_count)
		{
			*bytes_written = WORKING_SIZE_IN_BYTES - g_decoding_input_bytes_current;
		}
	}

	memcpy(&(g_decoding_input_bytes_buffer[g_decoding_input_bytes_current]),source,*bytes_written);
	g_decoding_input_bytes_current += *bytes_written;
}


void bwt_decoding_read_symbols(BYTE *dest,int *symbol_count,int max_symbols)
{
	*symbol_count = g_decoding_output_symbols_current;
	if (*symbol_count > max_symbols)
	{
		*symbol_count = max_symbols;
	}

	memcpy(dest,g_decoding_output_symbols_buffer,*symbol_count * g_symbol_size);

	memmove(g_decoding_output_symbols_buffer,&(g_decoding_output_symbols_buffer[*symbol_count * g_symbol_size]),(MAX_SYMBOL_COUNT - *symbol_count) * g_symbol_size);
	g_decoding_output_symbols_current -= *symbol_count;
}


bool bwt_finish()
{
	return bwt_flush_batchs(false);
}


bool bwt_flush()
{
	return bwt_flush_batchs(true);
}




/////////////////////////////
// Private Functions
bool bwt_flush_batchs(bool require_batch_count)
{
	bool result;

//	printf("FLUSHING BATCHES[%d][%d][%c]\n",g_encoding_input_symbols_current,SYMBOL_BATCH_COUNT,"NY"[!!require_batch_count]);

	result = false;

	if (g_encoding)
	{
		while (g_encoding_input_symbols_current >= SYMBOL_BATCH_COUNT)
		{
			BYTE working_buffer[WORKING_SIZE_IN_BYTES];
			int index;
			int bytes_written;

			bwt_encode(g_encoding_input_symbols_buffer,g_symbol_size,SYMBOL_BATCH_COUNT,working_buffer,&index,&bytes_written);

			if (bytes_written + 1 < (WORKING_SIZE_IN_BYTES - g_encoding_output_bytes_current))
			{
				assert(index < SYMBOL_BATCH_COUNT);
				assert(index >= 0);

				g_encoding_output_bytes_buffer[g_encoding_output_bytes_current] = (BYTE)index;
				g_encoding_output_bytes_current++;

				memcpy(&(g_encoding_output_bytes_buffer[g_encoding_output_bytes_current]),working_buffer,bytes_written);
				g_encoding_output_bytes_current += bytes_written;

				memmove(g_encoding_input_symbols_buffer,&(g_encoding_input_symbols_buffer[g_symbol_size*SYMBOL_BATCH_COUNT]),(MAX_SYMBOL_COUNT - SYMBOL_BATCH_COUNT)*g_symbol_size);
				g_encoding_input_symbols_current -= SYMBOL_BATCH_COUNT;
	//			printf("wrote[%d][%d][%d][%d]\n",index,bytes_written,g_encoding_output_bytes_current,g_encoding_input_symbols_current);

				result = true; //made meaningful progress
			}
			else
			{
				//wasn't able to make progress, not enough output byte space remaining
				break;
			}
		}

		if (require_batch_count == false)
		{
			if (g_encoding_input_symbols_current > 0)
			{
				BYTE working_buffer[WORKING_SIZE_IN_BYTES];
				int index;
				int bytes_written;
				int symbol_count = g_encoding_input_symbols_current;

				bwt_encode(g_encoding_input_symbols_buffer,g_symbol_size,symbol_count,working_buffer,&index,&bytes_written);				

				if (bytes_written + 1 < (WORKING_SIZE_IN_BYTES - g_encoding_output_bytes_current))
				{
					assert(index < SYMBOL_BATCH_COUNT);
					assert(index >= 0);

					g_encoding_output_bytes_buffer[g_encoding_output_bytes_current] = (BYTE)index;
					g_encoding_output_bytes_current++;

					memcpy(&(g_encoding_output_bytes_buffer[g_encoding_output_bytes_current]),working_buffer,bytes_written+1);
					g_encoding_output_bytes_current += bytes_written;

					memmove(g_encoding_input_symbols_buffer,&(g_encoding_input_symbols_buffer[g_symbol_size*symbol_count]),symbol_count*g_symbol_size);
					g_encoding_input_symbols_current -= symbol_count;
					result = true; //made meaningful progress
//					printf("wrote remainder[%d][%d][%d][%d]\n",index,bytes_written+1,g_encoding_output_bytes_current,symbol_count);

				}

			}
		}
	}
	else
	{
		int decode_batch_size;
		static BYTE *working_buffer = (BYTE *)malloc(g_symbol_size * SYMBOL_BATCH_COUNT);

		decode_batch_size = (g_symbol_size * SYMBOL_BATCH_COUNT) + 1;

		while (g_decoding_input_bytes_current >= decode_batch_size)
		{
			int index;
			int symbols_written;

			index = g_decoding_input_bytes_buffer[0];
			assert(index < SYMBOL_BATCH_COUNT);
			assert(index >= 0);

			bwt_decode(g_decoding_input_bytes_buffer + 1,g_symbol_size,SYMBOL_BATCH_COUNT,index,working_buffer,&symbols_written);

			if (symbols_written < (MAX_SYMBOL_COUNT - g_decoding_output_symbols_current))
			{
				memcpy(&(g_decoding_output_symbols_buffer[g_decoding_output_symbols_current]),working_buffer,g_symbol_size*symbols_written);
				g_decoding_output_symbols_current += symbols_written;

				memmove(g_decoding_input_bytes_buffer,&(g_decoding_input_bytes_buffer[decode_batch_size]),WORKING_SIZE_IN_BYTES - decode_batch_size);
				g_decoding_input_bytes_current -= decode_batch_size;

//				printf("decoded[%d][%d]\n",index,symbols_written);

				result = true; //made meaningful progress
			}
			else
			{
				break; // wasn't able to make progress because there's not enough room to store the new batch of symbols
			}
		}

		if (require_batch_count == false)
		{
			int index;
			int symbols_written;

			index = g_decoding_input_bytes_buffer[0];
//			printf("decode remainder[%d][%d]\n",index,g_decoding_input_bytes_current);
			decode_batch_size = g_decoding_input_bytes_current;

			bwt_decode(g_decoding_input_bytes_buffer + 1,g_symbol_size,g_decoding_input_bytes_current-1,index,working_buffer,&symbols_written);

			if (symbols_written < (MAX_SYMBOL_COUNT - g_decoding_output_symbols_current))
			{
				memcpy(&(g_decoding_output_symbols_buffer[g_decoding_output_symbols_current]),working_buffer,g_symbol_size*symbols_written);
				g_decoding_output_symbols_current += symbols_written;

				memmove(g_decoding_input_bytes_buffer,&(g_decoding_input_bytes_buffer[decode_batch_size]),decode_batch_size);
				g_decoding_input_bytes_current -= decode_batch_size;

//				printf("decoded remainder[%d][%d][%d]\n",index,symbols_written,g_decoding_input_bytes_current);
				assert(g_decoding_input_bytes_current == 0); //this should have consumed our current input bytes

				result = true; //made meaningful progress
			}
			else
			{
				assert(false);
			}

		}
	}

//	printf("LEAVING FLUSHING BATCHES[%c]\n","NY"[!!result]);

	return result;

}



bool bwt_encode(const BYTE *source, int symbol_size, int symbol_count, BYTE *dest, int *index,int *bytes_written)
{
	// printf("entered bwt encode[%p][%d][%d][%p][%p]\n", source, symbol_size, symbol_count, dest, index);
	
	BYTE **matrix;
	int i;
	bool result;

	result = true;

	// error checking
	if (source == NULL || symbol_size <= 0 || symbol_count <= 0 || dest == NULL || index == NULL || bytes_written == NULL)
	{
		result = false;
	}
	else // no errors, begin bwt process
	{
		// initialize matrix
		matrix = (BYTE**)malloc(symbol_count*sizeof(BYTE*));

		for(i=0; i<symbol_count; i++)
		{
			matrix[i] = (BYTE*)malloc(symbol_size*symbol_count);
		}

		// built matrix of rotations
		for(i=0; i<symbol_count; i++)
		{
			int j;
			for(j=0; j<symbol_count; j++)
			{
				int src_index = i + j;
				if (src_index > symbol_count-1)
				{
					src_index = (src_index % symbol_count);
				}
				memcpy(&(matrix[i][j*symbol_size]), &(source[src_index*symbol_size]), symbol_size);
			}
		}
		// print_matrix(matrix, symbol_size, symbol_count);
		
		// sort matrix
		in_place_quicksort(matrix, 0, symbol_count-1, symbol_size, symbol_count);
		// printf("\n");
		 //print_matrix(matrix, symbol_size, symbol_count);
		
		// find index
		for (i=0; i<symbol_count; i++)
		{
			if(row_compare(source, matrix[i], symbol_size, symbol_count) == 0)
			{
				*index = i;
				break;
			}
		}
		// printf("index[%d]\n", *index);
		
		assert(i < symbol_count);

		// copy last column into dest
		for(i=0; i<symbol_count; i++)
		{
			// printf("[%p][%p]\n", &((*dest)[i*symbol_size]), (matrix[i][symbol_size*(symbol_count-1)]));
			memmove(&(dest[i*symbol_size]), &(matrix[i][symbol_size*(symbol_count-1)]), symbol_size);
		}
		// print_row(dest, symbol_size, symbol_count);

		*bytes_written = symbol_size * symbol_count;

		// free matrix memory
		for(i=0; i<symbol_count; i++)
		{
			free(matrix[i]);
		}
		free(matrix);
	}

	// printf("\nleaving bwt encode[%c]\n", "NY"[!!result]);
	return result;
}

bool bwt_decode(const BYTE *source, int symbol_size, int symbol_count, int index, BYTE *dest,int *symbols_written)
{
	BYTE **matrix;
	int i;
	int j;

//	printf("entering bwt_decode[%d][%d]\n",symbol_size,symbol_count);
	// initialize matrix
	matrix = (BYTE**)malloc(symbol_count*sizeof(BYTE*));

	for(i=0; i<symbol_count; i++)
	{
		matrix[i] = (BYTE*)malloc(symbol_size*symbol_count);
	}

//	 print_row(source, symbol_size, symbol_count);

	// printf("\n" );

	// do decode algorithm
//	printf("building matrix\n");
	for(i=0; i<symbol_count; i++)
	{
		for(j=0; j<symbol_count; j++)
		{
			memcpy(&(matrix[j][symbol_size]), &(matrix[j][0]), symbol_size*i);
		}

		for(j=0; j<symbol_count; j++)
		{
			memcpy(&(matrix[j][0]), &(source[j*symbol_size]), symbol_size);
		}
		
		in_place_quicksort(matrix, 0, symbol_count-1, symbol_size, i+1);

		// print_matrix(matrix, symbol_size, symbol_count);
	}
//	printf("\tbuilt matrix\n");

//	 print_matrix(matrix, symbol_size, symbol_count);

	memcpy(dest, matrix[index], symbol_size*symbol_count);
	*symbols_written = symbol_count;

	// free matrix memory
	for(i=0; i<symbol_count; i++)
	{
		free(matrix[i]);
	}
	free(matrix);

	// print_row(dest, symbol_size, symbol_count);

//	printf("leaving bwt_decode\n");

	return true;

}

void print_row(const BYTE *row, int symbol_size, int symbol_count)
{
	int j;
	for(j=0; j<symbol_count; j++)
	{
		printf("%c ", row[j*symbol_size]);
	}	
	printf("\n");
}

void print_matrix(BYTE **matrix, int symbol_size, int symbol_count)
{
	int i;
	for(i=0; i<symbol_count; i++)
	{
		print_row(matrix[i], symbol_size, symbol_count);
	}
	printf("\n");
}

void in_place_quicksort(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count)
{
	if(lo < hi)
	{
		int p;
		p = partition(matrix, lo, hi, symbol_size, symbol_count);
        in_place_quicksort(matrix, lo, p-1, symbol_size, symbol_count);
        in_place_quicksort(matrix, p+1, hi, symbol_size, symbol_count);
	}
}

int partition(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count)
{
	BYTE *pivot;
	int i;
	int j;

	pivot = matrix[hi];
	i = lo;

	for(j=lo; j<hi; j++)
	{
		if (row_compare(matrix[j], pivot, symbol_size, symbol_count) < 0) 
		{
			BYTE *temp = matrix[i];
			matrix[i] = matrix[j];
			matrix[j] = temp; 
			i++;
		}
	}
	BYTE *temp = matrix[i];
	matrix[i] = matrix[hi];
	matrix[hi] = temp; 

	return i;
}

int row_compare(const BYTE *row1, const BYTE *row2, int symbol_size, int symbol_count)
{
	int result;
	int i;

	result = 0;

	for (i=0; i<symbol_count; i++)
	{
		int compare;
		compare = memcmp(&(row1[i*symbol_size]), &(row2[i*symbol_size]), symbol_size);
		if (compare != 0)
		{
			result = compare;
			break;
		}
	}
	return result;
}



