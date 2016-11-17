/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>
#include <string>
#include <tuple>
#include <limits>

#include "decoder.hh"
#include "frame.hh"
#include "frame_input.hh"
#include "vp8_raster.hh"
#include "ivf_writer.hh"
#include "costs.hh"
#include "enc_state_serializer.hh"
#include "file_descriptor.hh"
#include "block.hh"

enum EncoderPass
{
  FIRST_PASS,
  SECOND_PASS
};

enum EncoderQuality
{
  BEST_QUALITY,
  REALTIME_QUALITY
};

class SafeReferences
{
private:
  /* For now, we only need the Y planes to do the diamond search, so we only
     keep them in our safe references. */
  SafeRaster last_, golden_, alternative_;

public:
  SafeReferences( const uint16_t width, const uint16_t height );
  SafeReferences( const References & references );

  void update_ref( reference_frame reference_id, RasterHandle reference_raster );
  void update_all_refs( const References & references );

  // XXX remove these
  const SafeRaster & last() const { return last_; }
  const SafeRaster & golden() const { return golden_; }
  const SafeRaster & alternative() const { return alternative_; }

  const SafeRaster & get( reference_frame reference_id ) const;
};

class Encoder
{
private:
  struct MBPredictionData
  {
    mbmode prediction_mode { DC_PRED };
    MotionVector mv {};

    uint32_t rate       { std::numeric_limits<uint32_t>::max() };
    uint32_t distortion { std::numeric_limits<uint32_t>::max() };
    uint32_t cost       { std::numeric_limits<uint32_t>::max() };
  };

  struct MVSearchResult
  {
    MotionVector mv;
    size_t first_step;
  };

  typedef SafeArray<SafeArray<std::pair<uint32_t, uint32_t>,
                              MV_PROB_CNT>,
                    2> MVComponentCounts;

  IVFWriter ivf_writer_;
  uint16_t width() const { return ivf_writer_.width(); }
  uint16_t height() const { return ivf_writer_.height(); }
  MutableRasterHandle temp_raster_handle_ { width(), height() };
  DecoderState decoder_state_;
  References references_;
  SafeReferences safe_references_;

  bool has_state_ { false };

  Costs costs_ {};

  bool two_pass_encoder_ { false };
  EncoderQuality encode_quality_{ BEST_QUALITY };

  KeyFrame key_frame_;
  InterFrame inter_frame_;

  // TODO: Where did these come from?
  uint32_t RATE_MULTIPLIER { 300 };
  uint32_t DISTORTION_MULTIPLIER { 1 };

