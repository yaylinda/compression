#include "FileInputStream.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/////////////////
//private structs
struct PrivateFileInputStreamData
{
	FILE *mFP;
	const char *mFileName;
};




///////////////////
//public methods
FileInputStream::FileInputStream()
{
	mOpaque = NULL;
}


FileInputStream::~FileInputStream()
{
	mOpaque = NULL;
}


bool FileInputStream::initialize(const char *fileName)
{
	bool result;

	result = false;

	if (fileName != NULL)
	{
		FILE *fp;

		fp = fopen(fileName,"rb");


		if (fp != NULL)
		{
			struct PrivateFileInputStreamData *opaque;

			opaque = (struct PrivateFileInputStreamData *)malloc(sizeof(struct PrivateFileInputStreamData));
			opaque->mFileName = strdup(fileName);
			opaque->mFP = fp;

			mOpaque = (void *)opaque;

			result = true;
		}
		else
		{
			printf("Problem opening file [%s].\n", fileName);
		}
	}

	return result;
}


void FileInputStream::shutdown()
{
	if (mOpaque != NULL)
	{
		struct PrivateFileInputStreamData *alias;

		alias = (struct PrivateFileInputStreamData *)mOpaque;

		fclose(alias->mFP);
		alias->mFP = NULL;
		free((void*)alias->mFileName);

		free(mOpaque);
		mOpaque = NULL;
	}
}




int FileInputStream::tell()
{
	int result;

	result = -1;

	if (mOpaque != NULL)
	{
		struct PrivateFileInputStreamData *alias;

		alias = (struct PrivateFileInputStreamData *)mOpaque;

		result = ftell(alias->mFP);
	}

	return result;
}


bool FileInputStream::seek(int delta,SEEK_MODE mode)
{
	bool result;

	result = false;

	if (mOpaque != NULL)
	{
		struct PrivateFileInputStreamData *alias;
		int seek_definition;

		alias = (struct PrivateFileInputStreamData *)mOpaque;

		switch (mode)
		{
			case SEEK_BEGINNING :
				seek_definition = SEEK_SET;
				break;
			case SEEK_CURRENT :
				seek_definition = SEEK_CUR;
				break;
			case SEEK_ENDING :
				seek_definition = SEEK_END;
				break;
			default:
				assert(!"huh??  FileInputStream::seek\n");
				break;
		}


		result = fseek(alias->mFP,delta,seek_definition);
	}

	return result;
}


int FileInputStream::read(void *buffer,int size,int count)
{
	int result;

	result = -1;

	if (mOpaque != NULL)
	{
		struct PrivateFileInputStreamData *alias;

		alias = (struct PrivateFileInputStreamData *)mOpaque;

		result = fread(buffer,size,count,alias->mFP);
	}

	return result;
}



////////////////////////
//private methods
