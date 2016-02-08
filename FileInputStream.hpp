#ifndef FILE_INPUT_STREAM__HPP
#define FILE_INPUT_STREAM__HPP

#include "InputStream.hpp"




class FileInputStream : public InputStream
{
	public:
		FileInputStream();
		virtual ~FileInputStream();

		virtual bool initialize(const char *fileName);
		virtual void shutdown();

		virtual int tell();
		virtual bool seek(int delta,SEEK_MODE mode);
		virtual int read(void *buffer,int size,int count);


	private:


		void *mOpaque;



};

#endif // FILE_INPUT_STREAM__HPP