  static uint32_t rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier );

  template<unsigned int size>
  static uint32_t sad( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction );

  template<unsigned int size>
  static uint32_t variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction );

  MVSearchResult diamond_search( const VP8Raster::Macroblock & original_mb,
                                 VP8Raster::Macroblock & reconstructed_mb,
                                 VP8Raster::Macroblock & temp_mb,
                                 InterFrameMacroblock & frame_mb,
                                 const SafeRaster & reference,
                                 MotionVector base_mv,
                                 MotionVector origin,
                                 size_t step_size,
                                 const size_t y_ac_qi ) const;

  void luma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              InterFrameMacroblock & frame_mb,
                              const Quantizer & quantizer,
                              MVComponentCounts & component_counts,
                              const size_t y_ac_qi,
                              const EncoderPass encoder_pass );

  void luma_mb_apply_inter_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       InterFrameMacroblock & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode best_pred,
                                       const MotionVector best_mv );

  void chroma_mb_inter_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                InterFrameMacroblock & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  MBPredictionData luma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                 VP8Raster::Macroblock & reconstructed_mb,
                                                 VP8Raster::Macroblock & temp_mb,
                                                 MacroblockType & frame_mb,
                                                 const Quantizer & quantizer,
                                                 const EncoderPass encoder_pass = FIRST_PASS,
                                                 const bool interframe = false ) const;

  template<class MacroblockType>
  void luma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer,
                                       const mbmode min_prediction_mode,
                                       const EncoderPass encoder_pass = FIRST_PASS ) const;

  template<class MacroblockType>
  void luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                              VP8Raster::Macroblock & constructed_mb,
                              VP8Raster::Macroblock & temp_mb,
                              MacroblockType & frame_mb,
                              const Quantizer & quantizer,
                              const EncoderPass encoder_pass = FIRST_PASS ) const;

  MBPredictionData chroma_mb_best_prediction_mode( const VP8Raster::Macroblock & original_mb,
                                                   VP8Raster::Macroblock & reconstructed_mb,
                                                   VP8Raster::Macroblock & temp_mb,
                                                   const bool interframe = false ) const;

  template<class MacroblockType>
  void chroma_mb_apply_intra_prediction( const VP8Raster::Macroblock & original_mb,
                                         VP8Raster::Macroblock & reconstructed_mb,
                                         __attribute__((unused)) VP8Raster::Macroblock & temp_mb,
                                         MacroblockType & frame_mb,
                                         const Quantizer & quantizer,
                                         const mbmode min_prediction_mode,
                                         const EncoderPass encoder_pass ) const;


  template<class MacroblockType>
  void chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                VP8Raster::Macroblock & constructed_mb,
                                VP8Raster::Macroblock & temp_mb,
                                MacroblockType & frame_mb,
                                const Quantizer & quantizer,
                                const EncoderPass encoder_pass = FIRST_PASS,
                                const bool interframe = false ) const;

  bmode luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                               VP8Raster::Block4 & constructed_sb,
                               VP8Raster::Block4 & temp_sb,
                               const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const;

  void luma_sb_apply_intra_prediction( const VP8Raster::Block4 & original_sb,
                                       VP8Raster::Block4 & reconstructed_sb,
                                       YBlock & frame_sb,
                                       const Quantizer & quantizer,
                                       bmode sb_prediction_mode,
                                       const EncoderPass encoder_pass ) const;

  template<class FrameSubblockType>
  void trellis_quantize( FrameSubblockType & frame_sb,
                         const Quantizer & quantizer ) const;

  void check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const;

  VP8Raster & temp_raster() { return temp_raster_handle_.get(); }

  template<class FrameType>
  void apply_best_loopfilter_settings( const VP8Raster & original,
                                       VP8Raster & reconstructed,
                                       FrameType & frame );

  template<class FrameType>
  void optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts );

  template<class FrameHeaderType, class MacroblockType>
  void optimize_prob_skip( Frame<FrameHeaderType, MacroblockType> & frame );

  void optimize_interframe_probs( InterFrame & frame );
  void optimize_mv_probs( InterFrame & frame, const MVComponentCounts & counts );

  void update_mv_component_counts( const int16_t & component,
                                   const bool is_x,
                                   MVComponentCounts & counts ) const;

  static unsigned calc_prob( unsigned false_count, unsigned total );

  template<class FrameType>
  static FrameType make_empty_frame( const uint16_t width, const uint16_t height,
                                     const bool show_frame );

  template<class FrameType>
  void write_frame( const FrameType & frame );

  template<class FrameType>
  void write_frame( const FrameType & frame, const ProbabilityTables & prob_tables );

  /* Convergence-related stuff */
  template<class FrameType>
  InterFrame & reencode_as_interframe( const VP8Raster & unfiltered_output,
                                       const FrameType & original_frame,
                                       const QuantIndices & quant_indices );

  InterFrame & update_residues( const VP8Raster & unfiltered_output,
                                const InterFrame & original_frame,
                                const QuantIndices & quant_indices,
                                const bool last_frame );

  void update_macroblock( const VP8Raster::Macroblock & original_rmb,
                          VP8Raster::Macroblock & reconstructed_rmb,
                          VP8Raster::Macroblock & temp_mb,
                          InterFrameMacroblock & frame_mb,
                          const InterFrameMacroblock & original_fmb,
                          const Quantizer & quantizer );

  template<class FrameType>
  void update_decoder_state( const FrameType & frame );

  template<class FrameType>
  std::pair<FrameType &, double> encode_raster( const VP8Raster & raster,
                                                const QuantIndices & quant_indices,
                                                const bool update_state = false,
                                                const bool compute_ssim = false );

  template<class FrameType>
  FrameType & encode_with_quantizer_search( const VP8Raster & raster,
                                            const double minimum_ssim );

public:
  Encoder( IVFWriter && output, const bool two_pass,
           const EncoderQuality quality );

  Encoder( const Decoder & decoder, IVFWriter && output, const bool two_pass,
           const EncoderQuality quality );

  void encode_with_minimum_ssim( const VP8Raster & raster,
                                 const double minimum_ssim );

  void encode_with_quantizer( const VP8Raster & raster,
                              const uint8_t y_ac_qi );

  void reencode( const std::vector<RasterHandle> & original_rasters,
                 const std::vector<std::pair<Optional<KeyFrame>, Optional<InterFrame> > > & prediction_frames,
                 const double kf_q_weight,
                 const bool extra_frame_chunk );

  Decoder export_decoder() const { return { decoder_state_, references_ }; }

  void set_expected_decoder_entry_hash( const uint32_t minihash )
  {
    ivf_writer_.set_expected_decoder_entry_hash( minihash );
  }
};

#endif /* ENCODER_HH */
