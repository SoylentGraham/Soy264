#include "SoyStreamBinaryFile.h"



SoyStreamBinaryFile::SoyStreamBinaryFile(const char* Filename) :
	mFile	( nullptr )
{
	auto Error = fopen_s( &mFile, Filename, "rb" );
}

SoyStreamBinaryFile::~SoyStreamBinaryFile()
{
	if ( mFile )
	{
		fclose( mFile );
		mFile = nullptr;
	}
}

bool SoyStreamBinaryFile::IsValid() const
{
	return mFile != nullptr;
}

bool SoyStreamBinaryFile::Read(ArrayBridge<char>& Data)
{
	if ( !IsValid() )
		return false;

	//	read a chunk
	int Block = ofMin( 1024*10, Data.MaxSize() );
	if ( !Data.SetSize( Block ) )
		return false;

	auto BytesRead = fread_s( Data.GetArray(), Data.GetDataSize(), Data.GetElementSize(), Data.GetSize(), mFile );
	Data.SetSize( BytesRead );

	//	todo: distinguish between EOF and error?
	if ( BytesRead == 0 )
		return false;

	return true;
}

