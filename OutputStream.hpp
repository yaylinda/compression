#ifndef OUTPUT_STEAM__HPP
#define OUTPUT_STEAM__HPP


#ifndef COMMON__H
#	include "common.h"
#endif



class OutputStream
{
	public:

		virtual ~OutputStream();

		virtual int tell() = 0;
		virtual bool seek(int delta,SEEK_MODE mode) = 0;
		virtual int write(void *buffer,int size,int count) = 0;		
	
	private:

};

#endif // OUTPUT_STEAM__HPP