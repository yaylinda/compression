#include "./dictionary.h"

#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

/////////////////////////////
// private defines

#define MAX_REPRESENTATION_LENGTH 2048 // the max number of bits an encoded symbol can be

#define NEWICK_RECURSE_LEFT 97
#define NEWICK_RECURSE_RIGHT 98
#define NEWICK_POP 99
#define NEWICK_SYMBOL 100

#define NONE_OF_THE_WAY (0x0)
#define ONE_QUARTER (0xFFFFFFFF / 4)
#define HALF_WAY (0xFFFFFFFF / 2)
#define THREE_QUARTERS (3 * (0xFFFFFFFF/4))
#define ALL_THE_WAY (0xFFFFFFFF)

#define SHOULD_SCALE_HALF(high,low) (high < HALF_WAY || low > HALF_WAY)
#define SHOULD_SCALE_QUARTER(high,low) (low > ONE_QUARTER && high < THREE_QUARTERS)

/////////////////////////////
// Private Structures
struct node
{
	struct node *m_left; // 1
	struct node *m_right; // 0
	struct symbol_info m_symbol_info;
};

struct newick_structure
{
	DWORD m_num_symbols;
	BYTE *m_symbols;
	DWORD m_num_commands;
	BYTE *m_commands;

	int m_symbol_index;
	int m_command_index;
};


struct huffman_structure
{
	struct node *m_head;
	struct node *m_decode_cursor;

	bool m_found;
	int m_depth;
};

struct arithmetic_structure
{
	DWORD *m_lower_precision;
	DWORD *m_higher_precision;
	int m_total_symbols;

	DWORD m_interval_low;
	DWORD m_interval_high;
	int m_num_splits;

	char m_bit_buffer[32];
	int m_bit_buffer_index;
	DWORD m_z;
	bool m_is_z_initialized;
};

struct dictionary_internal
{
	int m_num_symbols;
	struct symbol_info *m_symbols;

	BYTE m_algorithm_id;

	struct huffman_structure m_huffman;
	struct arithmetic_structure m_arithmetic;

	int m_representation_length;
	char m_representation[MAX_REPRESENTATION_LENGTH]; // literally the characters '0' or '1' to represent a bit string
};



/////////////////////////////
// Global Variables


/////////////////////////////
// Private Prototypes
void rescale_half(struct dictionary_internal *dictionary, char bit_representation);
void rescale_quarter(struct dictionary_internal *dictionary, char bit_representation);
int find_symbol_index(struct dictionary_internal *dictionary,struct symbol sym);
void find_symbol(struct dictionary_internal *dictionary,struct symbol sym, struct node *current_node);

int find_min_node(int num_nodes, struct node **nodes);
void make_tree(struct dictionary_internal *dictionary);
void free_tree(struct node *head);


/*void initialize_newick_structure(struct newick_structure *newick);
void print_newick_structure(struct newick_structure *newick);
void write_newick(FILE *fp,struct newick_structure *newick);
void read_newick(FILE *fp,struct newick_structure *newick);

void newick_from_tree(struct node *head, struct newick_structure *newick);
struct node *tree_from_newick(struct newick_structure *newick);
*/


/////////////////////////////
// Public Functions

DICTIONARY create_dictionary(BYTE algorithm_id)
{
	struct dictionary_internal *addition;

	addition = (struct dictionary_internal *)malloc(sizeof(struct dictionary_internal));

	addition->m_algorithm_id = algorithm_id;

	addition->m_arithmetic.m_lower_precision = NULL;
	addition->m_arithmetic.m_higher_precision = NULL;

	addition->m_num_symbols = 0;
	addition->m_representation_length = 0;

	addition->m_symbols = NULL;
	addition->m_huffman.m_head = NULL;

	return (DICTIONARY)addition;
}

