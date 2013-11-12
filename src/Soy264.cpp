#include "Soy264.h"
#include <SoyDebug.h>


using namespace Soy264;



TNalPriority::Type TNalPriority::GetType(int Priority)
{
	if ( Priority < TNalPriority::Disposable || Priority > TNalPriority::Highest )
		return TNalPriority::Invalid;

	return static_cast<TNalPriority::Type>( Priority );
}

const char*	TNalPriority::ToString(TNalPriority::Type Priority)
{
	switch ( Priority )
	{
	case TNalPriority::Highest:		return "Highest";
	case TNalPriority::High:		return "High";
	case TNalPriority::Low:			return "Low";
	case TNalPriority::Disposable:	return "Disposable";
	default:
		return "Unknown Priority";
	}
}


TPictureOrderCountType::Type TPictureOrderCountType::GetType(int PictureOrderCountType)
{
	if ( PictureOrderCountType < 0 || PictureOrderCountType > 2 )
		return TPictureOrderCountType::Invalid;

	return static_cast<TPictureOrderCountType::Type>( PictureOrderCountType );
}

TChromaFormat::Type TChromaFormat::GetType(int ChromaFormat)
{
	if ( ChromaFormat < 0 || ChromaFormat > TChromaFormat::YUV444 )
		return TChromaFormat::Unknown;

	return static_cast<TChromaFormat::Type>( ChromaFormat );
}

const char*	TChromaFormat::ToString(TChromaFormat::Type Format)
{
	switch ( Format )
	{
	case TChromaFormat::YUV400:	return "YUV400";
	case TChromaFormat::YUV420:	return "YUV420";
	case TChromaFormat::YUV422:	return "YUV422";
	case TChromaFormat::YUV444:	return "YUV444";
	default:
		return "Unknown format";
	}
}


TProfileIDC::Type TProfileIDC::GetType(int ProfileIDC)
{
	switch ( ProfileIDC )
	{
		case FREXT_CAVLC444:
		case BASELINE:
		case MAIN:
		case EXTENDED:
		case FREXT_HP:
		case FREXT_Hi10P:
		case FREXT_Hi422:
		case FREXT_Hi444:
		case MULTIVIEW_HIGH:
		case STEREO_HIGH:
			return static_cast<TProfileIDC::Type>( ProfileIDC );
	}

	return TProfileIDC::Invalid;
}

TSliceType::Type TSliceType::GetType(int SliceType)
{
	//	error if >= 5?
	if ( SliceType < 0 )
		return TSliceType::Invalid;

	SliceType %= 5;
	return static_cast<TSliceType::Type>( SliceType );
}

const char* TSliceType::ToString(TSliceType::Type SliceType)
{
	switch ( SliceType )
	{
	case P_SLICE:	return "P Slice";
	case B_SLICE:	return "B Slice";
	case I_SLICE:	return "I Slice";
	case SP_SLICE:	return "SP Slice";
	case SI_SLICE:	return "SI Slice";
	default:
		return "Unknown slice type";
	}
}

const char*	TError::ToString(TError::Type Error)
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

TError::Type TNalPacketHeader::Init(uint8 HeaderByte)
{
	BufferArray<char,1> HeaderBytes;
	HeaderBytes.PushBack( HeaderByte );
	TBitReader BitReader( GetArrayBridge( HeaderBytes ) );

	bool Success = true;
	int ForbiddenZero = 0;
	int UnitType = 0;
	int RefId = 0;
	Success &= BitReader.Read( ForbiddenZero, 1 );
	Success &= BitReader.Read( RefId, 2 );
	Success &= BitReader.Read( UnitType, 5 );

	if ( !Success )
		return TError::FailedToReadNalPacketHeader;

	if ( ForbiddenZero != 0 )
		return TError::NalPacketForbiddenZeroNotZero;

	mPriority = TNalPriority::GetType( RefId );
	mType = TNalUnitType::GetType( UnitType );
	if ( mType == TNalUnitType::Invalid )
		return TError::InvalidNalPacketType;

	return TError::Success;
}

