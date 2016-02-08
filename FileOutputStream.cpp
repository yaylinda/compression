#include "FileOutputStream.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>




/////////////////
//private structs
struct PrivateFileOutputStreamData
{
	FILE *mFP;
	const char *mFileName;
};


////////////////////
//public methods

//virtual
FileOutputStream::FileOutputStream()
{
	mOpaque = NULL;
}


//virtual
FileOutputStream::~FileOutputStream()
{
	assert(mOpaque == NULL);
}




//virtual
bool FileOutputStream::initialize(const char *fileName)
{
	bool result;

	result = false;

	if (fileName != NULL)
	{
		FILE *fp;

		fp = fopen(fileName,"wb");


		if (fp != NULL)
		{
			struct PrivateFileOutputStreamData *opaque;

			opaque = (struct PrivateFileOutputStreamData *)malloc(sizeof(struct PrivateFileOutputStreamData));
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


//virtual
void FileOutputStream::shutdown()
{
	if (mOpaque != NULL)
	{
		struct PrivateFileOutputStreamData *alias;

		alias = (struct PrivateFileOutputStreamData *)mOpaque;

		fclose(alias->mFP);
		alias->mFP = NULL;
		free((void*)alias->mFileName);

		free(mOpaque);
		mOpaque = NULL;
	}

}

//virtual
int FileOutputStream::tell()
{
	int result;

	result = -1;

	if (mOpaque != NULL)
	{
		struct PrivateFileOutputStreamData *alias;

		alias = (struct PrivateFileOutputStreamData *)mOpaque;

		result = ftell(alias->mFP);
	}

	return result;
}


//virtual
bool FileOutputStream::seek(int delta,SEEK_MODE mode)
{
	bool result;

	result = false;

	if (mOpaque != NULL)
	{
		struct PrivateFileOutputStreamData *alias;
		int seek_definition;

		alias = (struct PrivateFileOutputStreamData *)mOpaque;

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
				assert(!"huh??  FileOutputStream::seek\n");
				break;
		}


		result = fseek(alias->mFP,delta,seek_definition);
	}

	return result;}


//virtual
int FileOutputStream::write(void *buffer,int size,int count)
{
	int result;

	result = -1;

	if (mOpaque != NULL)
	{
		struct PrivateFileOutputStreamData *alias;

		alias = (struct PrivateFileOutputStreamData *)mOpaque;

		result = fwrite(buffer,size,count,alias->mFP);
	}

	return result;
}





////////////////////
//private methods