void destroy_dictonary(DICTIONARY dictionary)
{
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)dictionary;

	free(alias->m_symbols);
	alias->m_symbols = NULL;

	if (alias->m_algorithm_id == ALGORITHM_HUFFMAN)
	{
		free_tree(alias->m_huffman.m_head);
		alias->m_huffman.m_head = NULL;
		alias->m_huffman.m_decode_cursor = NULL;
	}
	else
	{
		if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
		{
			free(alias->m_arithmetic.m_lower_precision);
			alias->m_arithmetic.m_lower_precision = NULL;

			free(alias->m_arithmetic.m_higher_precision);
			alias->m_arithmetic.m_higher_precision = NULL;
		}
	}

	free(alias);
}


void update_dictionary(DICTIONARY dictionary,struct symbol sym)
{
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)dictionary;

	int i;
	for (i = 0;i < alias->m_num_symbols;i++)
	{
		if (alias->m_symbols[i].m_symbol.m_value == sym.m_value) 
		{
			alias->m_symbols[i].m_count++;
			break;
		}
	}

	if (i == alias->m_num_symbols)
	{
		alias->m_num_symbols++;
		alias->m_symbols = (struct symbol_info *)realloc(alias->m_symbols, sizeof(struct symbol_info)*alias->m_num_symbols);
		alias->m_symbols[alias->m_num_symbols - 1].m_count = 1;
		alias->m_symbols[alias->m_num_symbols - 1].m_symbol.m_value = sym.m_value; 
	}
}

bool finalize_dictionary(DICTIONARY dictionary)
{
	bool result;
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)dictionary;

	result = false;

	if (alias->m_algorithm_id == ALGORITHM_HUFFMAN)
	{
		make_tree(alias);

		if (alias->m_huffman.m_head != NULL)
		{
			alias->m_huffman.m_decode_cursor = alias->m_huffman.m_head;
			result = true;
		}
	}
	else
	{
		if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
		{
			alias->m_arithmetic.m_lower_precision = (DWORD *)malloc(sizeof(DWORD) * alias->m_num_symbols);
			alias->m_arithmetic.m_higher_precision = (DWORD *)malloc(sizeof(DWORD) * alias->m_num_symbols);
			alias->m_arithmetic.m_total_symbols = 0;
			alias->m_arithmetic.m_interval_low = NONE_OF_THE_WAY;
			alias->m_arithmetic.m_interval_high = ALL_THE_WAY;
			alias->m_arithmetic.m_num_splits = 0;
			alias->m_arithmetic.m_bit_buffer_index = 0;
			alias->m_arithmetic.m_is_z_initialized = false;
			alias->m_arithmetic.m_z = 0;

			int i;
			DWORD previous_count = 0;
			for(i=0; i<alias->m_num_symbols; i++)
			{
				alias->m_arithmetic.m_lower_precision[i] = previous_count;
				previous_count += alias->m_symbols[i].m_count;
			}

			for(i=0; i<alias->m_num_symbols; i++)
			{
				alias->m_arithmetic.m_higher_precision[i] = alias->m_arithmetic.m_lower_precision[i] + alias->m_symbols[i].m_count;
			}

			alias->m_arithmetic.m_total_symbols = previous_count;

//			printf("total symbols[%d]  num symbols[%d]\n",alias->m_arithmetic.m_total_symbols, alias->m_num_symbols);
		}
	}

//	print_dictionary(dictionary);

	return result;
}


void serialize_dictionary_to_bytes(DICTIONARY dictionary,int *num_bytes,BYTE **bytes)
{
	struct dictionary_internal *alias;
	BYTE *cursor;

	alias = (struct dictionary_internal *)dictionary;

	*num_bytes = sizeof(int) + (alias->m_num_symbols * sizeof(struct symbol_info));
	*bytes = (BYTE *)malloc(sizeof(BYTE) * *num_bytes);
	cursor = *bytes;

	memcpy(cursor,&(alias->m_algorithm_id),sizeof(alias->m_algorithm_id));
	cursor += sizeof(alias->m_algorithm_id);

	memcpy(cursor,&(alias->m_num_symbols),sizeof(int));
	cursor += sizeof(alias->m_num_symbols);

	memcpy(cursor,alias->m_symbols,(alias->m_num_symbols * sizeof(struct symbol_info)));
	cursor += alias->m_num_symbols * sizeof(struct symbol_info);
}


