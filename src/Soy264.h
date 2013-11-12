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
			MissingSPSPacket,
			UnsupportedPacketType,
		};

		const char*	ToString(Type Error);
	};

	class TSliceHeader;

	class TNalPacket;
	class TNalPacketMeta;
	class TNalPacketRaw;
	class TNalPacketHeader;

	class TNalPacket_SPS;
	class TNalPacket_PPS;
	class TNalPacket_SliceIDR;
	class TNalPacket_SliceNonIDR;

	namespace TSliceType
	{
		enum Type
		{
			Invalid = -1,
			P_SLICE = 0,
			B_SLICE = 1,
			I_SLICE = 2,
			SP_SLICE = 3,
			SI_SLICE = 4,
		};
		Type		GetType(int SliceType);
		const char*	ToString(Type SliceType);
	};

	namespace TChromaFormat
	{
		enum Type
		{
			Unknown	= -1,
			YUV400	=  0,     //!< Monochrome
			YUV420	=  1,     //!< 4:2:0
			YUV422	=  2,     //!< 4:2:2
			YUV444	=  3      //!< 4:4:4
		};

		Type		GetType(int ChromaFormat);
		const char*	ToString(Type SliceType);
	};

	namespace TNalPriority
	{
		enum Type
		{
			Invalid		= -1,
			Highest		= 3,
			High		= 2,
			Low			= 1,
			Disposable	= 0,
		};
		Type		GetType(int Priority);
		const char*	ToString(Type Priority);
	};

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

	
	//AVC Profile IDC definitions
	namespace TProfileIDC
	{
		enum Type
		{
			Invalid			= 0,

			FREXT_CAVLC444 = 44,       //!< YUV 4:4:4/14 "CAVLC 4:4:4"
			BASELINE       = 66,       //!< YUV 4:2:0/8  "Baseline"
			MAIN           = 77,       //!< YUV 4:2:0/8  "Main"
			EXTENDED       = 88,       //!< YUV 4:2:0/8  "Extended"
			FREXT_HP       = 100,      //!< YUV 4:2:0/8  "High"
			FREXT_Hi10P    = 110,      //!< YUV 4:2:0/10 "High 10"
			FREXT_Hi422    = 122,      //!< YUV 4:2:2/10 "High 4:2:2"
			FREXT_Hi444    = 244,      //!< YUV 4:4:4/14 "High 4:4:4"
			MULTIVIEW_HIGH = 118,      //!< YUV 4:2:0/8  "Multiview High"
			STEREO_HIGH    = 128       //!< YUV 4:2:0/8  "Stereo High"
		};
		
		Type	GetType(int ProfileIDC);
	};


	namespace TColourPlane
	{
		enum Type
		{
			// YUV
			PLANE_Y = 0,  // PLANE_Y
			PLANE_U = 1,  // PLANE_Cb
			PLANE_V = 2,  // PLANE_Cr
  
			// RGB
			PLANE_G = 0,
			PLANE_B = 1,
			PLANE_R = 2
		};
		inline Type	GetType(int ColourPlane)	{	return static_cast<Type>( ColourPlane );	}
	};


	namespace TPictureOrderCountType
	{
		enum Type
		{
			Invalid	= -1,
			TYPE_0	= 0,
			TYPE_1	= 1,
			TYPE_2	= 2,
		};
		Type	GetType(int PictureOrderCountType);
	};


	//	these are for multi-view streams (typically stereoscopic) in newer format H264
	//	http://en.wikipedia.org/wiki/Multiview_Video_Coding
	namespace TPictureStructure
	{
		enum Type
		{
			WHOLE_FRAME,
			TOP_FIELD,
			BOTTOM_FIELD,
		};
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
	bool						ReadUnsignedExpGolomb(int& Data);
	bool						ReadSignedExpGolomb(int& Data);
	int							Read(int BitCount)					{	int Data;	return Read( Data, BitCount) ? Data : -1;	}
	int							ReadUnsignedExpGolomb()				{	int Data;	return ReadUnsignedExpGolomb( Data ) ? Data : -1;	}
	int							ReadSignedExpGolomb()				{	int Data;	return ReadSignedExpGolomb( Data ) ? Data : -1;	}

private:
	const ArrayBridge<char>&	mData;
	unsigned int				mBitPos;	//	current bit-reading-pos 
};



class Soy264::TNalPacketHeader
{
public:
	TNalPacketHeader() :
		mPriority	( TNalPriority::Invalid ),
		mType		( TNalUnitType::Invalid )
	{
	}

	TError::Type			Init(uint8 HeaderByte);
	
	BufferString<100>		GetDebug() const	{	return BufferString<100>() << TNalUnitType::ToString( mType ) << "[" << TNalPriority::ToString(mPriority) << "]";	}

public:
	TNalUnitType::Type		mType;
	TNalPriority::Type		mPriority;
};

class Soy264::TNalPacketMeta
{
public:
	TNalPacketMeta() :
		mPacketIndex	( -1 ),
		mFilePosition	( -1 )
	{
	}

	BufferString<100>		GetDebug() const	{	return "";	}

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

