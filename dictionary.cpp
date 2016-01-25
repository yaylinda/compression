#include "./dictionary.h"

#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

/////////////////////////////
// private defines

#define MAX_REPRESENTATION_LENGTH 2048 // the max number of bits an encoded symbol can be

#define NEWICK_RECURSE_LEFT 97
#define NEWICK_RECURSE_RIGHT 98
#define NEWICK_POP 99
#define NEWICK_SYMBOL 100


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


struct dictionary_internal
{
	int m_num_symbols;
	struct symbol_info *m_symbols;

	struct node *m_head;
	struct node *m_decode_cursor;

	bool m_found;
	int m_depth;
	int m_representation_length;
	char m_representation[MAX_REPRESENTATION_LENGTH]; // literally the characters '0' or '1' to represent a bit string
};



/////////////////////////////
// Global Variables


/////////////////////////////
// Private Prototypes
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

DICTIONARY create_dictionary()
{
	struct dictionary_internal *addition;

	addition = (struct dictionary_internal *)malloc(sizeof(struct dictionary_internal));

	addition->m_num_symbols = 0;
	addition->m_representation_length = 0;

	addition->m_symbols = NULL;
	addition->m_head = NULL;

	return (DICTIONARY)addition;
}

void destroy_dictonary(DICTIONARY dictionary)
{
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)dictionary;

	free(alias->m_symbols);
	alias->m_symbols = NULL;

	free_tree(alias->m_head);
	alias->m_head = NULL;
	alias->m_decode_cursor = NULL;

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

	make_tree(alias);

	if (alias->m_head == NULL)
	{
		result = false;
	}
	else
	{
		alias->m_decode_cursor = alias->m_head;
		result = true;
	}

	return result;
}




void serialize_dictionary_to_bytes(DICTIONARY dictionary,int *num_bytes,BYTE **bytes)
{
	struct dictionary_internal *alias;

	alias = (struct dictionary_internal *)dictionary;

	*num_bytes = sizeof(int) + (alias->m_num_symbols * sizeof(struct symbol_info));
	*bytes = (BYTE *)malloc(sizeof(BYTE) * *num_bytes);

	memcpy(*bytes,&(alias->m_num_symbols),sizeof(int));
	memcpy(&((*bytes)[sizeof(int)]),alias->m_symbols,(alias->m_num_symbols * sizeof(struct symbol_info)));
}


DICTIONARY deserialize_bytes_to_dictionary(int num_bytes,BYTE *bytes)
{
	DICTIONARY result;
	struct dictionary_internal *alias;

	result = create_dictionary();
	alias = (struct dictionary_internal *)result;

	memcpy(&(alias->m_num_symbols),bytes,sizeof(int));
	alias->m_symbols = (struct symbol_info *)malloc(sizeof(struct symbol_info) * alias->m_num_symbols);
	memcpy(alias->m_symbols,&(bytes[sizeof(int)]),sizeof(struct symbol_info) * alias->m_num_symbols);

	bool test;

	test = finalize_dictionary(result);
	if (test == false)
	{
		result = NULL;
	}

	return result;
}



const char *encode_symbol_to_bitstring(DICTIONARY dictionary,struct symbol sym)
{
	char *result;
	struct dictionary_internal *alias;

	result = NULL;
	alias = (struct dictionary_internal *)dictionary;

	alias->m_found = false;
	alias->m_depth = 0;

	find_symbol(alias,sym,alias->m_head);

	if (alias->m_found == true)
	{
		result = alias->m_representation;
	}

	return result;
}



bool decode_consume_bit(DICTIONARY dictionary,char bit_representation, struct symbol *decoded_symbol)
{
	struct dictionary_internal *alias;
	bool result;

	result = false;

	alias = (struct dictionary_internal *)dictionary;


	if (bit_representation == '1')
	{
		alias->m_decode_cursor = alias->m_decode_cursor->m_left;
	}
	else
	{
		if (bit_representation == '0')
		{
			alias->m_decode_cursor = alias->m_decode_cursor->m_right;
		}
	}

	if (alias->m_decode_cursor->m_left == NULL && alias->m_decode_cursor->m_right == NULL)
	{
		result = true;
		*decoded_symbol = alias->m_decode_cursor->m_symbol_info.m_symbol;
		alias->m_decode_cursor = alias->m_head; // reset the cursor for the next round
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



void find_symbol(struct dictionary_internal *dictionary,struct symbol sym, struct node *current_node)
{	
//	printf("find symbol[%d][%d]\n",depth,symbol);
	if (dictionary->m_found == false)
	{
		dictionary->m_depth++;
	}

	if ((current_node->m_left == NULL && current_node->m_right == NULL) &&
		(current_node->m_symbol_info.m_symbol.m_value == sym.m_value))
	{
		dictionary->m_found = true;
		dictionary->m_depth--;
		dictionary->m_representation[dictionary->m_depth] = '\0';
	}
	else
	{
		if ((dictionary->m_found == false) && (current_node->m_left != NULL))
		{ 
			dictionary->m_representation[dictionary->m_depth - 1] = '1';
			find_symbol(dictionary, sym, current_node->m_left);
		}

		if ((dictionary->m_found == false) && (current_node->m_right != NULL))
		{ 
			dictionary->m_representation[dictionary->m_depth - 1] = '0';
			find_symbol(dictionary, sym, current_node->m_right);
		}
	}

	if (dictionary->m_found == false)
	{
		dictionary->m_depth--;
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
			dictionary->m_head = additional;
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