DICTIONARY deserialize_bytes_to_dictionary(int num_bytes,BYTE *bytes)
{
	DICTIONARY result;
	BYTE algorithm_id;
	BYTE *cursor;
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)result;
	cursor = bytes;

	memcpy(&algorithm_id,cursor,sizeof(alias->m_algorithm_id));
	cursor += sizeof(alias->m_algorithm_id);

	result = create_dictionary(algorithm_id);
	alias = (struct dictionary_internal *)result;


	memcpy(&(alias->m_num_symbols),cursor,sizeof(alias->m_num_symbols));
	cursor += sizeof(alias->m_num_symbols);

	alias->m_symbols = (struct symbol_info *)malloc(sizeof(struct symbol_info) * alias->m_num_symbols);
	memcpy(alias->m_symbols,cursor,sizeof(struct symbol_info) * alias->m_num_symbols);
	cursor += sizeof(struct symbol_info) * alias->m_num_symbols;

	bool test;

	test = finalize_dictionary(result);
	if (test == false)
	{
		result = NULL;
	}

	return result;
}



void wonky_fill_in(char *buffer,char first,char rest,int rest_count)
{
	int i;


	buffer[0] = first;

	for(i = 0;i < rest_count;i++)
	{
		buffer[i+1] = rest;
	}

	buffer[i+1] = '\0';
}


DWORD round_div(DWORD dividend, DWORD divisor)
{
    return (dividend + (divisor / 2)) / divisor;
}



static char s_working_buffer[MAX_REPRESENTATION_LENGTH];

const char *encode_symbol_to_bitstring(DICTIONARY dictionary,struct symbol sym)
{
	char *result;
	struct dictionary_internal *alias;

//	printf("push encode_symbol_to_bitstring\n");
	result = NULL;
	alias = (struct dictionary_internal *)dictionary;

	if (alias->m_algorithm_id == ALGORITHM_HUFFMAN)
	{
		alias->m_huffman.m_found = false;
		alias->m_huffman.m_depth = 0;

		find_symbol(alias,sym,alias->m_huffman.m_head);

		if (alias->m_huffman.m_found == true)
		{
			result = alias->m_representation;
		}
	}
	else
	{
		if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
		{
			DWORD diff;
			int symbol_index;
			int write_index;
			DWORD higher_precision;
			DWORD lower_precision;

			diff = alias->m_arithmetic.m_interval_high - alias->m_arithmetic.m_interval_low;
			symbol_index = find_symbol_index(alias, sym);

//			printf("before interval high[%llu]  interval low[%llu] symbol[%c] symbol_higher[%llu] symbol_lower[%llu]\n",alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low, sym.m_value, alias->m_arithmetic.m_higher_precision[symbol_index],alias->m_arithmetic.m_lower_precision[symbol_index]);

			higher_precision = diff * alias->m_arithmetic.m_higher_precision[symbol_index];
			lower_precision = diff * alias->m_arithmetic.m_lower_precision[symbol_index];

			alias->m_arithmetic.m_interval_high = alias->m_arithmetic.m_interval_low + round_div(higher_precision, alias->m_arithmetic.m_total_symbols);
			alias->m_arithmetic.m_interval_low = alias->m_arithmetic.m_interval_low + round_div(lower_precision, alias->m_arithmetic.m_total_symbols);

			write_index = 0;

//			printf("after interval high[%llu]  interval low[%llu]\n",alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low);

//			printf("after[%llu][%llu][%llu][%llu][%d]\n",alias->m_arithmetic.m_interval_high,alias->m_arithmetic.m_interval_low,higher_precision,lower_precision,symbol_index);

			// rescaling 1
			while (SHOULD_SCALE_HALF(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low))
			{
//				printf("scaling1! [%llu][%llu]\n",alias->m_arithmetic.m_interval_high,alias->m_arithmetic.m_interval_low);
				assert(write_index < MAX_REPRESENTATION_LENGTH);

				if (alias->m_arithmetic.m_interval_high < HALF_WAY)
				{
					//flush some encoding back to our caller
					wonky_fill_in(&(s_working_buffer[write_index]),'0','1',alias->m_arithmetic.m_num_splits);
					write_index += 1 + alias->m_arithmetic.m_num_splits;
					result = s_working_buffer;

					alias->m_arithmetic.m_interval_low = alias->m_arithmetic.m_interval_low * 2;
					alias->m_arithmetic.m_interval_high = alias->m_arithmetic.m_interval_high * 2;
					alias->m_arithmetic.m_num_splits = 0;
				}
				else if (alias->m_arithmetic.m_interval_low > HALF_WAY)
				{
					wonky_fill_in(&(s_working_buffer[write_index]),'1','0',alias->m_arithmetic.m_num_splits);
					write_index += 1 + alias->m_arithmetic.m_num_splits;

					alias->m_arithmetic.m_interval_low = 2 * (alias->m_arithmetic.m_interval_low - HALF_WAY);
					alias->m_arithmetic.m_interval_high = 2 * (alias->m_arithmetic.m_interval_high - HALF_WAY);
					alias->m_arithmetic.m_num_splits = 0;

					result = s_working_buffer;
				}
			}

//			printf("finished scaling1!\n");


			// rescaling 2
			while (SHOULD_SCALE_QUARTER(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low))
			{
//				printf("scaling2! [%llu][%llu]\n",alias->m_arithmetic.m_interval_high,alias->m_arithmetic.m_interval_low);

				alias->m_arithmetic.m_interval_low = 2 * (alias->m_arithmetic.m_interval_low - ONE_QUARTER);
				alias->m_arithmetic.m_interval_high = 2 * (alias->m_arithmetic.m_interval_high - ONE_QUARTER);
				alias->m_arithmetic.m_num_splits++;
			}
//			printf("finished scaling2!\n");

		}
	}

//	printf("pop encode_symbol_to_bitstring\n");

	return result;
}

