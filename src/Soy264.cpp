#include "Soy264.h"
#include <SoyDebug.h>




const char*	Soy264::TError::ToString(TError::Type Error)
{
	switch ( Error )
	{
	case Success:				return "Success";
	case NotImplemented:		return "Not implemented";
	case CouldNotOpenFile:		return "Could not open file";
	case EndOfFile:				return "End of file";
	case NalMarkersNotFound:	return "Nal markers not found";
	default:					return "Unknown error type";
	}
}


Soy264::TError::Type Soy264::TDecoder::Load(const char* Filename)
{
	mStream = ofPtr<SoyStreamBinaryFile>( new SoyStreamBinaryFile( Filename ) );
	if ( !mStream->IsValid() )
	{
		mStream.reset();
		return TError::CouldNotOpenFile;
	}
	
	return TError::Success;
}

Soy264::TError::Type Soy264::TDecoder::DecodeNextFrame(TPixels& Pixels)
{
	//	read nal packets until we have enough to make a frame
	Array<TNalPacket> NalPackets;

	while ( true )
	{
		TNalPacket Packet;
		auto Error = DecodeNextNalPacket( Packet );
		if ( Error != TError::Success )
			return Error;

		//	log the packet read
		BufferString<100> Debug;
		Debug << "Read packet " << NalPackets.GetSize() << " " << Packet.GetDebug();
		ofLogNotice( Debug.c_str() );

		NalPackets.PushBack( Packet );

		//	do we have enough packets for a frame?
	}

	return TError::NotImplemented;
}

template<int PATTERNSIZE>
int FindPatternStart(ArrayBridge<char>& Data,const BufferArray<char,PATTERNSIZE>& Pattern,int StartOffset=0)
{
	for ( int i=StartOffset;	i<Data.GetSize()-Pattern.GetSize();	i++ )
	{
		bool Match = true;
		for ( int p=0;	Match && p<Pattern.GetSize();	p++ )
			Match &= ( Data[i+p] == Pattern[p] );

		if ( !Match )
			continue;
			
		return i;
	}
	return -1;
}


Soy264::TError::Type Soy264::TDecoder::DecodeNextNalPacket(TNalPacket& Packet)
{
	BufferArray<char,4> NalMarker;
	NalMarker.PushBack(0);
	NalMarker.PushBack(0);
	NalMarker.PushBack(0);
	NalMarker.PushBack(1);

	BufferArray<int,2> MarkerIndexes;

	//	search current pending data
	int SearchPos = 0;
	{
		int StartIndex = FindPatternStart( GetArrayBridge( mPendingData ), NalMarker );
		while ( StartIndex != -1 && MarkerIndexes.GetSize() < MarkerIndexes.MaxSize() )
		{
			MarkerIndexes.PushBack( StartIndex );
			SearchPos = StartIndex + NalMarker.GetSize();
			StartIndex = FindPatternStart( GetArrayBridge( mPendingData ), NalMarker, SearchPos );

			//	finished going through the pending data, so no need to search any more
			//	gr: might need to rewind a few bytes
			if ( StartIndex == -1 )
				SearchPos = ofMax( 0, mPendingData.GetSize() - NalMarker.GetSize() );
		}
		
	}

	//	read more data until we get enough
	while ( MarkerIndexes.GetSize() < 2 )
	{
		BufferArray<char,1024> Buffer;
		if ( !mStream->Read( GetArrayBridge(Buffer) ) )
			return TError::EndOfFile;
		mPendingData.PushBackArray( Buffer );

		int StartIndex = FindPatternStart( GetArrayBridge( mPendingData ), NalMarker, SearchPos );
		if ( StartIndex != -1 )
		{
			MarkerIndexes.PushBack( StartIndex );
			SearchPos = StartIndex + NalMarker.GetSize();
		}
		else
		{
			SearchPos = ofMax( 0, mPendingData.GetSize() - NalMarker.GetSize() );
		}
	}

	//	found enough markers for a packet!
	if ( MarkerIndexes.GetSize() < 2 )
		return TError::NalMarkersNotFound;
	
	//	first marker should start at the start of the data
	assert( MarkerIndexes[0] == 0 );

	//	pop NAL packet data (not including marker)
	int Index = MarkerIndexes[0] + NalMarker.GetSize();
	int Size = MarkerIndexes[1] - Index;
	auto PacketData = GetRemoteArray( &mPendingData[Index], Size, Size );
	Packet.mData = PacketData;
	mPendingData.RemoveBlock( MarkerIndexes[0], MarkerIndexes[1]-MarkerIndexes[0] );

	auto Error = Packet.InitHeader();
	if ( Error != TError::Success )
		return Error;

	return TError::Success;
}

