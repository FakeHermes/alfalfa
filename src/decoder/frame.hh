#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"
#include "uncompressed_chunk.hh"
#include "modemv_data.hh"
#include "raster.hh"

struct References;

struct Quantizers
{
  Quantizer quantizer;
  SafeArray< Quantizer, num_segments > segment_quantizers;
};

template <class FrameHeaderType, class MacroblockType>
class Frame
{
 private:
  unsigned int display_width_, display_height_;
  unsigned int macroblock_width_ { Raster::macroblock_dimension( display_width_ ) },
    macroblock_height_ { Raster::macroblock_dimension( display_height_ ) };

  TwoD< Y2Block > Y2_ { macroblock_width_, macroblock_height_ };
  TwoD< YBlock > Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD< UVBlock > U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD< UVBlock > V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  BoolDecoder first_partition_;
  FrameHeaderType header_ { first_partition_ };

  std::vector< BoolDecoder > dct_partitions_;

  Optional< TwoD< MacroblockType > > macroblock_headers_ {};

  void relink_y2_blocks( void );

  void loopfilter( const DecoderState & decoder_state, Raster & target ) const;

 public:
  Frame( const UncompressedChunk & chunk,
	 const unsigned int width,
	 const unsigned int height );

  const FrameHeaderType & header( void ) const { return header_; }

  void parse_macroblock_headers( const DecoderState & decoder_state );
  void parse_tokens( const DecoderState & decoder_state );

  void decode( const DecoderState & decoder_state, Raster & target ) const;
  void decode( const DecoderState & decoder_state, const References & references, Raster & target ) const;
};

using KeyFrame = Frame< KeyFrameHeader, KeyFrameMacroblock >;
using InterFrame = Frame< InterFrameHeader, InterFrameMacroblock >;

#endif /* FRAME_HH */