const char *encode_symbol_to_bitstring_flush(DICTIONARY dictionary)
{
	struct dictionary_internal *alias;
	char *result;

	alias = (struct dictionary_internal *)dictionary;

	if (alias->m_algorithm_id == ALGORITHM_HUFFMAN)
	{
		result = NULL;
	}
	else
	{
		if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
		{
			int j;

			alias->m_arithmetic.m_num_splits++;

			if (alias->m_arithmetic.m_interval_low <= ONE_QUARTER)
			{
				wonky_fill_in(s_working_buffer,'0','1',alias->m_arithmetic.m_num_splits);
			}
			else
			{
				wonky_fill_in(s_working_buffer,'1','0',alias->m_arithmetic.m_num_splits);
			}

			result = s_working_buffer;

//			printf("flushing [%s]\n",result);
		}
	}

	return result;
}

bool decode_consume_bit(DICTIONARY dictionary,char bit_representation, struct symbol *decoded_symbol)
{
	struct dictionary_internal *alias;
	bool result;

	result = false;

	alias = (struct dictionary_internal *)dictionary;

	if (alias->m_algorithm_id == ALGORITHM_HUFFMAN)
	{
		if (bit_representation == '1')
		{
			alias->m_huffman.m_decode_cursor = alias->m_huffman.m_decode_cursor->m_left;
		}
		else
		{
			if (bit_representation == '0')
			{
				alias->m_huffman.m_decode_cursor = alias->m_huffman.m_decode_cursor->m_right;
			}
		}

		if (alias->m_huffman.m_decode_cursor->m_left == NULL && alias->m_huffman.m_decode_cursor->m_right == NULL)
		{
			result = true;
			*decoded_symbol = alias->m_huffman.m_decode_cursor->m_symbol_info.m_symbol;
			alias->m_huffman.m_decode_cursor = alias->m_huffman.m_head; // reset the cursor for the next round
		}
	}
	else
	{
		if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
		{
			if (alias->m_arithmetic.m_bit_buffer_index < 32)
			{
				alias->m_arithmetic.m_bit_buffer[alias->m_arithmetic.m_bit_buffer_index] = bit_representation;
				alias->m_arithmetic.m_bit_buffer_index++;
			}
			else
			{
				if (alias->m_arithmetic.m_is_z_initialized == false)
				{
					int pos;

					alias->m_arithmetic.m_z = 0;
					for (pos = 0; pos < alias->m_arithmetic.m_bit_buffer_index; pos++)
					{
						if (alias->m_arithmetic.m_bit_buffer[pos] == '1')
						{
							alias->m_arithmetic.m_z |= (1 << (32 - pos));
						}
					}

					alias->m_arithmetic.m_is_z_initialized = true;
				}
				else
				{
					if (!SHOULD_SCALE_HALF(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low) 
						&& !SHOULD_SCALE_QUARTER(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low))
					{
						int j;
						// decode a symbol
						for (j=0; j<alias->m_num_symbols; j++)
						{
							DWORD diff;
							DWORD b0;
							DWORD a0;
							DWORD higher_precision;
							DWORD lower_precision;

							diff = alias->m_arithmetic.m_interval_high - alias->m_arithmetic.m_interval_low;

							higher_precision = diff * alias->m_arithmetic.m_higher_precision[j];
							lower_precision = diff * alias->m_arithmetic.m_lower_precision[j];

							b0 = alias->m_arithmetic.m_interval_low + round_div(higher_precision, alias->m_arithmetic.m_total_symbols);
							a0 = alias->m_arithmetic.m_interval_low + round_div(lower_precision, alias->m_arithmetic.m_total_symbols);

							if (a0 <= alias->m_arithmetic.m_z && alias->m_arithmetic.m_z < b0)
							{
								alias->m_arithmetic.m_interval_low = a0;
								alias->m_arithmetic.m_interval_high = b0;
								*decoded_symbol = alias->m_symbols[j].m_symbol;
								result = true; // symbol found!
								break;
							}
						}
					}
					rescale_half(alias, bit_representation);
					rescale_quarter(alias, bit_representation);
				}
			}
		}
	}
	return result;
}