Soy264::TNalUnitType::Type Soy264::TNalUnitType::GetType(int Value)
{
	if ( Value <= Soy264::TNalUnitType::Invalid )
		return Soy264::TNalUnitType::Invalid;
	if ( Value >= Soy264::TNalUnitType::Count )
		return Soy264::TNalUnitType::Invalid;

	return static_cast<Soy264::TNalUnitType::Type>( Value );
}

const char* Soy264::TNalUnitType::ToString(Soy264::TNalUnitType::Type NalUnitType)
{
	switch ( NalUnitType )
	{
	case Soy264::TNalUnitType::Slice_NonIDRPicture:			return "Coded slice of a non-IDR picture";
	case Soy264::TNalUnitType::Slice_CodedPartitionA:		return "Coded slice data partition A";
	case Soy264::TNalUnitType::Slice_CodedPartitionB:		return "Coded slice data partition B";
	case Soy264::TNalUnitType::Slice_CodedPartitionC:		return "Coded slice data partition C";
	case Soy264::TNalUnitType::Slice_CodedIDRPicture:		return "Coded slice of an IDR picture";
	case Soy264::TNalUnitType::SupplimentalEnhancementInformation:	return "Supplemental enhancement information (SEI)";
	case Soy264::TNalUnitType::SequenceParameterSet:		return "Sequence parameter set";
	case Soy264::TNalUnitType::PictureParameterSet:			return "Picture parameter set";
	case Soy264::TNalUnitType::AccessUnitDelimiter:			return "Access unit delimiter";
	case Soy264::TNalUnitType::EndOfSequence:				return "End of sequence";
	case Soy264::TNalUnitType::EndOfStream:					return "End of stream";
	case Soy264::TNalUnitType::FillerData:					return "Filler data";
	case Soy264::TNalUnitType::SequenceParameterSetExtension:	return "SequenceParameterSetExtension";
	case Soy264::TNalUnitType::Slice_AuxCodedUnpartitioned:		return "Slice_AuxCodedUnpartitioned";

	case Soy264::TNalUnitType::Reserved14:			
	case Soy264::TNalUnitType::Reserved15:
	case Soy264::TNalUnitType::Reserved16:
	case Soy264::TNalUnitType::Reserved17:
	case Soy264::TNalUnitType::Reserved18:
	case Soy264::TNalUnitType::Reserved20:
	case Soy264::TNalUnitType::Reserved21:
	case Soy264::TNalUnitType::Reserved22:
	case Soy264::TNalUnitType::Reserved23:
		return "Reserved";

	case Soy264::TNalUnitType::Unspecified:	
	case Soy264::TNalUnitType::Unspecified24:
	case Soy264::TNalUnitType::Unspecified25:
	case Soy264::TNalUnitType::Unspecified26:
	case Soy264::TNalUnitType::Unspecified27:
	case Soy264::TNalUnitType::Unspecified28:
	case Soy264::TNalUnitType::Unspecified29:
	case Soy264::TNalUnitType::Unspecified30:
	case Soy264::TNalUnitType::Unspecified31:
		return "Unspecified";

	case Invalid:
	default:
		return "Invalid";
	};
}

Soy264::TNalPacket::TNalPacket() :
	mBitPos	( 0 )
{
}

Soy264::TError::Type Soy264::TNalPacket::InitHeader()
{
	int ForbiddenZero = ReadBits(1);
	int RefIDC = ReadBits(2);
	int UnitType = ReadBits(5);
	//nalu->last_rbsp_byte=&nal_buf[nalu_size-1];

	mHeader.mType = TNalUnitType::GetType( UnitType );

	return TError::Success;
}

BufferString<100> Soy264::TNalPacket::GetDebug()
{
	BufferString<100> Debug;
	Debug << TNalUnitType::ToString( mHeader.mType ) << " ";
	Debug << Soy::FormatSizeBytes(mData.GetSize()) << " ";

	return Debug;
}

int Soy264::TNalPacket::ReadBits(int BitSize)
{
	if ( BitSize <= 0 )
		return -1;

	//	current byte
	int CurrentByte = mBitPos / 8;
	int CurrentBit = mBitPos % 8;

	//	out of range 
	if ( CurrentByte >= mData.GetSize() )
		return -1;

	//	move along
	mBitPos += BitSize;

	//	get byte
	int DataByte = mData[CurrentByte];
	//	pick out certain bits
	DataByte >>= CurrentBit;
	DataByte &= (1<<BitSize)-1;
	return DataByte;
}