	const TNalPacketMeta&	GetMeta() const		{	return mMeta;	}
	const TNalPacketHeader&	GetHeader() const	{	return mHeader;	}
	BufferString<100>		GetDebug() const;

	TError::Type			Init(const ArrayBridge<char>& Data,TNalPacketMeta Meta);

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
	TError::Type	CreatePacket(Soy264::TNalPacket*& Packet,const TNalPacketRaw& PacketRaw,const TNalPacket_SPS* LastSpsPacket);	//	create packet type from factory

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

	const TNalPacketMeta&	GetMeta() const					{	return mMeta;	}
	const TNalPacketHeader&	GetHeader() const				{	return mHeader;	}
	virtual void			GetDebug(TString& Debug) const	{	Debug << mHeader.GetDebug() << " " << mMeta.GetDebug();	}

public:
	TNalPacketMeta		mMeta;
	TNalPacketHeader	mHeader;
};


template<int MATRIX_SIZE>	//	16(4x4) or 64(8x8)
class TScalingList
{
public:
	TScalingList() :
		mIsPresent			( false ),
		mUseDefault			( false ),
		mMatrix				( MATRIX_SIZE )
	{
		mMatrix.SetAll(0);
	}

	bool						Init(TBitReader& BitReader);

public:
	bool							mIsPresent;
	bool							mUseDefault;
	BufferArray<int,MATRIX_SIZE>	mMatrix;
};

template<> bool TScalingList<16>::Init(TBitReader& BitReader);
template<> bool TScalingList<64>::Init(TBitReader& BitReader);


class Soy264::TNalPacket_SPS : public TNalPacket
{
public:
	TNalPacket_SPS(const TNalPacketRaw& PacketRaw);

private:
	TError::Type		Read(TBitReader& BitReader);
	TError::Type		ReadColourFormat(TBitReader& BitReader);

	virtual void		GetDebug(TString& Debug) const;

	int					GetMaxFrameNumber() const		{	return 1<<mFrameNumMaxBits;	}	//	should use -1 to get last index?
	int					GetMaxFrameNumberBits() const	{	return mFrameNumMaxBits;	}

public:
	TProfileIDC::Type	mProfileIDC;

	TChromaFormat::Type	mChromaFormat;
	bool				mSeperateChromaPlane;
	int					mFrameNumMaxBits;
	TPictureOrderCountType::Type	mPictureOrderCountType;
	int					mPictureOrderCountMaxBits;

	int bit_depth_luma_minus8;
	int bit_depth_chroma_minus8;
	int lossless_qpprime_flag;

	int constraint_set0_flag;
	int constraint_set1_flag;
	int constraint_set2_flag;
	int reserved_zero_5bits;
	int level_idc;
	int seq_parameter_set_id;
	int pic_order_cnt_type;
	int log2_max_pic_order_cnt_lsb;
	int MaxPicOrderCntLsb;
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int num_ref_frames_in_pic_order_cnt_cycle;
	int offset_for_ref_frame[256];
	int num_ref_frames;
	int gaps_in_frame_num_value_allowed_flag;
	int PicWidthInMbs;
	int PicWidthInSamples;
	int PicHeightInMapUnits;
	int PicSizeInMapUnits;
	int FrameHeightInMbs;
	int FrameHeightInSamples;
	int frame_mbs_only_flag;
	int mb_adaptive_frame_field_flag;
	int direct_8x8_inference_flag;
	int frame_cropping_flag;
	int frame_crop_left_offset;
	int frame_crop_right_offset;
	int frame_crop_top_offset;
	int frame_crop_bottom_offset;
	int vui_parameters_present_flag;

	BufferArray<TScalingList<16>,6>	mScalingList4x4;
	BufferArray<TScalingList<64>,6>	mScalingList8x8;
};

class Soy264::TNalPacket_PPS : public TNalPacket
{
public:
	TNalPacket_PPS(const TNalPacketRaw& PacketRaw);
};


class Soy264::TSliceHeader
{
public:
	TError::Type		Read(TBitReader& BitReader,const TNalPacketHeader& Packet,const TNalPacket_SPS& Sequence);

public:
	int						mFirstMbInSlice;
	TSliceType::Type		mSliceType;
	TColourPlane::Type		mColourPlane;
	TPictureStructure::Type	mPictureStructure;
	int						mFrameNumber;
	int						mIDRPicId;

	enum {
  LIST_0 = 0,
  LIST_1 = 1,
  BI_PRED = 2,
  BI_PRED_L0 = 3,
  BI_PRED_L1 = 4
};
	int						num_ref_idx_active[2];	//	number of available list references
};


class Soy264::TNalPacket_SliceIDR : public TNalPacket
{
public:
	TNalPacket_SliceIDR(const TNalPacketRaw& PacketRaw,const TNalPacket_SPS& Sequence);
	
	virtual void		GetDebug(TString& Debug) const;

public:
	TSliceHeader		mSliceHeader;
};


class Soy264::TNalPacket_SliceNonIDR : public TNalPacket
{
public:
	TNalPacket_SliceNonIDR(const TNalPacketRaw& PacketRaw,const TNalPacket_SPS& Sequence);
	
	virtual void		GetDebug(TString& Debug) const;

public:
	TSliceHeader		mSliceHeader;
};