bool decode_consume_bit_flush(DICTIONARY dictionary, struct symbol *decoded_symbol)
{
	struct dictionary_internal *alias;
	bool result;

	alias = (struct dictionary_internal *)dictionary;
	result = false;

	if (alias->m_algorithm_id == ALGORITHM_ARITHMETIC)
	{

		if (alias->m_arithmetic.m_is_z_initialized == false)
		{
			int pos;

			alias->m_arithmetic.m_z = 0;
			for (pos = 0; pos < alias->m_arithmetic.m_bit_buffer_index; pos++)
			{
				if (alias->m_arithmetic.m_bit_buffer[pos] == '1')
				{
					alias->m_arithmetic.m_z |= (1 << (32 - pos));
				}
			}

			alias->m_arithmetic.m_is_z_initialized = true;
		}


		if (!SHOULD_SCALE_HALF(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low) 
			&& !SHOULD_SCALE_QUARTER(alias->m_arithmetic.m_interval_high, alias->m_arithmetic.m_interval_low))
		{
			int j;
			// decode a symbol
			for (j=0; j<alias->m_num_symbols; j++)
			{
				DWORD diff;
				DWORD b0;
				DWORD a0;

				diff = alias->m_arithmetic.m_interval_high - alias->m_arithmetic.m_interval_low;

				b0 = alias->m_arithmetic.m_interval_low + (diff * alias->m_arithmetic.m_higher_precision[j])/alias->m_arithmetic.m_total_symbols;
				a0 = alias->m_arithmetic.m_interval_low + (diff * alias->m_arithmetic.m_lower_precision[j])/alias->m_arithmetic.m_total_symbols;

				if (a0 <= alias->m_arithmetic.m_z && alias->m_arithmetic.m_z < b0)
				{
					alias->m_arithmetic.m_interval_low = a0;
					alias->m_arithmetic.m_interval_high = b0;
					*decoded_symbol = alias->m_symbols[j].m_symbol;
					result = true; // symbol found!
					break;
				}
			}
		}
		// rescale_half(alias, bit_representation);
		// rescale_quarter(alias, bit_representation);
	}

	return result;
}

