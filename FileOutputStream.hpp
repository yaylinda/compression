#ifndef FILE_OUTPUT_STREAM__HPP
#define FILE_OUTPUT_STREAM__HPP

#include "OutputStream.hpp"


class FileOutputStream : public OutputStream
{
	public:

		FileOutputStream();
		~FileOutputStream();


		virtual bool initialize(const char *fileName);
		virtual void shutdown();

		virtual int tell();
		virtual bool seek(int delta,SEEK_MODE mode);
		virtual int write(void *buffer,int size,int count);		


	private:

		void *mOpaque;

};


#endif // FILE_OUTPUT_STREAM__HPP