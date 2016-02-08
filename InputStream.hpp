#ifndef INPUT_STREAM__HPP
#define INPUT_STREAM__HPP

#ifndef COMMON__H
#	include "common.h"
#endif


class InputStream
{
	public:

		virtual ~InputStream();

		virtual int tell() = 0;
		virtual bool seek(int delta,SEEK_MODE mode) = 0;
		virtual int read(void *buffer, int size,int count) = 0;		

	private:
};


#endif // INPUT_STREAMS__HPP