void print_dictionary(DICTIONARY dictionary)
{
	struct dictionary_internal *alias;
	int total;
	int i;

	alias = (struct dictionary_internal *)dictionary;
	total = 0;

	for (i = 0;i < alias->m_num_symbols;i++)
	{
		printf("symbol[%d], frequency[%d]\n", alias->m_symbols[i].m_symbol.m_value, alias->m_symbols[i].m_count);
		total += alias->m_symbols[i].m_count;
	}

	printf("num distinct symbols[%d]\n", alias->m_num_symbols);
	printf("total in print freq[%d]\n", total);
}



/////////////////////////////
// Private Functions

void rescale_half(struct dictionary_internal *dictionary, char bit_representation)
{
	if (SHOULD_SCALE_HALF(dictionary->m_arithmetic.m_interval_high,dictionary->m_arithmetic.m_interval_low))
	{
		if (dictionary->m_arithmetic.m_interval_high < HALF_WAY)
		{
			dictionary->m_arithmetic.m_interval_low = dictionary->m_arithmetic.m_interval_low * 2;
			dictionary->m_arithmetic.m_interval_high = dictionary->m_arithmetic.m_interval_high * 2;
			dictionary->m_arithmetic.m_z = dictionary->m_arithmetic.m_z * 2;
		}
		else if (dictionary->m_arithmetic.m_interval_low > HALF_WAY)
		{
			dictionary->m_arithmetic.m_interval_low = 2 * (dictionary->m_arithmetic.m_interval_low - HALF_WAY);
			dictionary->m_arithmetic.m_interval_high = 2 * (dictionary->m_arithmetic.m_interval_high - HALF_WAY);
			dictionary->m_arithmetic.m_z = 2 * (dictionary->m_arithmetic.m_z - HALF_WAY);
		}

		if (bit_representation == '1')
		{
			dictionary->m_arithmetic.m_z++;
		}
	}
}

void rescale_quarter(struct dictionary_internal *dictionary, char bit_representation)
{
	if (SHOULD_SCALE_HALF(dictionary->m_arithmetic.m_interval_high,dictionary->m_arithmetic.m_interval_low) == false)
	{
		if (SHOULD_SCALE_QUARTER(dictionary->m_arithmetic.m_interval_high,dictionary->m_arithmetic.m_interval_low))
		{
			dictionary->m_arithmetic.m_interval_low = 2 * (dictionary->m_arithmetic.m_interval_low - ONE_QUARTER);
			dictionary->m_arithmetic.m_interval_high = 2 * (dictionary->m_arithmetic.m_interval_high - ONE_QUARTER);
			dictionary->m_arithmetic.m_z = 2 * (dictionary->m_arithmetic.m_z - ONE_QUARTER);

			if (bit_representation == '1')
			{
				dictionary->m_arithmetic.m_z++;
			}
		}
	}
}

int find_symbol_index(struct dictionary_internal *dictionary,struct symbol sym)
{
	int i;

//	printf("push find_symbol_index\n");
	for(i=0; i<dictionary->m_num_symbols; i++)
	{
		if (memcmp(&(dictionary->m_symbols[i].m_symbol), &sym, sizeof(struct symbol)) == 0)
		{
			break;
		}
	}

	if (i == dictionary->m_num_symbols)
	{
		i = -1;
	}
//	printf("pop find_symbol_index\n");

	return i;
}



void find_symbol(struct dictionary_internal *dictionary,struct symbol sym, struct node *current_node)
{	
//	printf("find symbol[%d][%d]\n",depth,symbol);
	if (dictionary->m_huffman.m_found == false)
	{
		dictionary->m_huffman.m_depth++;
	}

	if ((current_node->m_left == NULL && current_node->m_right == NULL) &&
		(current_node->m_symbol_info.m_symbol.m_value == sym.m_value))
	{
		dictionary->m_huffman.m_found = true;
		dictionary->m_huffman.m_depth--;
		dictionary->m_representation[dictionary->m_huffman.m_depth] = '\0';
	}
	else
	{
		if ((dictionary->m_huffman.m_found == false) && (current_node->m_left != NULL))
		{ 
			dictionary->m_representation[dictionary->m_huffman.m_depth - 1] = '1';
			find_symbol(dictionary, sym, current_node->m_left);
		}

		if ((dictionary->m_huffman.m_found == false) && (current_node->m_right != NULL))
		{ 
			dictionary->m_representation[dictionary->m_huffman.m_depth - 1] = '0';
			find_symbol(dictionary, sym, current_node->m_right);
		}
	}

	if (dictionary->m_huffman.m_found == false)
	{
		dictionary->m_huffman.m_depth--;
	}
}


