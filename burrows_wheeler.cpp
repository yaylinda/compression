#include "burrows_wheeler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>


#define SYMBOL_COUNT 20
#define OUTPUT_WORKING_SIZE_IN_BYTES 2048


/////////////////////////////
// Private Structures

/////////////////////////////
// Global Variables
static int g_input_symbols_current = 0;
static BYTE *g_input_buffer;

static int g_output_bytes_available = 0;
static BYTE *g_output_buffer;

static bool g_encoding;



/////////////////////////////
// Private Prototypes
bool bwt_encode(const BYTE *source, int symbol_size, int symbol_count, BYTE *dest, int *index);
bool bwt_decode(const BYTE *source, int symbol_size, int symbol_count, int index, BYTE *dest);
void print_row(const BYTE *row, int symbol_size, int symbol_count);
void print_matrix(BYTE **matrix, int symbol_size, int symbol_count);
void in_place_quicksort(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count);
int partition(BYTE **matrix, int lo, int hi, int symbol_size, int symbol_count);
int row_compare(const BYTE *row1, const BYTE *row2, int symbol_size, int symbol_count);



/////////////////////////////
// Public Functions
void bwt_initialize(int symbol_size,bool encode)
{
	g_encoding = encode;
	g_input_buffer = (BYTE *)malloc(symbol_size*SYMBOL_COUNT);
	g_output_buffer = (BYTE *)malloc(OUTPUT_WORKING_SIZE_IN_BYTES);
}


bool bwt_write_symbols(const BYTE *source,int symbol_count)
{
	bool result;
	int symbols_left;

	result = true;

	symbols_left = SYMBOL_COUNT - g_input_symbols_current;

	memcpy(&(g_input_buffer[g_input_symbols_current]),source,symbol_size*symbols_left);

	if (symbols_left == 0)
	{
		bwt_flush();

		if (g_input_symbols_current == SYMBOL_COUNT)
		{
			result = false;
		}
	}


	return result;
}

void bwt_read_bytes(BYTE *dest,int *count,int buffer_size)
{
	*count = g_output_bytes_available;
	if (*count > buffer_size)
	{
		*count = buffer_size;

	}

	memcpy(dest,g_output_buffer,*count);

	memmov(g_output_buffer,&(g_output_buffer[*count]),OUTPUT_WORKING_SIZE_IN_BYTES - g_output_bytes_available);
	g_output_bytes_available -= *count;
}

bool bwt_flush()
{


}


/////////////////////////////
// Private Functions


bool bwt_encode(const BYTE *source, int symbol_size, int symbol_count, BYTE *dest, int *index)
{
	// printf("entered bwt encode[%p][%d][%d][%p][%p]\n", source, symbol_size, symbol_count, dest, index);
	
	BYTE **matrix;
	int i;
	bool result;

	result = true;

	// error checking
	if (source == NULL || symbol_size <= 0 || symbol_count <= 0 || dest == NULL || index == NULL)
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
		// print_matrix(matrix, symbol_size, symbol_count);
		
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
			memcpy(&(dest[i*symbol_size]), &(matrix[i][symbol_size*(symbol_count-1)]), symbol_size);
		}
		// print_row(dest, symbol_size, symbol_count);

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

bool bwt_decode(const BYTE *source, int symbol_size, int symbol_count, int index, BYTE *dest)
{
	BYTE **matrix;
	int i;
	int j;

	// initialize matrix
	matrix = (BYTE**)malloc(symbol_count*sizeof(BYTE*));

	for(i=0; i<symbol_count; i++)
	{
		matrix[i] = (BYTE*)malloc(symbol_size*symbol_count);
	}

	// print_row(source, symbol_size, symbol_count);

	// printf("\n" );

	// do decode algorithm
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

	// print_matrix(matrix, symbol_size, symbol_count);

	memcpy(dest, matrix[index], symbol_size*symbol_count);

	// free matrix memory
	for(i=0; i<symbol_count; i++)
	{
		free(matrix[i]);
	}
	free(matrix);

	// print_row(dest, symbol_size, symbol_count);

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



