#pragma once


#include <ofxSoylent.h>
#include <SoyPixels.h>
#include <SoyStreamBinaryFile.h>




namespace Soy264
{
	class TDecoder;
	class TVideoMeta;

	namespace TError
	{
		enum Type
		{
			Success,
			NotImplemented,
			CouldNotOpenFile,
			EndOfFile,
			NalMarkersNotFound,
			NalPacketForbiddenZeroNotZero,
			FailedToReadNalPacketHeader,
			InvalidNalPacketType,
		};

		const char*	ToString(Type Error);
	};

	class TNalPacket;
	class TNalPacketMeta;
	class TNalPacketRaw;
	class TNalPacketHeader;

	class TNalPacket_SPS;
	class TNalPacket_PPS;
	class TNalPacket_SliceNonIDR;

	namespace TNalUnitType
	{
		enum Type
		{	
			Invalid						= -1,
			Unspecified					= 0,
			Slice_NonIDRPicture			= 1,
			Slice_CodedPartitionA		= 2,
			Slice_CodedPartitionB		= 3,
			Slice_CodedPartitionC		= 4,
			Slice_CodedIDRPicture		= 5,
			SupplimentalEnhancementInformation	= 6,	//	Supplemental enhancement information
			SequenceParameterSet		= 7,
			PictureParameterSet			= 8,
			AccessUnitDelimiter			= 9,
			EndOfSequence				= 10,
			EndOfStream					= 11,
			FillerData					= 12,
			SequenceParameterSetExtension	= 13,
			Reserved14					= 14,
			Reserved15					= 15,
			Reserved16					= 16,
			Reserved17					= 17,
			Reserved18					= 18,
			Slice_AuxCodedUnpartitioned	= 19,
			Reserved20					= 20,
			Reserved21					= 21,
			Reserved22					= 22,
			Reserved23					= 23,
			Unspecified24				= 24,
			Unspecified25				= 25,
			Unspecified26				= 26,
			Unspecified27				= 27,
			Unspecified28				= 28,
			Unspecified29				= 29,
			Unspecified30				= 30,
			Unspecified31				= 31,

			Count
		};

		Type			GetType(int Value);
		const char*		ToString(Type NalUnitType);
	};


};


class TBitReader
{
public:
	TBitReader(const ArrayBridge<char>& Data) :
		mData	( Data ),
		mBitPos	( 0 )
	{
	}
	
	bool						Read(int& Data,int BitCount);

private:
	const ArrayBridge<char>&	mData;
	unsigned int				mBitPos;	//	current bit-reading-pos 
};



class Soy264::TNalPacketHeader
{
public:
	TNalPacketHeader() :
		mRefId	( -1 ),
		mType	( Soy264::TNalUnitType::Invalid )
	{
	}

	TError::Type	Init(uint8 HeaderByte);

public:
	TNalUnitType::Type		mType;
	int						mRefId;		//	there's some kinda reference ID. I think its for multiple streams in one file (stream switching)
};

class Soy264::TNalPacketMeta
{
public:
	TNalPacketMeta() :
		mPacketIndex	( -1 ),
		mFilePosition	( -1 )
	{
	}

public:
	int		mPacketIndex;
	int		mFilePosition;	//	where in the file did this packet come from?
};

class Soy264::TNalPacketRaw
{
public:
	TNalPacketRaw()
	{
	}

	TError::Type		Init(const ArrayBridge<char>& Data,TNalPacketMeta Meta);
	BufferString<100>	GetDebug();

public:
	TNalPacketHeader	mHeader;
	Array<char>			mData;		//	packet data not including start marker (0001)
	TNalPacketMeta		mMeta;	
};


class Soy264::TVideoMeta
{
public:
	TVideoMeta() :
		mWidth	( 0 ),
		mHeight	( 0 )
	{
	}

public:
	int		mWidth;
	int		mHeight;
};


class Soy264::TDecoder
{
public:
	TDecoder() :
		mPendingDataFileOffset	( 0 )
	{
	}
	TError::Type	Load(const char* Filename);
	TError::Type	DecodeNextFrame(TPixels& Pixels);

private:
	TError::Type	DecodeNextNalPacket(TNalPacketRaw& Packet);
	TNalPacket*		CreatePacket(const TNalPacketRaw& Packet);	//	create packet type from factory

public:
	TVideoMeta			mMeta;
	ofPtr<SoyStreamBinaryFile>	mStream;
	Array<char>			mPendingData;			//	data we've read, but not used
	int					mPendingDataFileOffset;	//	keep track of where we are in the file to match up data with a hex editor
};


class Soy264::TNalPacket
{
public:
	TNalPacket(const TNalPacketRaw& PacketRaw) :
		mHeader	( PacketRaw.mHeader ),
		mMeta	( PacketRaw.mMeta )
	{
	}
	virtual ~TNalPacket()	{}

public:
	TNalPacketMeta		mMeta;
	TNalPacketHeader	mHeader;
};



class Soy264::TNalPacket_SPS : public TNalPacket
{
public:
	TNalPacket_SPS(const TNalPacketRaw& PacketRaw);
};



class Soy264::TNalPacket_PPS : public TNalPacket
{
public:
	TNalPacket_PPS(const TNalPacketRaw& PacketRaw);
};



class Soy264::TNalPacket_SliceNonIDR : public TNalPacket
{
public:
	TNalPacket_SliceNonIDR(const TNalPacketRaw& PacketRaw);
};