int find_min_node(int num_nodes, struct node **nodes)
{
	int result = -1;
	int smallest_value = -1;
	int i;
	for (i=0; i<num_nodes; i++)
	{
		if (nodes[i] != NULL)
		{
			if (nodes[i]->m_symbol_info.m_count < smallest_value || smallest_value == -1)
			{
				smallest_value = nodes[i]->m_symbol_info.m_count;
				result = i;
			}
		}
	}
	return result;
}

void make_tree(struct dictionary_internal *dictionary)
{
	struct node **nodes = NULL;

	int num_nodes;
	num_nodes = dictionary->m_num_symbols;

	nodes = (struct node **) malloc(sizeof(struct node *) * num_nodes);

	int total = 0;

	int i;
	for (i=0; i<num_nodes; i++)
	{
		nodes[i] = (struct node *) malloc(sizeof(struct node));
		nodes[i]->m_symbol_info = dictionary->m_symbols[i];
		nodes[i]->m_left = NULL;
		nodes[i]->m_right = NULL;

		total += nodes[i]->m_symbol_info.m_count;
	}
//	printf("total in make tree[%d]\n", total);

	while (true)
	{
		int min_node_1;
		int min_node_2;

		struct node * node_1;
		int node_1_count;
		struct node * node_2;
		int node_2_count;
		struct node * additional;

		node_1 = NULL;
		node_1_count = 0;
		node_2 = NULL;
		node_2_count = 0;

		min_node_1 = find_min_node(num_nodes, nodes);
		if (min_node_1 != -1)
		{
			node_1= nodes[min_node_1];
			nodes[min_node_1] = NULL;
			node_1_count = node_1->m_symbol_info.m_count;
		}
		
		min_node_2 = find_min_node(num_nodes, nodes);
		if (min_node_2 != -1)
		{
			node_2 = nodes[min_node_2];
			nodes[min_node_2] = NULL;
			node_2_count = node_2->m_symbol_info.m_count;
		}	

		additional = (struct node *) malloc(sizeof(struct node));
		additional->m_symbol_info.m_count = node_1_count + node_2_count;
		additional->m_left = node_1;
		additional->m_right = node_2;

		if (find_min_node(num_nodes, nodes) == -1) 
		{
			dictionary->m_huffman.m_head = additional;
			break;
		}

		nodes[min_node_1] = additional;
	}

//	printf("root count[%d]\n", dictionary->m_head->m_symbol_info.m_count);
}


void free_tree(struct node *head)
{
	//TODO
}



struct node *make_node()
{
	struct node *result;

	result = (struct node *)malloc(sizeof(struct node));
	result->m_left = NULL;
	result->m_right = NULL;
	result->m_symbol_info.m_count = -1;
	result->m_symbol_info.m_symbol.m_value = 255;

	return result;
}

