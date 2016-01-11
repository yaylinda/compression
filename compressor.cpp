#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>



#define MAGIC_NUMBER 0xC0EDBABE
#define VERSION 1
#define ALGORITHM_HUFFMAN 1


#define DWORD unsigned long long
#define WORD unsigned int
#define EPSILON 0.00001f
#define BYTE unsigned char

#define NEWICK_RECURSE_LEFT 97
#define NEWICK_RECURSE_RIGHT 98
#define NEWICK_POP 99
#define NEWICK_SYMBOL 100

/* TODO
	- definition of compressed file format
		- magic number: 1 DWORD
		- version number: 1 WORD
		- algorithm: 1 BYTE
		- algorithm data:
			- symbol length in bits: 1 WORD
			- symbol count: 1 DWORD
			- dictionary
				- newick command string lendgth:  1 DWORD
				- newick command string "((A, (C, D)), B)"
			- number of remainder bits at last BYTE: 2 bits
			- bitstream
		- crc: 1 DWORD
*/

/////////////////////////////
// Private Structures
struct symbol
{
	BYTE m_value;
};

struct symbol_info
{
	int m_count;
	struct symbol m_symbol;
};


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

struct huffman_format
{
	struct newick_structure m_newick;
	BYTE m_remainder_bits;
	BYTE *m_bytestream;
};

struct compressed_file_format
{
	DWORD m_magic_number;
	WORD m_version_number;
	BYTE m_algorithm_id;
	struct huffman_format m_data;
	DWORD crc;
};

/////////////////////////////
// Global Variables
static int g_num_symbols = 0;
static struct symbol_info *g_symbols = NULL;
static struct node *g_root = NULL;
static int g_representation_length = 0;
static char *g_representation = NULL;
static FILE *g_output = NULL;
static int total_bits = 0;
static BYTE g_bitstring = 0;
static int g_bit_index = 7;

/////////////////////////////
// Private Prototypes
bool open_files(const char *source_filename, const char *dest_filename, FILE **source, FILE **dest);

int update_frequency_table(const BYTE *source_buffer, int max_size, int process_size);
int compress_buffer(const BYTE *source_buffer, int max_size, int process_size);
bool process_file(FILE *source, int (*lambda)(const BYTE *, int, int));

DWORD get_file_size(FILE *source);
void print_frequency_table();
void make_tree();

void initialize_newick_structure(struct newick_structure *newick);
void print_newick_structure(struct newick_structure *newick);
void write_newick(FILE *fp,struct newick_structure *newick);
void read_newick(FILE *fp,struct newick_structure *newick);

void newick_from_tree(struct node *head, struct newick_structure *newick);
struct node *tree_from_newick(struct newick_structure *newick);



