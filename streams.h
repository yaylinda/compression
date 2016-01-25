#ifndef STREAMS__H
#define STREAMS__H


struct encoding_stream
{
	void (*initialize)(int symbol_size,bool encode);

	void (*encoding_write_symbols)(const BYTE *source,int symbol_count);
	void (*encoding_read_bytes)(BYTE *dest,int *byte_count,int buffer_size);

	bool (*flush)();
	bool (*finish)();
};


struct decoding_stream
{
	void (*initialize)(int symbol_size,bool encode);

	void (*decoding_write_bytes)(const BYTE *source,int byte_count);
	void (*decoding_read_symbols)(BYTE *dest,int *symbol_count,int buffer_size);

	bool (*flush)();
	bool (*finish)();
};




#endif // STREAMS__H