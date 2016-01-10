#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#define DWORD unsigned long long
#define EPSILON 0.00001f
#define BYTE unsigned char

/* TODO
	- define file format
	- at a minimum include:
		- dictionary (g_root)
		- number of bits?
		- bitstream
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
		printf("symbol[%c], representation[%s]\n", c, g_representation);
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











