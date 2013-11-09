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
	case NalPacketForbiddenZeroNotZero:	return "Forbidden Zero, not zero.";
	case InvalidNalPacketType:	return "Invalid NAL packet type";
	default:					return "Unknown error type";
	}
}

Soy264::TError::Type Soy264::TNalPacketHeader::Init(uint8 HeaderByte)
{
	BufferArray<char,1> HeaderBytes;
	HeaderBytes.PushBack( HeaderByte );
	TBitReader BitReader( GetArrayBridge( HeaderBytes ) );

	bool Success = true;
	int ForbiddenZero = 0;
	int UnitType = 0;
	Success &= BitReader.Read( ForbiddenZero, 1 );
	Success &= BitReader.Read( mRefId, 2 );
	Success &= BitReader.Read( UnitType, 5 );

	if ( !Success )
		return TError::FailedToReadNalPacketHeader;

	if ( ForbiddenZero != 0 )
		return TError::NalPacketForbiddenZeroNotZero;

	mType = TNalUnitType::GetType( UnitType );
	if ( mType == TNalUnitType::Invalid )
		return TError::InvalidNalPacketType;

	return TError::Success;
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
	Array<TNalPacket*> NalPackets;

	while ( true )
	{
		TNalPacketRaw Packet;
		auto Error = DecodeNextNalPacket( Packet );
		if ( Error != TError::Success )
			return Error;

		//	log the packet read
		BufferString<100> Debug;
		Debug << "Read packet " << NalPackets.GetSize() << " " << Packet.GetDebug();
		ofLogNotice( Debug.c_str() );

		//	make actual packet type from factory
		auto* pPacket = CreatePacket( Packet );
		if ( !pPacket )
			continue;

		NalPackets.PushBack( pPacket );

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


Soy264::TError::Type Soy264::TDecoder::DecodeNextNalPacket(TNalPacketRaw& Packet)
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
		BufferArray<char,1024*4> Buffer;
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
	//assert( MarkerIndexes[0] == 0 );

	//	pop NAL packet data (not including marker)
	int Index = MarkerIndexes[0] + NalMarker.GetSize();
	int Size = MarkerIndexes[1] - Index;
	auto PacketData = GetRemoteArray( &mPendingData[Index], Size, Size );

	TNalPacketMeta Meta;
	Meta.mFilePosition = Index + mPendingDataFileOffset;
	auto Error = Packet.Init( GetArrayBridge(PacketData), Meta );

	mPendingData.RemoveBlock( MarkerIndexes[0], MarkerIndexes[1]-MarkerIndexes[0] );
	mPendingDataFileOffset += MarkerIndexes[1]-MarkerIndexes[0];

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


Soy264::TError::Type Soy264::TNalPacketRaw::Init(const ArrayBridge<char>& Data,TNalPacketMeta Meta)
{
	//	copy data
	mData = Data;
	mMeta = Meta;

	if ( mData.IsEmpty() )
		return TError::FailedToReadNalPacketHeader;

	//	read header
	//	pop byte for header
	char HeaderByte = mData.PopAt(0);
	auto Error = mHeader.Init( HeaderByte );
	if ( Error != TError::Success )
		return Error;

	return TError::Success;
}


BufferString<100> Soy264::TNalPacketRaw::GetDebug()
{
	BufferString<100> Debug;
	Debug << TNalUnitType::ToString( mHeader.mType ) << " in stream " << mHeader.mRefId << " ";
	Debug << Soy::FormatSizeBytes(mData.GetSize()) << " at filepos " << mMeta.mFilePosition;

	return Debug;
}
	
bool TBitReader::Read(int& Data,int BitCount)
{
	if ( BitCount <= 0 )
		return false;

	//	current byte
	int CurrentByte = mBitPos / 8;
	int CurrentBit = mBitPos % 8;

	//	out of range 
	if ( CurrentByte >= mData.GetSize() )
		return false;

	//	move along
	mBitPos += BitCount;

	//	get byte
	Data = mData[CurrentByte];

	//	pick out certain bits
	//	gr: reverse endianess to what I thought...
	//Data >>= CurrentBit;
	Data >>= 8-CurrentBit-BitCount;
	Data &= (1<<BitCount)-1;

	return true;
}
	
bool TBitReader::ReadUnsignedExpGolomb(int& Data)
{
	int exp;
	for( exp=0;	Read(1)==0;	exp++ )
	{
	}

	if( exp ) 
		Data = (1<<exp)-1 + Read(exp);
	else
		Data = 0;

	return true;
}

bool TBitReader::ReadSignedExpGolomb(int& Data)
{
	if ( !ReadUnsignedExpGolomb( Data ) )
		return false;
	
	Data = (Data&1) ? (Data+1)>>1 : -(Data>>1);
	return true;
}



Soy264::TNalPacket* Soy264::TDecoder::CreatePacket(const Soy264::TNalPacketRaw& Packet)
{
	switch ( Packet.mHeader.mType )
	{
	case Soy264::TNalUnitType::SequenceParameterSet:	return new TNalPacket_SPS( Packet );
	case Soy264::TNalUnitType::PictureParameterSet:		return new TNalPacket_PPS( Packet );
	case Soy264::TNalUnitType::Slice_NonIDRPicture:		return new TNalPacket_SliceNonIDR( Packet );
	};

	BufferString<100> Debug;
	Debug << "Unsupported packet type; " << Soy264::TNalUnitType::ToString( Packet.mHeader.mType );
	ofLogNotice( Debug.c_str() );
	return nullptr;
}


Soy264::TNalPacket_SPS::TNalPacket_SPS(const TNalPacketRaw& PacketRaw) :
	TNalPacket	( PacketRaw )
{
	//	read SPS header
	TBitReader BitReader( GetArrayBridge( PacketRaw.mData ) );

	profile_idc                           =BitReader.Read(8);
	constraint_set0_flag                  =BitReader.Read(1);
	constraint_set1_flag                  =BitReader.Read(1);
	constraint_set2_flag                  =BitReader.Read(1);
	reserved_zero_5bits                   =BitReader.Read(5);
	level_idc                             =BitReader.Read(8);
	seq_parameter_set_id                  =BitReader.ReadUnsignedExpGolomb();
	log2_max_frame_num                    =BitReader.ReadUnsignedExpGolomb()+4;
	MaxFrameNum=1<<log2_max_frame_num;
	pic_order_cnt_type                    =BitReader.ReadUnsignedExpGolomb();
	if(pic_order_cnt_type==0) 
	{
		log2_max_pic_order_cnt_lsb          =BitReader.ReadUnsignedExpGolomb()+4;
		MaxPicOrderCntLsb=1<<log2_max_pic_order_cnt_lsb;
	}
	else if(pic_order_cnt_type==1) 
	{
		delta_pic_order_always_zero_flag    =BitReader.Read(1);
		offset_for_non_ref_pic              =BitReader.ReadSignedExpGolomb();
		offset_for_top_to_bottom_field      =BitReader.ReadSignedExpGolomb();
		num_ref_frames_in_pic_order_cnt_cycle=BitReader.ReadUnsignedExpGolomb();
		for( int i=0; i<num_ref_frames_in_pic_order_cnt_cycle; ++i)
			offset_for_ref_frame[i]           =BitReader.ReadSignedExpGolomb();
	}
	
	num_ref_frames                        =BitReader.ReadUnsignedExpGolomb();
	gaps_in_frame_num_value_allowed_flag  =BitReader.Read(1);
	PicWidthInMbs                         =BitReader.ReadUnsignedExpGolomb()+1;
	PicWidthInSamples=PicWidthInMbs*16;
	PicHeightInMapUnits                   =BitReader.ReadUnsignedExpGolomb()+1;
	PicSizeInMapUnits=PicWidthInMbs*PicHeightInMapUnits;
	frame_mbs_only_flag                   =BitReader.Read(1);
	FrameHeightInMbs=(2-frame_mbs_only_flag)*PicHeightInMapUnits;
	FrameHeightInSamples=16*FrameHeightInMbs;
	if(!frame_mbs_only_flag)
	{
		mb_adaptive_frame_field_flag        =BitReader.Read(1);
	}
	direct_8x8_inference_flag             =BitReader.Read(1);
	frame_cropping_flag                   =BitReader.Read(1);
	if(frame_cropping_flag) 
	{
		frame_crop_left_offset              =BitReader.ReadUnsignedExpGolomb();
		frame_crop_right_offset             =BitReader.ReadUnsignedExpGolomb();
		frame_crop_top_offset               =BitReader.ReadUnsignedExpGolomb();
		frame_crop_bottom_offset            =BitReader.ReadUnsignedExpGolomb();
	}
	vui_parameters_present_flag           =BitReader.Read(1);
}

Soy264::TNalPacket_PPS::TNalPacket_PPS(const TNalPacketRaw& PacketRaw) :
	TNalPacket	( PacketRaw )
{
}

Soy264::TNalPacket_SliceNonIDR::TNalPacket_SliceNonIDR(const TNalPacketRaw& PacketRaw) :
	TNalPacket	( PacketRaw )
{
	//	read slice header
	TBitReader BitReader( GetArrayBridge( PacketRaw.mData ) );

	//	FirstPartOfSliceHeader
	mFirstMbInSlice = BitReader.ReadUnsignedExpGolomb();
	mSliceType = Soy264::TSliceType::GetSliceType( BitReader.ReadUnsignedExpGolomb() );

  currSlice->pic_parameter_set_id = read_ue_v ("SH: pic_parameter_set_id", currStream, &p_Dec->UsedBits);

  if( p_Vid->separate_colour_plane_flag )
    currSlice->colour_plane_id = read_u_v (2, "SH: colour_plane_id", currStream, &p_Dec->UsedBits);
  else
    currSlice->colour_plane_id = PLANE_Y;

  return p_Dec->UsedBits;
}

