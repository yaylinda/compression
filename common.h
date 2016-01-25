#ifndef COMMON__H
#define COMMON__H

#define MAGIC_NUMBER 0xC0EDBABE
#define VERSION 1
#define ALGORITHM_HUFFMAN 1
#define ALGORITHM_ARITHMETIC 2
#define EPSILON 0.0001f


#define DWORD unsigned long long
#define WORD unsigned int
#define BYTE unsigned char


struct symbol
{
	BYTE m_value;
};

struct symbol_info
{
	int m_count;
	struct symbol m_symbol;
};




#endif // COMMON__H