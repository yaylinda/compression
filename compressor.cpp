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
	BYTE m_number_of_remainder_bits;
	BYTE *m_bytestream;
};

struct compressed_file_format
{
	DWORD m_magic_number;
	WORD m_version_number;
	BYTE m_algorithm_id;
	struct huffman_format m_huffman;
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
static struct compressed_file_format g_meta;
static int g_remainder_bits_position_within_source_buffer = 0;

/////////////////////////////
// Private Prototypes
bool open_files(const char *source_filename, const char *dest_filename, FILE **source, FILE **dest);

int update_frequency_table(const BYTE *source_buffer, int max_size, int process_size);
int compress_buffer(const BYTE *source_buffer, int max_size, int process_size);
int decompress_buffer(const BYTE *source_buffer, int max_size, int process_size);

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

	if (argc != 4) 
	{
		printf("Usage: compressor OPTION source-filename dest-filename.\n");
		printf("OPTION = c -> compress; OPTION = d -> decompress\n");
		result = 0;
	} 
	else
	{
		FILE *source;
		bool open_files_test;
		open_files_test = open_files(argv[2], argv[3], &source, &g_output);

		if (open_files_test) 
		{
			bool compress = (argv[1][0] == 'c' || argv[1][0] == 'C');
			if (compress)
			{
				bool process_file_test;

				fseek(source, 0, SEEK_SET);
				process_file_test = process_file(source, update_frequency_table);

				if (process_file_test)
				{
				//	print_frequency_table();
					g_representation_length = g_num_symbols;
					g_representation = (char *) malloc(sizeof(char)*g_representation_length);
					memset(g_representation, 0, sizeof(char)*g_representation_length);
					make_tree();

					
					//process the input file to the output file
					g_meta.m_magic_number = MAGIC_NUMBER;
					g_meta.m_version_number = VERSION;
					g_meta.m_algorithm_id = ALGORITHM_HUFFMAN;
					

					//dictionary representation
					{
						initialize_newick_structure(&g_meta.m_huffman.m_newick);

						newick_from_tree(g_root,&g_meta.m_huffman.m_newick);
			//			print_newick_structure(&g_meta.m_huffman.m_newick);
					}

					fwrite(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1,g_output);
					fwrite(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1,g_output);
					fwrite(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1,g_output);

					write_newick(g_output,&g_meta.m_huffman.m_newick);

					int remainder_bits_position;
					remainder_bits_position = ftell(g_output);


					//write some dummy data to acount for what could be
					g_meta.m_huffman.m_number_of_remainder_bits = 0;
					fwrite(&g_meta.m_huffman.m_number_of_remainder_bits, sizeof(BYTE), 1, g_output);


					fseek(source, 0, SEEK_SET);
					process_file(source, compress_buffer);


					if (g_bit_index != 7)
					{
						g_meta.m_huffman.m_number_of_remainder_bits = 7 - g_bit_index;

						if (g_meta.m_huffman.m_number_of_remainder_bits > 0)
						{
							fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
						}

						fseek(g_output, remainder_bits_position, SEEK_SET);
						fwrite(&g_meta.m_huffman.m_number_of_remainder_bits, sizeof(BYTE), 1, g_output);
		//				printf("number of remainder bits written[%d]\n", g_meta.m_huffman.m_number_of_remainder_bits);
						fseek(g_output, 0, SEEK_END);
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
				fread(&g_meta.m_magic_number,sizeof(g_meta.m_magic_number),1,source);
				assert(g_meta.m_magic_number == MAGIC_NUMBER);
				fread(&g_meta.m_version_number,sizeof(g_meta.m_version_number),1,source);
				assert(g_meta.m_version_number == VERSION);
				fread(&g_meta.m_algorithm_id,sizeof(g_meta.m_algorithm_id),1,source);
				assert(g_meta.m_algorithm_id == ALGORITHM_HUFFMAN);

				read_newick(source,&g_meta.m_huffman.m_newick);

		//		print_newick_structure(&g_meta.m_huffman.m_newick);


				fread(&g_meta.m_huffman.m_number_of_remainder_bits, sizeof(g_meta.m_huffman.m_number_of_remainder_bits), 1, source);
	//			printf("number of remainder bits written[%d]\n", g_meta.m_huffman.m_number_of_remainder_bits);

				g_root = tree_from_newick(&g_meta.m_huffman.m_newick);

				{
					int cur;
					cur = ftell(source);
					fseek(source,0,SEEK_END);
					g_remainder_bits_position_within_source_buffer = ftell(source) - cur;
					fseek(source,cur,SEEK_SET);
				}

				process_file(source, decompress_buffer);

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
			g_symbols = (struct symbol_info *)realloc(g_symbols, sizeof(struct symbol_info)*g_num_symbols);
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
		depth--;
		g_representation[depth] = '\0';
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
//		printf("g_bitstring written[%d]\n", g_bitstring);
		fwrite(&g_bitstring, sizeof(BYTE), 1, g_output);
		g_bitstring = 0;
		g_bit_index = 7;
	}
}

bool decode_symbol(char c,BYTE *decoded_symbol)
{
	static char s_representation[100] = {0};
	static int s_fuckers = 0;
	static struct node *s_cursor = g_root;
	bool result;

	result = false;

	s_representation[s_fuckers++] = c;

	if (c == '1')
	{
		s_cursor = s_cursor->m_left;
	}
	else
	{
		if (c == '0')
		{
			s_cursor = s_cursor->m_right;
		}
	}

	if (s_cursor->m_left == NULL && s_cursor->m_right == NULL)
	{
		s_representation[s_fuckers] = '\0';
//		printf("rep[%s] fuck[%d]\n",s_representation,s_fuckers);
		s_fuckers = 0;
		s_representation[0] = '\0';
		result = true;
		*decoded_symbol = s_cursor->m_symbol_info.m_symbol.m_value;
		s_cursor = g_root;
	}

	return result;
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
//		 printf("symbol[%c], representation[%s] depth[%d]\n", c, g_representation,depth);
		for (j=0; j<depth; j++)
		{
			write_bit(g_representation[j]);
		}
		total_bits += depth;
	}	
	return -1;
}


int decompress_buffer(const BYTE *source_buffer, int max_size, int process_size)
{
	static int s_current_processed_total = 0;
	int i;
	BYTE decoded_symbol;

//	printf("decompress buffer\n");

	for (i = 0;i < process_size;i++)
	{
		BYTE cur_byte;
		int j;
		s_current_processed_total++;

		cur_byte = source_buffer[i];
	//	printf("cur_byte[%d]\n", cur_byte);

		//magic linda code here..
		//..need to know if the byte we're deoding is the last byte in the file.
		//... If it IS, then we need to make sure we respect ONLY the meta.huffman.remainder_bits worth of that last byte.
		// (the subsequent bits are garbage)

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

			test = decode_symbol(bit,&decoded_symbol);

			if (test == true)
			{
				fwrite(&decoded_symbol,sizeof(decoded_symbol),1,g_output);
			}

			if (s_current_processed_total == g_remainder_bits_position_within_source_buffer)
			{
				if (j + g_meta.m_huffman.m_number_of_remainder_bits == 8)
				{

	//				printf("[%d][%d][%d]found our last byte and the last legitimate bit\n",j,s_current_processed_total,g_remainder_bits_position_within_source_buffer);
					break;
				}
			}
		}
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

	int pos;
	pos = ftell(source);
//	printf("Processing File from [%d]\n",pos);

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
			g_root = additional;
			break;
		}

		nodes[min_node_1] = additional;
	}

//	printf("root count[%d]\n", g_root->m_symbol_info.m_count);
}




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











