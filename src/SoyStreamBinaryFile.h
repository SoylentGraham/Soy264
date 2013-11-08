#pragma once


#include <ofxSoylent.h>


class SoyStreamBinaryFile
{
public:
	SoyStreamBinaryFile(const char* Filename);
	~SoyStreamBinaryFile();

	bool		IsValid() const;
	bool		Read(ArrayBridge<char>& Data);

private:
	FILE*		mFile;
};