/*
void initialize_newick_structure(struct newick_structure *newick)
{
	newick->m_num_symbols = 0;
	newick->m_symbols = NULL;
	newick->m_num_commands = 0;
	newick->m_commands = NULL;

	newick->m_symbol_index = 0;
	newick->m_command_index = 0;
}

void add_symbol_to_newick_structure(struct newick_structure *newick,BYTE addition)
{
	newick->m_symbols = (BYTE *)realloc(newick->m_symbols,sizeof(BYTE)*(newick->m_num_symbols+1));
	newick->m_symbols[newick->m_num_symbols] = addition;
	newick->m_num_symbols++;
}

void add_command_to_newick_structure(struct newick_structure *newick,BYTE addition)
{
	newick->m_commands = (BYTE *)realloc(newick->m_commands,sizeof(BYTE)*(newick->m_num_commands+1));
	newick->m_commands[newick->m_num_commands] = addition;
	newick->m_num_commands++;
}


void print_newick_structure(struct newick_structure *newick)
{
	printf("Newick Structure:\n");
	printf("  Symbols Count[%d]  Symbols[%*s]\n",(int)newick->m_num_symbols,(int)newick->m_num_symbols,newick->m_symbols);
	printf("  Commands Count[%d]  Commands[%*s]\n",(int)newick->m_num_commands,(int)newick->m_num_commands,newick->m_commands);
}

void write_newick(FILE *fp,struct newick_structure *newick)
{
	fwrite(&(newick->m_num_symbols),sizeof(newick->m_num_symbols),1,fp);
	fwrite(newick->m_symbols,sizeof(BYTE),newick->m_num_symbols,fp);

	fwrite(&(newick->m_num_commands),sizeof(newick->m_num_commands),1,fp);
	fwrite(newick->m_commands,sizeof(BYTE),newick->m_num_commands,fp);
}

void read_newick(FILE *fp,struct newick_structure *newick)
{
	fread(&(newick->m_num_symbols),sizeof(newick->m_num_symbols),1,fp);
	newick->m_symbols = (BYTE*)malloc(sizeof(BYTE)*(newick->m_num_symbols));
	fread(newick->m_symbols,sizeof(BYTE),newick->m_num_symbols,fp);

	fread(&(newick->m_num_commands),sizeof(newick->m_num_commands),1,fp);
	newick->m_commands = (BYTE*)malloc(sizeof(BYTE)*(newick->m_num_commands));
	fread(newick->m_commands,sizeof(BYTE),newick->m_num_commands,fp);
}


BYTE read_newick_command(struct newick_structure *newick)
{
	BYTE result;

	result = newick->m_commands[newick->m_command_index];
	newick->m_command_index++;

	return result;
}

BYTE read_newick_symbol(struct newick_structure *newick)
{
	BYTE result;

	result = newick->m_symbols[newick->m_symbol_index];
	newick->m_symbol_index++;

	return result;
}


void newick_from_tree(struct node *head, struct newick_structure *newick)
{
	if (head != NULL)
	{
		if (head->m_left == NULL && head->m_right == NULL)
		{
			add_command_to_newick_structure(newick,NEWICK_SYMBOL);
			add_symbol_to_newick_structure(newick,head->m_symbol_info.m_symbol.m_value);
		}
		else
		{
			add_command_to_newick_structure(newick,NEWICK_RECURSE_LEFT);
			newick_from_tree(head->m_left, newick);
			add_command_to_newick_structure(newick,NEWICK_RECURSE_RIGHT);
			newick_from_tree(head->m_right, newick);
			add_command_to_newick_structure(newick,NEWICK_POP);
		}
	}
}


bool is_symbol(char test)
{
	bool result;


	result = true;

	if (test == '(' || test == ',' || test == ')' || test == '\0')
	{
		result = false;
	}

	return result;
}

//(((f,(i,b)),((c,s),d)),a)
struct node *tree_from_newick(struct newick_structure *newick)
{
	struct node *result;
	static BYTE s_command;

	// printf("\n");

	result = NULL;
	result = make_node();


	s_command = read_newick_command(newick);
	// printf("command: %c\n",s_command);

 
	if (s_command == NEWICK_RECURSE_LEFT)
	{
		// printf("LEFT\n");
		result->m_left = tree_from_newick(newick);
	}

	if (s_command == NEWICK_SYMBOL)
	{
		BYTE symbol;

		symbol = read_newick_symbol(newick);
		// printf("FOUND VALUE[%c]\n",symbol);
		result->m_symbol_info.m_symbol.m_value = symbol;
		s_command = read_newick_command(newick);
	}
	else
	{
		if (s_command == NEWICK_RECURSE_RIGHT)
		{
			// printf("RIGHT\n");
			result->m_right = tree_from_newick(newick);
		}

		if (s_command == NEWICK_POP)
		{
			s_command = read_newick_command(newick);
		}
	}

	// printf("POP\n");

	return result;
}

*/