TError::Type TDecoder::Load(const char* Filename)
{
	mStream = ofPtr<SoyStreamBinaryFile>( new SoyStreamBinaryFile( Filename ) );
	if ( !mStream->IsValid() )
	{
		mStream.reset();
		return TError::CouldNotOpenFile;
	}
	
	return TError::Success;
}

TError::Type TDecoder::DecodeNextFrame(TPixels& Pixels)
{
	//	read nal packets until we have enough to make a frame
	Array<TNalPacket*> NalPackets;
	int LastSpsPacket = -1;

	while ( true )
	{
		TNalPacketRaw PacketRaw;
		{
			auto Error = DecodeNextNalPacket( PacketRaw );
			if ( Error != TError::Success )
				return Error;
		}

		//	make actual packet type from factory
		auto* pLastSpsPacket = (LastSpsPacket<0) ? nullptr : static_cast<TNalPacket_SPS*>(NalPackets[LastSpsPacket]);
		TNalPacket* Packet = nullptr;
		{
			auto Error = CreatePacket( Packet, PacketRaw, pLastSpsPacket );
			if ( !Packet || Error != TError::Success )
			{
				BufferString<100> Debug;
				Debug << "Error creating packet (" << TNalUnitType::ToString(PacketRaw.GetHeader().mType) << "; " << TError::ToString( Error );
				ofLogNotice( Debug.c_str() );
				continue;
			}
		}
		
		int PacketIndex = NalPackets.GetSize();
		NalPackets.PushBack( Packet );

		if ( PacketRaw.GetHeader().mType == TNalUnitType::SequenceParameterSet )
			LastSpsPacket = PacketIndex;

		//	log the packet read
		TString Debug;
		TString PacketDebug;
		Packet->GetDebug(PacketDebug);
		Debug << "Read packet #" << PacketIndex << " " << PacketDebug;
		ofLogNotice( Debug.c_str() );

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


TError::Type TDecoder::DecodeNextNalPacket(TNalPacketRaw& Packet)
{
	BufferArray<char,4> NalMarker;
	//NalMarker.PushBack(0);
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

TNalUnitType::Type TNalUnitType::GetType(int Value)
{
	if ( Value <= TNalUnitType::Invalid )
		return TNalUnitType::Invalid;
	if ( Value >= TNalUnitType::Count )
		return TNalUnitType::Invalid;

	return static_cast<TNalUnitType::Type>( Value );
}

const char* TNalUnitType::ToString(TNalUnitType::Type NalUnitType)
{
	switch ( NalUnitType )
	{
	case TNalUnitType::Slice_NonIDRPicture:			return "Coded slice of a non-IDR picture";
	case TNalUnitType::Slice_CodedPartitionA:		return "Coded slice data partition A";
	case TNalUnitType::Slice_CodedPartitionB:		return "Coded slice data partition B";
	case TNalUnitType::Slice_CodedPartitionC:		return "Coded slice data partition C";
	case TNalUnitType::Slice_CodedIDRPicture:		return "Coded slice of an IDR picture";
	case TNalUnitType::SupplimentalEnhancementInformation:	return "Supplemental enhancement information (SEI)";
	case TNalUnitType::SequenceParameterSet:		return "Sequence parameter set";
	case TNalUnitType::PictureParameterSet:			return "Picture parameter set";
	case TNalUnitType::AccessUnitDelimiter:			return "Access unit delimiter";
	case TNalUnitType::EndOfSequence:				return "End of sequence";
	case TNalUnitType::EndOfStream:					return "End of stream";
	case TNalUnitType::FillerData:					return "Filler data";
	case TNalUnitType::SequenceParameterSetExtension:	return "SequenceParameterSetExtension";
	case TNalUnitType::Slice_AuxCodedUnpartitioned:		return "Slice_AuxCodedUnpartitioned";

	case TNalUnitType::Reserved14:			
	case TNalUnitType::Reserved15:
	case TNalUnitType::Reserved16:
	case TNalUnitType::Reserved17:
	case TNalUnitType::Reserved18:
	case TNalUnitType::Reserved20:
	case TNalUnitType::Reserved21:
	case TNalUnitType::Reserved22:
	case TNalUnitType::Reserved23:
		return "Reserved";

	case TNalUnitType::Unspecified:	
	case TNalUnitType::Unspecified24:
	case TNalUnitType::Unspecified25:
	case TNalUnitType::Unspecified26:
	case TNalUnitType::Unspecified27:
	case TNalUnitType::Unspecified28:
	case TNalUnitType::Unspecified29:
	case TNalUnitType::Unspecified30:
	case TNalUnitType::Unspecified31:
		return "Unspecified";

	case Invalid:
	default:
		return "Invalid";
	};
}


TError::Type TNalPacketRaw::Init(const ArrayBridge<char>& Data,TNalPacketMeta Meta)
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
	//	read until we find the control bit
	int exp = 0;
	while ( Read(1) == 0 )
		exp++;
	
	//	insert first bit (if first bit is control bit-0- the -1 later will remove it)
	Data = 1 << exp;

	for ( int i=exp-1;	i>=0;	i--)
	{
		Data |= Read(1) << i;
	}
	
	Data = Data-1;
	return true;
}

bool TBitReader::ReadSignedExpGolomb(int& Data)
{
	if ( !ReadUnsignedExpGolomb( Data ) )
		return false;
	
	Data = (Data&1) ? (Data+1)>>1 : -(Data>>1);
	return true;
}



TError::Type TDecoder::CreatePacket(TNalPacket*& Packet,const TNalPacketRaw& PacketRaw,const TNalPacket_SPS* LastSpsPacket)
{
	switch ( PacketRaw.mHeader.mType )
	{
		case TNalUnitType::SequenceParameterSet:	
			Packet = new TNalPacket_SPS( PacketRaw );
			return TError::Success;

		case TNalUnitType::PictureParameterSet:		
			Packet = new TNalPacket_PPS( PacketRaw );
			return TError::Success;

		case TNalUnitType::Slice_CodedIDRPicture:
		{
			if ( !LastSpsPacket )
				return TError::MissingSPSPacket;
			Packet = new TNalPacket_SliceIDR( PacketRaw, *LastSpsPacket );
			return TError::Success;
		}

		case TNalUnitType::Slice_NonIDRPicture:		
		{
			if ( !LastSpsPacket )
				return TError::MissingSPSPacket;
			Packet = new TNalPacket_SliceNonIDR( PacketRaw, *LastSpsPacket );
			return TError::Success;
		}
	};

	BufferString<100> Debug;
	Debug << "Unsupported packet type; " << TNalUnitType::ToString( PacketRaw.mHeader.mType );
	ofLogNotice( Debug.c_str() );
	return TError::UnsupportedPacketType;
}


TNalPacket_SPS::TNalPacket_SPS(const TNalPacketRaw& PacketRaw) :
	TNalPacket	( PacketRaw )
{
	TBitReader BitReader( GetArrayBridge( PacketRaw.mData ) );

	Read( BitReader );
}
	
void TNalPacket_SPS::GetDebug(TString& Debug) const
{
	TNalPacket::GetDebug( Debug );
	//	show width etc
	Debug << " " << TChromaFormat::ToString( mChromaFormat );
}

TError::Type TNalPacket_SPS::Read(TBitReader& BitReader)
{
	//	read SPS header

	mProfileIDC = TProfileIDC::GetType( BitReader.Read(8) );

	constraint_set0_flag                  =BitReader.Read(1);
	constraint_set1_flag                  =BitReader.Read(1);
	constraint_set2_flag                  =BitReader.Read(1);
	reserved_zero_5bits                   =BitReader.Read(5);
	level_idc                             =BitReader.Read(8);

	seq_parameter_set_id                  =BitReader.ReadUnsignedExpGolomb();
	//	gr: these should be the same?
	//this->mHeader.mRefId == seq_parameter_set_id

	//	extra header depending on profile format
	switch ( mProfileIDC )
	{
	case TProfileIDC::FREXT_HP:
	case TProfileIDC::FREXT_Hi10P:
	case TProfileIDC::FREXT_Hi422:
	case TProfileIDC::FREXT_Hi444:
	case TProfileIDC::FREXT_CAVLC444:
		
	case TProfileIDC::MULTIVIEW_HIGH:		//#if (MVC_EXTENSION_ENABLE)
	case TProfileIDC::STEREO_HIGH:	//#if (MVC_EXTENSION_ENABLE)
		auto Error = ReadColourFormat( BitReader );
		if ( Error != TError::Success )
			return Error;
	};


	mFrameNumMaxBits = BitReader.ReadUnsignedExpGolomb()+4;

	mPictureOrderCountType = TPictureOrderCountType::GetType( BitReader.ReadUnsignedExpGolomb() );
	if( mPictureOrderCountType == TPictureOrderCountType::TYPE_0 ) 
	{
		mPictureOrderCountMaxBits = BitReader.ReadUnsignedExpGolomb()+4;
	}
	else if( mPictureOrderCountType == TPictureOrderCountType::TYPE_1 )
	{
		assert(false);
		/*
		delta_pic_order_always_zero_flag    =BitReader.Read(1);
		offset_for_non_ref_pic              =BitReader.ReadSignedExpGolomb();
		offset_for_top_to_bottom_field      =BitReader.ReadSignedExpGolomb();
		num_ref_frames_in_pic_order_cnt_cycle=BitReader.ReadUnsignedExpGolomb();
		for( int i=0; i<num_ref_frames_in_pic_order_cnt_cycle; ++i)
			offset_for_ref_frame[i]           =BitReader.ReadSignedExpGolomb();
			*/
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

	return TError::Success;
}
	
const int ZZ_SCAN16[16]  =
{  
	0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

const int ZZ_SCAN64[64] =
{  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

template<int MATRIX_SIZE>
bool TScalingList_Init(TScalingList<MATRIX_SIZE>& ScalingList,TBitReader& BitReader,const int* MatrixScan)
{
	int lastScale      = 8;
	int nextScale      = 8;

	for( int j=0; j<ScalingList.mMatrix.GetSize(); j++)
	{
		int scanj = MatrixScan[j];

		if(nextScale!=0)
		{
			int delta_scale;
			if ( !BitReader.ReadSignedExpGolomb( delta_scale ) )
				return false;
			nextScale = (lastScale + delta_scale + 256) % 256;
			ScalingList.mUseDefault = (scanj==0 && nextScale==0);
		}

		ScalingList.mMatrix[scanj] = (nextScale==0) ? lastScale:nextScale;
		lastScale = ScalingList.mMatrix[scanj];
	}

	return true;
}

template<> bool TScalingList<16>::Init(TBitReader& BitReader)
{
	return TScalingList_Init( *this, BitReader, ZZ_SCAN16 );
}

template<> bool TScalingList<64>::Init(TBitReader& BitReader)
{
	return TScalingList_Init( *this, BitReader, ZZ_SCAN64 );
}


TError::Type TNalPacket_SPS::ReadColourFormat(TBitReader& BitReader)
{
	mChromaFormat = TChromaFormat::GetType( BitReader.ReadUnsignedExpGolomb() );
	
    if ( mChromaFormat == TChromaFormat::YUV444 )
		mSeperateChromaPlane = BitReader.Read(1);
	else
		mSeperateChromaPlane = false;

    bit_depth_luma_minus8                  = BitReader.ReadUnsignedExpGolomb();
    bit_depth_chroma_minus8                = BitReader.ReadUnsignedExpGolomb();
   
	//checking;
	/*
    if((sps->bit_depth_luma_minus8+8 > sizeof(imgpel)*8) || (sps->bit_depth_chroma_minus8+8> sizeof(imgpel)*8))
	{
		error ("Source picture has higher bit depth than imgpel data type. \nPlease recompile with larger data type for imgpel.", 500);
	}
	*/

    lossless_qpprime_flag          = BitReader.Read(1);
    
	int seq_scaling_matrix_present_flag	= BitReader.Read(1);
	if( seq_scaling_matrix_present_flag)
	{
		//	test me!
		assert(false);
		int n_ScalingList = (mChromaFormat != TChromaFormat::YUV444) ? 8 : 12;
		mScalingList4x4.SetSize( 6 );
		mScalingList8x8.SetSize( n_ScalingList - 6 );

		for ( int i=0;	i<mScalingList4x4.GetSize();	i++ )
		{
			mScalingList4x4[i].mIsPresent = BitReader.Read(1);
			if ( mScalingList4x4[i].mIsPresent )
				mScalingList4x4[i].Init( BitReader );
		}
		
		for ( int i=0;	i<mScalingList8x8.GetSize();	i++ )
		{
			mScalingList8x8[i].mIsPresent = BitReader.Read(1);
			if ( mScalingList8x8[i].mIsPresent )
				mScalingList8x8[i].Init( BitReader );
		}
	}

	return TError::Success;
}


TNalPacket_PPS::TNalPacket_PPS(const TNalPacketRaw& PacketRaw) :
	TNalPacket	( PacketRaw )
{
}



TNalPacket_SliceNonIDR::TNalPacket_SliceNonIDR(const TNalPacketRaw& PacketRaw,const TNalPacket_SPS& Sequence) :
	TNalPacket	( PacketRaw )
{
	//	read slice header
	TBitReader BitReader( GetArrayBridge( PacketRaw.mData ) );

	//auto Error = mSliceHeader.Read( BitReader, mHeader, Sequence );
	
}

	
void TNalPacket_SliceNonIDR::GetDebug(TString& Debug) const
{
	TNalPacket::GetDebug( Debug );
	Debug << " " << TSliceType::ToString( mSliceHeader.mSliceType );
}


TNalPacket_SliceIDR::TNalPacket_SliceIDR(const TNalPacketRaw& PacketRaw,const TNalPacket_SPS& Sequence) :
	TNalPacket	( PacketRaw )
{
	//	read slice header
	TBitReader BitReader( GetArrayBridge( PacketRaw.mData ) );

	auto Error = mSliceHeader.Read( BitReader, mHeader, Sequence );
	
}

	
void TNalPacket_SliceIDR::GetDebug(TString& Debug) const
{
	TNalPacket::GetDebug( Debug );
	Debug << " " << TSliceType::ToString( mSliceHeader.mSliceType );
}

TError::Type TSliceHeader::Read(TBitReader& BitReader,const TNalPacketHeader& Packet,const TNalPacket_SPS& Sequence)
{
	mFirstMbInSlice = BitReader.ReadUnsignedExpGolomb();
	mSliceType = TSliceType::GetType( BitReader.ReadUnsignedExpGolomb() );

	int mPictureParameterSetId = BitReader.ReadUnsignedExpGolomb();
	 
	if ( Sequence.mSeperateChromaPlane )
		mColourPlane = TColourPlane::GetType( BitReader.Read(2) );
	else
		mColourPlane = TColourPlane::PLANE_Y;
	 
	mFrameNumber = BitReader.Read( Sequence.mFrameNumMaxBits );
	
	if ( Packet.mType == TNalUnitType::Slice_CodedIDRPicture ) //if (p_Vid->idr_flag)
	{
		/*
		p_Vid->pre_frame_num = currSlice->frame_num;
		// picture error concealment
		p_Vid->last_ref_pic_poc = 0;
		*/
		assert( mFrameNumber == 0);		
	}

	if ( Sequence.frame_mbs_only_flag )
	{
		mPictureStructure = TPictureStructure::WHOLE_FRAME;
	}
	else
	{
		auto field_pic_flag = BitReader.Read(1);
		if ( field_pic_flag )
		{
			auto bottom_field_flag = BitReader.Read(1);
			mPictureStructure = bottom_field_flag ? TPictureStructure::BOTTOM_FIELD : TPictureStructure::TOP_FIELD;
		}
		else
		{
			mPictureStructure = TPictureStructure::WHOLE_FRAME;
		}
	}

	if ( Packet.mType == TNalUnitType::Slice_CodedIDRPicture )	//	idr_flag == type==IDR
	{
		mIDRPicId = BitReader.ReadUnsignedExpGolomb();
	}
	//else if ( true/*MVC_EXTENSION_ENABLE*/ && Sequence.svc_extension_flag == 0 && Sequence.NaluHeaderMVCExt.non_idr_flag == 0 )
	else if ( true )
	{
		assert(false);
		mIDRPicId = BitReader.ReadUnsignedExpGolomb();
	}

	
	if ( Sequence.mPictureOrderCountType == TPictureOrderCountType::TYPE_0 )
	{
		//me
		//Sequence.pic_order_cnt_lsb = BitReader.Read( Sequence.mPictureOrderCountMaxBits );
		auto pic_order_cnt_lsb = BitReader.Read( Sequence.mPictureOrderCountMaxBits );
		/*
		if( p_Vid->active_pps->bottom_field_pic_order_in_frame_present_flag  ==  1 &&  !currSlice->field_pic_flag )
			currSlice->delta_pic_order_cnt_bottom = BitReader.ReadSignedExpGolomb("SH: delta_pic_order_cnt_bottom", currStream, &p_Dec->UsedBits);
		else
			currSlice->delta_pic_order_cnt_bottom = 0;	//	me
			*/
	}
	if ( Sequence.mPictureOrderCountType == TPictureOrderCountType::TYPE_1 )
	{
		assert(false);
		/*
		if ( !active_sps->delta_pic_order_always_zero_flag )
		{
			currSlice->delta_pic_order_cnt[ 0 ] = BitReader.ReadSignedExpGolomb("SH: delta_pic_order_cnt[0]", currStream, &p_Dec->UsedBits);
			if( p_Vid->active_pps->bottom_field_pic_order_in_frame_present_flag  ==  1  &&  !currSlice->field_pic_flag )
				currSlice->delta_pic_order_cnt[ 1 ] = BitReader.ReadSignedExpGolomb("SH: delta_pic_order_cnt[1]", currStream, &p_Dec->UsedBits);
			else
				currSlice->delta_pic_order_cnt[ 1 ] = 0;  // set to zero if not in stream
		}
		else
		{
			currSlice->delta_pic_order_cnt[ 0 ] = 0;
			currSlice->delta_pic_order_cnt[ 1 ] = 0;
		}
		*/
	}
	
	//if (p_Vid->active_pps->redundant_pic_cnt_present_flag)
	if ( false )
	{
	    //currSlice->redundant_pic_cnt = BitReader.ReadUnsignedExpGolomb();
	}
	
	if ( mSliceType == TSliceType::B_SLICE )
		auto direct_spatial_mv_pred_flag = BitReader.Read(1);
	
	if ( mSliceType == TSliceType::P_SLICE || mSliceType == TSliceType::SP_SLICE || mSliceType == TSliceType::B_SLICE )
	{
		assert(false);
		//currSlice->num_ref_idx_active[LIST_0] = p_Vid->active_pps->num_ref_idx_l0_default_active_minus1 + 1;
		//currSlice->num_ref_idx_active[LIST_1] = p_Vid->active_pps->num_ref_idx_l1_default_active_minus1 + 1;
		auto num_ref_idx_override_flag = BitReader.Read(1);
		if ( num_ref_idx_override_flag )
		{
			//currSlice->num_ref_idx_active[LIST_0] = 1 + read_ue_v ("SH: num_ref_idx_l0_active_minus1", currStream, &p_Dec->UsedBits);

			//if ( mSliceType == TSliceType::B_SLICE )
			//	currSlice->num_ref_idx_active[LIST_1] = 1 + read_ue_v ("SH: num_ref_idx_l1_active_minus1", currStream, &p_Dec->UsedBits);
		}
	}
	
	if ( mSliceType != TSliceType::B_SLICE )
	{
		num_ref_idx_active[LIST_1] = 0;
	}
	/*
#if (MVC_EXTENSION_ENABLE)
  if (currSlice->svc_extension_flag == 0 || currSlice->svc_extension_flag == 1)
    ref_pic_list_mvc_modification(currSlice);
  else
    ref_pic_list_reordering(currSlice);
#else
  ref_pic_list_reordering(currSlice);
#endif
  */

	//if ( Packet.mType == TNalUnitType::Slice_CodedIDRPicture || (pSlice->svc_extension_flag == 0 && pSlice->NaluHeaderMVCExt.non_idr_flag == 0) )
	if ( Packet.mType == TNalUnitType::Slice_CodedIDRPicture )
	{
		auto no_output_of_prior_pics_flag = BitReader.Read(1);
		//p_Vid->no_output_of_prior_pics_flag = pSlice->no_output_of_prior_pics_flag;
		auto long_term_reference_flag = BitReader.Read(1);
	}
	else
	{
		assert(false);
		/*
		 pSlice->adaptive_ref_pic_buffering_flag = read_u_1("SH: adaptive_ref_pic_buffering_flag", currStream, &p_Dec->UsedBits);
		if (pSlice->adaptive_ref_pic_buffering_flag)
		{
		  // read Memory Management Control Operation
		  do
		  {
			tmp_drpm=(DecRefPicMarking_t*)calloc (1,sizeof (DecRefPicMarking_t));
			tmp_drpm->Next=NULL;

			val = tmp_drpm->memory_management_control_operation = read_ue_v("SH: memory_management_control_operation", currStream, &p_Dec->UsedBits);

			if ((val==1)||(val==3))
			{
			  tmp_drpm->difference_of_pic_nums_minus1 = read_ue_v("SH: difference_of_pic_nums_minus1", currStream, &p_Dec->UsedBits);
			}
			if (val==2)
			{
			  tmp_drpm->long_term_pic_num = read_ue_v("SH: long_term_pic_num", currStream, &p_Dec->UsedBits);
			}

			if ((val==3)||(val==6))
			{
			  tmp_drpm->long_term_frame_idx = read_ue_v("SH: long_term_frame_idx", currStream, &p_Dec->UsedBits);
			}
			if (val==4)
			{
			  tmp_drpm->max_long_term_frame_idx_plus1 = read_ue_v("SH: max_long_term_pic_idx_plus1", currStream, &p_Dec->UsedBits);
			}

			// add command
			if (pSlice->dec_ref_pic_marking_buffer==NULL)
			{
			  pSlice->dec_ref_pic_marking_buffer=tmp_drpm;
			}
			else
			{
			  tmp_drpm2=pSlice->dec_ref_pic_marking_buffer;
			  while (tmp_drpm2->Next!=NULL) tmp_drpm2=tmp_drpm2->Next;
			  tmp_drpm2->Next=tmp_drpm;
			}

		  }
		  while (val != 0);
		}
	*/
	}

  



	return TError::Success;
}