/////////////////////////////
// Public Functions
int main(int argc, char *argv[])
{
	int result = 20;

	if (argc != 3) 
	{
		printf("Usage: test source-filename dest-filename.\n");
		result = 0;
	} 
	else
	{
		FILE *source;
		bool open_files_test;
		open_files_test = open_files(argv[1], argv[2], &source, &g_output);
		if (open_files_test) 
		{
			bool process_file_test;
			process_file_test = process_file(source, update_frequency_table);

			if (process_file_test)
			{
				print_frequency_table();
				g_representation_length = g_num_symbols;
				g_representation = (char *) malloc(sizeof(char)*g_representation_length);
				memset(g_representation, 0, sizeof(char)*g_representation_length);
				make_tree();

				//process the input file to the output file
				struct compressed_file_format meta;
				meta.m_magic_number = MAGIC_NUMBER;
				meta.m_version_number = VERSION;
				meta.m_algorithm_id = ALGORITHM_HUFFMAN;

				struct huffman_format huffman;
				BYTE m_remainder_bits;
				BYTE *m_bytestream;

				//dictionary representation
				{
					initialize_newick_structure(&huffman.m_newick);

					newick_from_tree(g_root,&huffman.m_newick);
					print_newick_structure(&huffman.m_newick);
				}

				fwrite(&meta.m_magic_number,sizeof(meta.m_magic_number),1,g_output);
				fwrite(&meta.m_version_number,sizeof(meta.m_version_number),1,g_output);
				fwrite(&meta.m_algorithm_id,sizeof(meta.m_algorithm_id),1,g_output);

				write_newick(g_output,&huffman.m_newick);

				process_file(source, compress_buffer);
				if (g_bit_index != 7)
				{
					fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
				}
				printf("total_bits[%d]\n", total_bits);
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
			result = 1;
		}
	} 

	return result;
}

/////////////////////////////
// Private Functions
bool open_files(const char* source_filename, const char* dest_filename, FILE **source, FILE **dest)
{
	// read file
	bool result = false;

	*source = fopen(source_filename, "rb");
	if (*source != NULL) 
	{
		*dest = fopen(dest_filename, "wb");
		if (*dest != NULL)
		{
			result = true;
		}
		else
		{
			printf("Could not open destination file for writing.\n");
		}

	} 
	else
	{
		printf("Could not open source file for reading.\n");
	}

	return result;
}

int update_frequency_table(const BYTE *source_buffer, int max_size, int process_size)
{
	int i;
	for (i=0; i<process_size; i++)
	{
		BYTE c = source_buffer[i];
		int j;
		for (j=0; j<g_num_symbols; j++)
		{
			if (g_symbols[j].m_symbol.m_value == c) 
			{
				g_symbols[j].m_count++;
				break;
			}
		}

		if (j == g_num_symbols)
		{
			g_num_symbols++;
			g_symbols = (struct symbol_info *) realloc(g_symbols, sizeof(struct symbol_info)*g_num_symbols);
			g_symbols[g_num_symbols-1].m_count = 1;
			g_symbols[g_num_symbols-1].m_symbol.m_value = c; 
		}
	}

	return -1;
}

static bool found;
static int depth;

void find_symbol(BYTE symbol, struct node *current_node)
{	
	if (found == false)
	{
		depth++;
	}
	if (current_node->m_symbol_info.m_symbol.m_value == symbol)
	{
		found = true;
		g_representation[depth-1] = '\0';
	}
	else
	{
		if ((found == false) && (current_node->m_left != NULL))
		{ 
			g_representation[depth-1] = '1';
			find_symbol(symbol, current_node->m_left);
		}

		if ((found == false) && (current_node->m_right != NULL))
		{ 
			g_representation[depth-1] = '0';
			find_symbol(symbol, current_node->m_right);
		}
	}
	if (found == false)
	{
		depth--;
	}
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
		fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
		g_bitstring = 0;
		g_bit_index = 7;
	}
}

int compress_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	int i;
	for (i=0; i<process_size; i++)
	{
		BYTE c = source_buffer[i];
		int j;
		found = false;
		depth = 0;
		find_symbol(c, g_root);
		for (j=0; j<depth; j++)
		{
			write_bit(g_representation[j]);
		}
		// printf("symbol[%c], representation[%s]\n", c, g_representation);
		total_bits += depth;
	}	
	return -1;
}

bool process_file(FILE *source, int (*lambda)(const BYTE *, int, int))
{
	bool result = true;

	float previous_percentile = 0.0f;
	float diff_percentile = 0.0f;
	const float increment = 0.05f;

	DWORD source_size;
	source_size = get_file_size(source);

	BYTE source_buffer[2048];

	DWORD amount_left;
	amount_left = source_size;

	fseek(source, 0, SEEK_SET);

	printf("[");

	while (amount_left > 0)
	{
		int amount_read;
		int amount_written;
		amount_read = fread(source_buffer, sizeof(source_buffer[0]), sizeof(source_buffer), (FILE*)source);
	
		int amount_processed;
		amount_processed = lambda(source_buffer, sizeof(source_buffer), amount_read);
		
		amount_left -= amount_read;
		
		float current_percentile;

		// printf("%s %d\n", "amount_read", amount_read);
		current_percentile = (float)(source_size - amount_left) / source_size;
		// printf("%s %f\n", "current_percentile", current_percentile);
		diff_percentile += current_percentile - previous_percentile;
		// printf("%s %f\n", "diff_percentile", diff_percentile);
		
		bool flag = false;
		while ((diff_percentile + EPSILON) > increment)
		{
			printf("-");
			diff_percentile -= increment;
			flag = true;
		}

		if (flag) 
		{
			previous_percentile = current_percentile;
		}

		if ((amount_read == 0) && (amount_left != 0))
		{
			result = false;
			break;
		}
	}

	printf("]\n");

	return result;
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

void make_tree()
{
	struct node **nodes = NULL;

	int num_nodes;
	num_nodes = g_num_symbols;

	nodes = (struct node **) malloc(sizeof(struct node *) * num_nodes);

	int total = 0;

	int i;
	for (i=0; i<num_nodes; i++)
	{
		nodes[i] = (struct node *) malloc(sizeof(struct node));
		nodes[i]->m_symbol_info = g_symbols[i];
		nodes[i]->m_left = NULL;
		nodes[i]->m_right = NULL;

		total += nodes[i]->m_symbol_info.m_count;
	}
	printf("total in make tree[%d]\n", total);

	while (true)
	{
		int min_node_1;
		int min_node_2;

		struct node * node_1;
		struct node * node_2;
		struct node * additional;

		min_node_1 = find_min_node(num_nodes, nodes);
		node_1 = nodes[min_node_1];
		nodes[min_node_1] = NULL;
		min_node_2 = find_min_node(num_nodes, nodes);
		node_2 = nodes[min_node_2];
		nodes[min_node_2] = NULL;

		additional = (struct node *) malloc(sizeof(struct node));
		additional->m_symbol_info.m_count = node_1->m_symbol_info.m_count + node_2->m_symbol_info.m_count;
		additional->m_left = node_1;
		additional->m_right = node_2;

		if (find_min_node(num_nodes, nodes) == -1) 
		{
			g_root = additional;
			break;
		}

		nodes[min_node_1] = additional;
	}

	printf("root count[%d]\n", g_root->m_symbol_info.m_count);
}




void initialize_newick_structure(struct newick_structure *newick)
{
	newick->m_num_symbols = 0;
	newick->m_symbols = NULL;
	newick->m_num_commands = 0;
	newick->m_commands = NULL;

	newick->m_symbol_index = -1;
	newick->m_command_index = -1;
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
	fread(newick->m_symbols,sizeof(BYTE),newick->m_num_symbols,fp);

	fread(&(newick->m_num_commands),sizeof(newick->m_num_commands),1,fp);
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
	BYTE command;

//	printf("\n");

	result = NULL;
	result = make_node();

//	printf("tree: %c\n",**newick_string);

	command = read_newick_command(newick);
 
	if (command == NEWICK_RECURSE_LEFT)
	{
//		printf("LEFT\n");
		result->m_left = tree_from_newick(newick);
	}

	if (command == NEWICK_SYMBOL)
	{
		BYTE symbol;

		symbol = read_newick_symbol(newick);
//		printf("FOUND VALUE[%c]\n",**newick_string);
		result->m_symbol_info.m_symbol.m_value = symbol;
	}
	else
	{
		if (command == NEWICK_RECURSE_RIGHT)
		{
			result->m_right = tree_from_newick(newick);
		}

		if (command == NEWICK_POP)
		{
//			(*newick_string)++;
		}
	}

//	printf("POP\n");

	return result;
}

/*
struct node *tree_from_newick_recurse(struct newick_structure *newick)
{
	struct node *result;

//	printf("\n");

	result = NULL;
	result = make_node();

//	printf("tree: %c\n",**newick_string);

 
	if (**newick_string == '(')
	{
		(*newick_string)++;

//		printf("LEFT\n");
		result->m_left = tree_from_newick_recurse(newick_string);
	}

	if (is_symbol(**newick_string))
	{
//		printf("FOUND VALUE[%c]\n",**newick_string);
		result->m_symbol_info.m_symbol.m_value = **newick_string;

		(*newick_string)++;		
	}
	else
	{
		if (**newick_string == ',')
		{
			(*newick_string)++;
//			printf("RIGHT\n");
			result->m_right = tree_from_newick_recurse(newick_string);
		}

		if (**newick_string == ')')
		{
			(*newick_string)++;
		}
	}

//	printf("POP\n");

	return result;
}


*/

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

void print_frequency_table()
{
	int total;
	total = 0;
	int i;
	for (i=0; i<g_num_symbols; i++)
	{
		printf("symbol[%d], frequency[%d]\n", g_symbols[i].m_symbol.m_value, g_symbols[i].m_count);
		total += g_symbols[i].m_count;
	}

	printf("num distinct symbols[%d]\n", g_num_symbols);
	printf("total in print freq[%d]\n", total);
}











