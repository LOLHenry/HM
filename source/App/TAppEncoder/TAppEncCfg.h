/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2026, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TAppEncCfg.h
    \brief    Handle encoder configuration parameters (header)
*/

#ifndef __TAPPENCCFG__
#define __TAPPENCCFG__

#include "TLibCommon/CommonDef.h"
#include "Utilities/program_options_lite.h"

#include "TLibEncoder/TEncCfg.h"
#if EXTENSION_360_VIDEO
#include "TAppEncHelper360/TExt360AppEncCfg.h"
#endif
#include <sstream>
#include <vector>

namespace po = df::program_options_lite;

//! \ingroup TAppEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

#if NH_MV
enum UIProfileName // this is used for determining profile strings, where multiple profiles map to a single profile idc with various constraint flag combinations
{
  UI_NONE = 0,
  UI_MAIN = 1,
  UI_MAIN10 = 2,
  UI_MAIN10_STILL_PICTURE=10002,
  UI_MAINSTILLPICTURE = 3,
  UI_MAINREXT = 4,
  UI_HIGHTHROUGHPUTREXT = 5,
  // The following are RExt profiles, which would map to the MAINREXT profile idc.
  // The enumeration indicates the bit-depth constraint in the bottom 2 digits
  //                           the chroma format in the next digit
  //                           the intra constraint in the next digit (1 for no intra constraint, 2 for intra constraint)
  //                           If it is a RExt still picture, there is a '1' for the top digit.
#if NH_MV
  UI_MULTIVIEWMAIN     = 6,
#if JVET_AH0046
  UI_MULTIVIEWEXTENDED    = 12,
  UI_MULTIVIEWEXTENDED10  = 13,
#else
#if JVET_AE0295
  UI_MULTIVIEWMAIN10     = 99,
#endif//JVET_AE0295
#endif  // JVET_AE0046
#if JVET_AM1018
  UI_MULTIVIEWREXT  = 14,
#endif //  JVET_AM1018
#endif
  UI_MONOCHROME_8      = 1008,
  UI_MONOCHROME_12     = 1012,
  UI_MONOCHROME_16     = 1016,
  UI_MAIN_12           = 1112,
  UI_MAIN_422_10       = 1210,
  UI_MAIN_422_12       = 1212,
  UI_MAIN_444          = 1308,
  UI_MAIN_444_10       = 1310,
  UI_MAIN_444_12       = 1312,
  UI_MAIN_444_16       = 1316, // non-standard profile definition, used for development purposes
  UI_MAIN_INTRA        = 2108,
  UI_MAIN_10_INTRA     = 2110,
  UI_MAIN_12_INTRA     = 2112,
  UI_MAIN_422_10_INTRA = 2210,
  UI_MAIN_422_12_INTRA = 2212,
  UI_MAIN_444_INTRA    = 2308,
  UI_MAIN_444_10_INTRA = 2310,
  UI_MAIN_444_12_INTRA = 2312,
  UI_MAIN_444_16_INTRA = 2316,
  UI_MAIN_444_STILL_PICTURE = 11308,
  UI_MAIN_444_16_STILL_PICTURE = 12316,
  // The following are high throughput profiles, which would map to the HIGHTHROUGHPUTREXT profile idc.
  // The enumeration indicates the bit-depth constraint in the bottom 2 digits
  //                           the chroma format in the next digit
  //                           the intra constraint in the next digit
  //                           There is a '2' for the top digit to indicate it is high throughput profile

  UI_HIGHTHROUGHPUT_444     = 21308,
  UI_HIGHTHROUGHPUT_444_10  = 21310,
  UI_HIGHTHROUGHPUT_444_14  = 21314,
  UI_HIGHTHROUGHPUT_444_16_INTRA  = 22316
#if NH_MV_ALLOW_NON_CONFORMING
  ,
  UI_MULTIVIEWMAIN_NONCONF     = 10000006,
#endif
};
#endif

/// encoder configuration class
class TAppEncCfg
{
public:
  template <class T>
  struct OptionalValue
  {
    Bool bPresent;
    T    value;
    OptionalValue() : bPresent(false), value() { }
  };

protected:
  // file I/O
#if NH_MV
  std::vector<TChar*>     m_pchInputFileList;                  ///< source file names
#else
  std::string m_inputFileName;                                ///< source file name
#endif
  std::string m_bitstreamFileName;                            ///< output bitstream file
#if NH_MV
  std::vector<char*>     m_pchReconFileList;                  ///< output reconstruction file names
  Int                    m_numberOfLayers;                    ///< number of Layers to Encode
  Int                    m_iNumberOfViews;                    ///< number of Layers that are views
#if NH_MV
  Bool                   m_shareParameterSets;
  IntAry1d               m_layerIdxInVpsToGopDefIdx;
  IntAry1d               m_layerIdxInVpsToRepFormatIdx;
  Int                    m_numRepFormats;
  Int                    m_numInputOutputRepFormats;
#endif
#else
  std::string m_reconFileName;                                ///< output reconstruction file
#endif
#if SHUTTER_INTERVAL_SEI_PROCESSING
  Bool        m_ShutterFilterEnable;                          ///< enable Pre-Filtering with Shutter Interval SEI
  std::string m_shutterIntervalPreFileName;                   ///< output Pre-Filtering video
#endif

#if NH_MV
// VPS specification
IntAry2d m_dimIds;                   ///< dimension ids ( pointers to m_viewId and m_depthFlag
IntAry1d               m_viewId;                            ///< view id
IntAry1d               m_viewOrderIndex;                    ///< view order index
IntAry1d               m_auxId;                             ///< auxiliary id

IntAry1d               m_targetEncLayerIdList;              ///< layer Ids in Nuh to be encoded
IntAry1d               m_layerIdInNuh;                      ///< layer Id in Nuh for each layer
Bool                   m_splittingFlag;                     ///< Splitting Flag
Int                    m_scalabilityMask;                   ///< Mask indicating scalabilities, 1: texture; 3: texture + depth
IntAry1d               m_dimensionIdLen;                    ///< Length of scalability dimension s

// layer sets
Int                    m_vpsNumLayerSets;                   ///< Number of layer sets
IntAry2d               m_layerIdxInVpsInSets;               ///< LayerIds in vps of layer set
Int                    m_numAddLayerSets;                   ///< Number of additional layer sets
IntAry2d               m_highestLayerIdxPlus1;              ///< HighestLayerIdxPlus1 for each additional layer set and each independent layer (value with index 0 will be ignored)
Int                    m_defaultOutputLayerIdc;             ///< Specifies output layers of layer sets, 0: output all layers, 1: output highest layers, 2: specified by LayerIdsInDefOuputLayerSet
IntAry1d               m_outputLayerSetIdx;                 ///< Indices of layer sets used as additional output layer sets
IntAry2d               m_layerIdsInAddOutputLayerSet;       ///< LayerIds in vps of additional output layers
IntAry2d               m_layerIdsInDefOutputLayerSet;       ///< Indices in vps of output layers in layer sets
IntAry2d               m_profileTierLevelIdx;               ///< Indices of of profile, per layer in layer set
BoolAry1d              m_altOutputLayerFlag;                ///< Alt output layer flag

// Dependencies
IntAry2d m_directRefLayers;          ///< LayerIds of direct reference layers
IntAry2d m_dependencyTypes;          ///< Dependency types of direct reference layers

// VPS VUI
Bool m_vpsVuiPresentFlag;
Bool m_crossLayerPicTypeAlignedFlag;
Bool m_crossLayerIrapAlignedFlag;
Bool m_allLayersIdrAlignedFlag;
Bool m_bitRatePresentVpsFlag;
Bool m_picRatePresentVpsFlag;
BoolAry2d m_bitRatePresentFlag;
BoolAry2d m_picRatePresentFlag;
IntAry2d  m_avgBitRate;
IntAry2d  m_maxBitRate;
IntAry2d  m_constantPicRateIdc;
IntAry2d  m_avgPicRate;
Bool      m_tilesNotInUseFlag;
BoolAry1d m_tilesInUseFlag;
BoolAry1d m_loopFilterNotAcrossTilesFlag;
Bool      m_wppNotInUseFlag;
BoolAry1d m_wppInUseFlag;

BoolAry2d m_tileBoundariesAlignedFlag;
Bool      m_ilpRestrictedRefLayersFlag;
IntAry2d  m_minSpatialSegmentOffsetPlus1;
BoolAry2d m_ctuBasedOffsetEnabledFlag;
IntAry2d  m_minHorizontalCtuOffsetPlus1;
Bool      m_singleLayerForNonIrapFlag;
Bool      m_higherLayerIrapSkipFlag;
#endif

  // Lambda modifiers
  Double    m_adLambdaModifier[ MAX_TLAYER ];                 ///< Lambda modifier array for each temporal layer
  std::vector<Double> m_adIntraLambdaModifier;                ///< Lambda modifier for Intra pictures, one for each temporal layer. If size>temporalLayer, then use [temporalLayer], else if size>0, use [size()-1], else use m_adLambdaModifier.
  Double    m_dIntraQpFactor;                                 ///< Intra Q Factor. If negative, use a default equation: 0.57*(1.0 - Clip3( 0.0, 0.5, 0.05*(Double)(isField ? (GopSize-1)/2 : GopSize-1) ))

  // source specification
  Int       m_iFrameRate;                                     ///< source frame-rates (Hz)
  UInt      m_FrameSkip;                                      ///< number of skipped frames from the beginning
  UInt      m_temporalSubsampleRatio;                         ///< temporal subsample ratio, 2 means code every two frames
#if NH_MV
  IntAry1d       m_iSourceWidths   ;                                   ///< source width in pixel
  IntAry1d       m_iSourceHeights  ;                                  ///< source height in pixel (when interlaced = field height)
  IntAry1d       m_inputFileWidths ;                                 ///< width of image in input file  (this is equivalent to sourceWidth,  if sourceWidth  is not subsequently altered due to padding)
  IntAry1d       m_inputFileHeights;                                ///< height of image in input file (this is equivalent to sourceHeight, if sourceHeight is not subsequently altered due to padding)

  IntAry1d       m_iSourceHeightOrgs;                               ///< original source height in pixel (when interlaced = frame height)
#else
  Int       m_sourceWidth;                                    ///< source width in pixel
  Int       m_sourceHeight;                                   ///< source height in pixel (when interlaced = field height)
  Int       m_inputFileWidth;                                 ///< width of image in input file  (this is equivalent to sourceWidth,  if sourceWidth  is not subsequently altered due to padding)
  Int       m_inputFileHeight;                                ///< height of image in input file (this is equivalent to sourceHeight, if sourceHeight is not subsequently altered due to padding)

  Int       m_sourceHeightOrg;                                ///< original source height in pixel (when interlaced = frame height)
#endif

  Bool      m_isField;                                        ///< enable field coding
  Bool      m_isTopFieldFirst;
  Bool      m_bEfficientFieldIRAPEnabled;                     ///< enable an efficient field IRAP structure.
  Bool      m_bHarmonizeGopFirstFieldCoupleEnabled;

  Int       m_conformanceWindowMode;
#if NH_MV
  IntAry1d  m_confWinLefts  ;
  IntAry1d  m_confWinRights ;
  IntAry1d  m_confWinTops   ;
  IntAry1d  m_confWinBottoms;
#else
  Int       m_confWinLeft;
  Int       m_confWinRight;
  Int       m_confWinTop;
  Int       m_confWinBottom;
#endif

#if NH_MV
  IntAry2d  m_aiPads;                                         ///< number of padded pixels for
#else
  Int       m_sourcePadding[2];                                       ///< number of padded pixels for width and height
#endif
  Int       m_framesToBeEncoded;                              ///< number of encoded frames
  Bool      m_AccessUnitDelimiter;                            ///< add Access Unit Delimiter NAL units
  InputColourSpaceConversion m_inputColourSpaceConvert;       ///< colour space conversion to apply to input video
  Bool      m_snrInternalColourSpace;                       ///< if true, then no colour space conversion is applied for snr calculation, otherwise inverse of input is applied.
  Bool      m_outputInternalColourSpace;                    ///< if true, then no colour space conversion is applied for reconstructed video, otherwise inverse of input is applied.
#if NH_MV
  std::vector<ChromaFormat> m_InputChromaFormatIDC;
#else
  ChromaFormat m_InputChromaFormatIDC;
#endif

  Bool      m_printMSEBasedSequencePSNR;
  Bool      m_printHexPsnr;
  Bool      m_printFrameMSE;
  Bool      m_printSequenceMSE;
  Bool      m_printMSSSIM;

  Bool      m_bXPSNREnableFlag;                              ///< xPSNR enable flag
  Double    m_dXPSNRWeight[MAX_NUM_COMPONENT];               ///< xPSNR per component weights

  Bool      m_cabacZeroWordPaddingEnabled;
  Bool      m_bClipInputVideoToRec709Range;
  Bool      m_bClipOutputVideoToRec709Range;

  // profile/level
#if NH_MV
  std::vector< Profile::Name > m_profiles;
  
  std::vector<UIProfileName>   m_uiProfiles;

  std::vector< Level::Tier   > m_levelTier;
  std::vector< Level::Name   > m_level;
  std::vector< Bool          > m_inblFlag;

  IntAry1d                    m_bitDepthConstraints          ;
  IntAry1d                    m_tmpConstraintChromaFormats   ;
  std::vector< ChromaFormat > m_chromaFormatConstraints      ;
  BoolAry1d                   m_intraConstraintFlags         ;
  BoolAry1d                   m_onePictureOnlyConstraintFlags;
  BoolAry1d                   m_lowerBitRateConstraintFlags  ;
  BoolAry1d                   m_progressiveSourceFlags       ;
  BoolAry1d                   m_interlacedSourceFlags        ;
  BoolAry1d                   m_nonPackedConstraintFlags     ;
  BoolAry1d                   m_frameOnlyConstraintFlags     ;

#else
  Profile::Name m_profile;
  Level::Tier   m_levelTier;
  Level::Name   m_level;
  UInt          m_bitDepthConstraint;
  ChromaFormat  m_chromaFormatConstraint;
  Bool          m_intraConstraintFlag;
  Bool          m_onePictureOnlyConstraintFlag;
  Bool          m_lowerBitRateConstraintFlag;
  Bool m_progressiveSourceFlag;
  Bool m_interlacedSourceFlag;
  Bool m_nonPackedConstraintFlag;
  Bool m_frameOnlyConstraintFlag;
#endif

  // coding structure
#if NH_MV
  IntAry1d  m_iIntraPeriod;                            ///< period of I-slice (random access period)
#else
  Int       m_iIntraPeriod;                                   ///< period of I-slice (random access period)
#endif
  Int       m_iDecodingRefreshType;                           ///< random access type
  Int       m_iGOPSize;                                       ///< GOP size of hierarchical structure
  Bool      m_bReWriteParamSetsFlag;                          ///< Flag to enable rewriting of parameter sets at random access points
#if NH_MV
  Int       m_extraRPSsMvc[MAX_NUM_LAYERS];                       ///< extra RPSs added to handle CRA for each layer
  std::vector< GOPEntry* >  m_GOPListMvc;                            ///< the coding structure entries from the config file for each layer
  Int       m_numReorderPicsMvc[MAX_NUM_LAYERS][MAX_TLAYER];      ///< total number of reorder pictures for each layer
  Int       m_maxDecPicBufferingMvc[MAX_NUM_LAYERS][MAX_TLAYER];  ///< total number of reference pictures needed for decoding for each layer
#else
  Int       m_extraRPSs;                                      ///< extra RPSs added to handle CRA
  GOPEntry  m_GOPList[MAX_GOP];                               ///< the coding structure entries from the config file
  Int       m_numReorderPics[MAX_TLAYER];                     ///< total number of reorder pictures
  Int       m_maxDecPicBuffering[MAX_TLAYER];                 ///< total number of pictures in the decoded picture buffer
#endif
  Bool      m_crossComponentPredictionEnabledFlag;            ///< flag enabling the use of cross-component prediction
  Bool      m_reconBasedCrossCPredictionEstimate;             ///< causes the alpha calculation in encoder search to be based on the decoded residual rather than the pre-transform encoder-side residual
#if NH_MV
  UInt      m_log2SaoOffsetScale[MAX_NUM_LAYERS][MAX_NUM_CHANNEL_TYPE];        ///< number of bits for the upward bit shift operation on the decoded SAO offsets
#else
  UInt      m_log2SaoOffsetScale[MAX_NUM_CHANNEL_TYPE];       ///< number of bits for the upward bit shift operation on the decoded SAO offsets
#endif
  Bool      m_useTransformSkip;                               ///< flag for enabling intra transform skipping
  Bool      m_useTransformSkipFast;                           ///< flag for enabling fast intra transform skipping
  UInt      m_log2MaxTransformSkipBlockSize;                  ///< transform-skip maximum size (minimum of 2)
  Bool      m_transformSkipRotationEnabledFlag;               ///< control flag for transform-skip/transquant-bypass residual rotation
  Bool      m_transformSkipContextEnabledFlag;                ///< control flag for transform-skip/transquant-bypass single significance map context
  Bool      m_rdpcmEnabledFlag[NUMBER_OF_RDPCM_SIGNALLING_MODES];///< control flags for residual DPCM
  Bool      m_enableAMP;
  Bool      m_persistentRiceAdaptationEnabledFlag;            ///< control flag for Golomb-Rice parameter adaptation over each slice
  Bool      m_cabacBypassAlignmentEnabledFlag;

  // coding quality
#if NH_MV
  std::vector<Int>   m_qpIncrementAtSourceFrame;             ///< Optional source frame number at which all subsequent frames are to use an increased internal QP.
  std::vector<Int>     m_iQP;                                 ///< QP value of key-picture (integer) for each layer
#else
  OptionalValue<UInt> m_qpIncrementAtSourceFrame;             ///< Optional source frame number at which all subsequent frames are to use an increased internal QP.
  Int       m_iQP;                                            ///< QP value of key-picture (integer)
#endif
  Int       m_intraQPOffset;                                  ///< QP offset for intra slice (integer)
  Bool      m_lambdaFromQPEnable;                             ///< enable flag for QP:lambda fix
  std::string m_dQPFileName;                                  ///< QP offset for each slice (initialized from external file)
#if NH_MV
  std::vector<Int*> m_aidQP;                                    ///< array of slice QP values for each layer
#else
  Int*      m_aidQP;                                          ///< array of slice QP values
#endif
  Int       m_iMaxDeltaQP;                                    ///< max. |delta QP|
  UInt      m_uiDeltaQpRD;                                    ///< dQP range for multi-pass slice QP optimization
  Int       m_iMaxCuDQPDepth;                                 ///< Max. depth for a minimum CuDQPSize (0:default)
  Int       m_diffCuChromaQpOffsetDepth;                      ///< If negative, then do not apply chroma qp offsets.
  Bool      m_bFastDeltaQP;                                   ///< Fast Delta QP (false:default)

  Int       m_cbQpOffset;                                     ///< Chroma Cb QP Offset (0:default)
  Int       m_crQpOffset;                                     ///< Chroma Cr QP Offset (0:default)
  WCGChromaQPControl m_wcgChromaQpControl;                    ///< Wide-colour-gamut chroma QP control.
  UInt      m_sliceChromaQpOffsetPeriodicity;                 ///< Used in conjunction with Slice Cb/Cr QpOffsetIntraOrPeriodic. Use 0 (default) to disable periodic nature.
  Int       m_sliceChromaQpOffsetIntraOrPeriodic[2/*Cb,Cr*/]; ///< Chroma Cb QP Offset at slice level for I slice or for periodic inter slices as defined by SliceChromaQPOffsetPeriodicity. Replaces offset in the GOP table.
  LumaLevelToDeltaQPMapping m_lumaLevelToDeltaQPMapping;      ///< mapping from luma level to Delta QP.
#if ADAPTIVE_QP_SELECTION
  Bool      m_bUseAdaptQpSelect;
#endif
#if JVET_V0078
  Bool      m_bSmoothQPReductionEnable;
  Double    m_dSmoothQPReductionThreshold;
  Double    m_dSmoothQPReductionModelScale;
  Double    m_dSmoothQPReductionModelOffset;
  Int       m_iSmoothQPReductionPeriodicity;
  Int       m_iSmoothQPReductionLimit;
#endif
  TComSEIMasteringDisplay m_masteringDisplay;
#if NH_MV
  std::vector<char*>     m_seiCfgFileNames;               ///< SEI message files.
  SEIMessages            m_seiMessages;                       ///< Buffer for SEI messages. 
#endif

  Bool      m_bUseAdaptiveQP;                                 ///< Flag for enabling QP adaptation based on a psycho-visual model
  Int       m_iQPAdaptationRange;                             ///< dQP range by QP adaptation

#if NH_MV
  Int       m_maxTempLayerMvc[MAX_NUM_LAYER_IDS];             ///< Max temporal layer for each layer
#else
  Int       m_maxTempLayer;                                  ///< Max temporal layer
#endif

  // coding unit (CU) definition
  // TODO: Remove MaxCUWidth/MaxCUHeight and replace with MaxCUSize.
  UInt      m_uiMaxCUWidth;                                   ///< max. CU width in pixel
  UInt      m_uiMaxCUHeight;                                  ///< max. CU height in pixel
  UInt      m_uiMaxCUDepth;                                   ///< max. CU depth (as specified by command line)
#if NH_MV
  IntAry1d  m_uiMaxTotalCUDepth;                              ///< max. total CU depth - includes depth of transform-block structure
#else
  UInt      m_uiMaxTotalCUDepth;                              ///< max. total CU depth - includes depth of transform-block structure
#endif
  UInt      m_uiLog2DiffMaxMinCodingBlockSize;                ///< difference between largest and smallest CU depth

  // transfom unit (TU) definition
  UInt      m_uiQuadtreeTULog2MaxSize;
  UInt      m_uiQuadtreeTULog2MinSize;

  UInt      m_uiQuadtreeTUMaxDepthInter;
  UInt      m_uiQuadtreeTUMaxDepthIntra;

  // coding tools (bit-depth)
#if NH_MV
  IntAry2d  m_inputBitDepths      ;         ///< bit-depth of input file
  IntAry2d  m_outputBitDepths     ;         ///< bit-depth of output file
  IntAry2d  m_MSBExtendedBitDepths;         ///< bit-depth of input samples after MSB extension
  IntAry2d  m_internalBitDepths   ;         ///< bit-depth codec operates at (input/output files will be converted)
#else
  Int       m_inputBitDepth   [MAX_NUM_CHANNEL_TYPE];         ///< bit-depth of input file
  Int       m_outputBitDepth  [MAX_NUM_CHANNEL_TYPE];         ///< bit-depth of output file
  Int       m_MSBExtendedBitDepth[MAX_NUM_CHANNEL_TYPE];      ///< bit-depth of input samples after MSB extension
  Int       m_internalBitDepth[MAX_NUM_CHANNEL_TYPE];         ///< bit-depth codec operates at (input/output files will be converted)
#endif

  Bool      m_extendedPrecisionProcessingFlag;
  Bool      m_highPrecisionOffsetsEnabledFlag;

  //coding tools (chroma format)
#if NH_MV
  std::vector<ChromaFormat> m_chromaFormatIDCs;
#else
  ChromaFormat m_chromaFormatIDC;
#endif

  // coding tools (PCM bit-depth)
  Bool      m_bPCMInputBitDepthFlag;                          ///< 0: PCM bit-depth is internal bit-depth. 1: PCM bit-depth is input bit-depth.

  // coding tool (SAO)
#if NH_MV
  std::vector<Bool> m_bUseSAO; 
#else
  Bool      m_bUseSAO;
#endif
  Bool      m_bTestSAODisableAtPictureLevel;
  Double    m_saoEncodingRate;                                ///< When >0 SAO early picture termination is enabled for luma and chroma
  Double    m_saoEncodingRateChroma;                          ///< The SAO early picture termination rate to use for chroma (when m_SaoEncodingRate is >0). If <=0, use results for luma.
  Int       m_maxNumOffsetsPerPic;                            ///< SAO maximun number of offset per picture
  Bool      m_saoCtuBoundary;                                 ///< SAO parameter estimation using non-deblocked pixels for CTU bottom and right boundary areas
  Bool      m_resetEncoderStateAfterIRAP;                     ///< When true, encoder state will be reset following an IRAP.
  // coding tools (loop filter)
#if NH_MV
  std::vector<Bool> m_bLoopFilterDisable;                     ///< flag for using deblocking filter for each layer
#else
  Bool      m_bLoopFilterDisable;                             ///< flag for using deblocking filter
#endif
  Bool      m_loopFilterOffsetInPPS;                          ///< offset for deblocking filter in 0 = slice header, 1 = PPS
  Int       m_loopFilterBetaOffsetDiv2;                       ///< beta offset for deblocking filter
  Int       m_loopFilterTcOffsetDiv2;                         ///< tc offset for deblocking filter
  Int       m_deblockingFilterMetric;                         ///< blockiness metric in encoder
  // coding tools (PCM)
  Bool      m_usePCM;                                         ///< flag for using IPCM
  UInt      m_pcmLog2MaxSize;                                 ///< log2 of maximum PCM block size
  UInt      m_uiPCMLog2MinSize;                               ///< log2 of minimum PCM block size
  Bool      m_bPCMFilterDisableFlag;                          ///< PCM filter disable flag
  Bool      m_enableIntraReferenceSmoothing;                  ///< flag for enabling(default)/disabling intra reference smoothing/filtering

  // coding tools (encoder-only parameters)
  Bool      m_bUseASR;                                        ///< flag for using adaptive motion search range
  Bool      m_bUseHADME;                                      ///< flag for using HAD in sub-pel ME
  Bool      m_useRDOQ;                                        ///< flag for using RD optimized quantization
  Bool      m_useRDOQTS;                                      ///< flag for using RD optimized quantization for transform skip
  Bool      m_useSelectiveRDOQ;                               ///< flag for using selective RDOQ
  Int       m_rdPenalty;                                      ///< RD-penalty for 32x32 TU for intra in non-intra slices (0: no RD-penalty, 1: RD-penalty, 2: maximum RD-penalty)
  Bool      m_bDisableIntraPUsInInterSlices;                  ///< Flag for disabling intra predicted PUs in inter slices.
  MESearchMethod m_motionEstimationSearchMethod;
  Bool      m_bRestrictMESampling;                            ///< Restrict sampling for the Selective ME
  Int       m_iSearchRange;                                   ///< ME search range
  Int       m_bipredSearchRange;                              ///< ME search range for bipred refinement
  Int       m_minSearchWindow;                                ///< ME minimum search window size for the Adaptive Window ME
  Bool      m_bClipForBiPredMeEnabled;                        ///< Enables clipping for Bi-Pred ME.
  Bool      m_bFastMEAssumingSmootherMVEnabled;               ///< Enables fast ME assuming a smoother MV.
#if NH_MV
  Bool      m_bUseDisparitySearchRangeRestriction;            ///< restrict vertical search range for inter-view prediction
  Int       m_iVerticalDisparitySearchRange;                  ///< ME vertical search range for inter-view prediction
#endif
  FastInterSearchMode m_fastInterSearchMode;                  ///< Parameter that controls fast encoder settings
  Bool      m_bUseEarlyCU;                                    ///< flag for using Early CU setting
  Bool      m_useFastDecisionForMerge;                        ///< flag for using Fast Decision Merge RD-Cost
  Bool      m_bUseCbfFastMode;                                ///< flag for using Cbf Fast PU Mode Decision
  Bool      m_useEarlySkipDetection;                          ///< flag for using Early SKIP Detection
  SliceConstraint m_sliceMode;
  Int             m_sliceArgument;                            ///< argument according to selected slice mode
  SliceConstraint m_sliceSegmentMode;
  Int             m_sliceSegmentArgument;                     ///< argument according to selected slice segment mode

  Bool      m_bLFCrossSliceBoundaryFlag;  ///< 1: filter across slice boundaries 0: do not filter across slice boundaries
  Bool      m_bLFCrossTileBoundaryFlag;   ///< 1: filter across tile boundaries  0: do not filter across tile boundaries
  Bool      m_tileUniformSpacingFlag;
  Int       m_numTileColumnsMinus1;
  Int       m_numTileRowsMinus1;
  std::vector<Int> m_tileColumnWidth;
  std::vector<Int> m_tileRowHeight;
  Bool      m_entropyCodingSyncEnabledFlag;

  Bool      m_bUseConstrainedIntraPred;                       ///< flag for using constrained intra prediction
  Bool      m_bFastUDIUseMPMEnabled;
  Bool      m_bFastMEForGenBLowDelayEnabled;
  Bool      m_bUseBLambdaForNonKeyLowDelayPictures;

  HashType  m_decodedPictureHashSEIType;                      ///< Checksum mode for decoded picture hash SEI message
  Bool      m_recoveryPointSEIEnabled;
  Bool      m_bufferingPeriodSEIEnabled;
  Bool      m_pictureTimingSEIEnabled;
  Bool      m_toneMappingInfoSEIEnabled;
  Bool      m_chromaResamplingFilterSEIenabled;
  Int       m_chromaResamplingHorFilterIdc;
  Int       m_chromaResamplingVerFilterIdc;
  Int       m_toneMapId;
  Bool      m_toneMapCancelFlag;
  Bool      m_toneMapPersistenceFlag;
  Int       m_toneMapCodedDataBitDepth;
  Int       m_toneMapTargetBitDepth;
  Int       m_toneMapModelId;
  Int       m_toneMapMinValue;
  Int       m_toneMapMaxValue;
  Int       m_sigmoidMidpoint;
  Int       m_sigmoidWidth;
  Int       m_numPivots;
  Int       m_cameraIsoSpeedIdc;
  Int       m_cameraIsoSpeedValue;
  Int       m_exposureIndexIdc;
  Int       m_exposureIndexValue;
  Bool      m_exposureCompensationValueSignFlag;
  Int       m_exposureCompensationValueNumerator;
  Int       m_exposureCompensationValueDenomIdc;
  Int       m_refScreenLuminanceWhite;
  Int       m_extendedRangeWhiteLevel;
  Int       m_nominalBlackLevelLumaCodeValue;
  Int       m_nominalWhiteLevelLumaCodeValue;
  Int       m_extendedWhiteLevelLumaCodeValue;
  Int*      m_startOfCodedInterval;
  Int*      m_codedPivotValue;
  Int*      m_targetPivotValue;
  Bool      m_framePackingSEIEnabled;
  Int       m_framePackingSEIType;
  Int       m_framePackingSEIId;
  Int       m_framePackingSEIQuincunx;
  Int       m_framePackingSEIInterpretation;
  Bool      m_segmentedRectFramePackingSEIEnabled;
  Bool      m_segmentedRectFramePackingSEICancel;
  Int       m_segmentedRectFramePackingSEIType;
  Bool      m_segmentedRectFramePackingSEIPersistence;
  Int       m_displayOrientationSEIAngle;
  Bool      m_temporalLevel0IndexSEIEnabled;
  Bool      m_gradualDecodingRefreshInfoEnabled;
  Int       m_noDisplaySEITLayer;
  Bool      m_decodingUnitInfoSEIEnabled;
  Bool      m_SOPDescriptionSEIEnabled;
  Bool      m_scalableNestingSEIEnabled;
  Bool      m_tmctsSEIEnabled;
#if MCTS_ENC_CHECK
  Bool      m_tmctsSEITileConstraint;
#endif
#if MCTS_EXTRACTION
  Bool      m_tmctsExtractionSEIEnabled;
#endif
  Bool      m_timeCodeSEIEnabled;
  Int       m_timeCodeSEINumTs;
  TComSEITimeSet m_timeSetArray[MAX_TIMECODE_SEI_SETS];
  Bool      m_kneeSEIEnabled;
  TEncCfg::TEncSEIKneeFunctionInformation m_kneeFunctionInformationSEI;
  Int       m_preferredTransferCharacteristics;
  UInt      m_greenMetadataType;
  UInt      m_xsdMetricType;
  Bool    m_ccvSEIEnabled;
  Bool    m_ccvSEICancelFlag;
  Bool    m_ccvSEIPersistenceFlag;
  Bool    m_ccvSEIPrimariesPresentFlag;
  Bool    m_ccvSEIMinLuminanceValuePresentFlag;
  Bool    m_ccvSEIMaxLuminanceValuePresentFlag;
  Bool    m_ccvSEIAvgLuminanceValuePresentFlag;
  Double  m_ccvSEIPrimariesX[MAX_NUM_COMPONENT]; // Int(32)
  Double  m_ccvSEIPrimariesY[MAX_NUM_COMPONENT]; // Int(32)
  Double  m_ccvSEIMinLuminanceValue;             // UInt(32)
  Double  m_ccvSEIMaxLuminanceValue;             // UInt(32)
  Double  m_ccvSEIAvgLuminanceValue;             // UInt(32)
  Bool      m_erpSEIEnabled;          
  Bool      m_erpSEICancelFlag;
  Bool      m_erpSEIPersistenceFlag;
  Bool      m_erpSEIGuardBandFlag;
  UInt      m_erpSEIGuardBandType;
  UInt      m_erpSEILeftGuardBandWidth;
  UInt      m_erpSEIRightGuardBandWidth;

  Bool      m_sphereRotationSEIEnabled;          
  Bool      m_sphereRotationSEICancelFlag;
  Bool      m_sphereRotationSEIPersistenceFlag;
  Int       m_sphereRotationSEIYaw;
  Int       m_sphereRotationSEIPitch;
  Int       m_sphereRotationSEIRoll;

  Bool      m_omniViewportSEIEnabled;          
  UInt      m_omniViewportSEIId;
  Bool      m_omniViewportSEICancelFlag;
  Bool      m_omniViewportSEIPersistenceFlag;
  UInt      m_omniViewportSEICntMinus1;
  std::vector<Int>  m_omniViewportSEIAzimuthCentre;
  std::vector<Int>  m_omniViewportSEIElevationCentre;
  std::vector<Int>  m_omniViewportSEITiltCentre;
  std::vector<UInt> m_omniViewportSEIHorRange;
  std::vector<UInt> m_omniViewportSEIVerRange;  
  Bool                  m_cmpSEIEnabled;
  Bool                  m_cmpSEICmpCancelFlag;
  Bool                  m_cmpSEICmpPersistenceFlag;
  Bool                  m_rwpSEIEnabled;
  Bool                  m_rwpSEIRwpCancelFlag;
  Bool                  m_rwpSEIRwpPersistenceFlag;
  Bool                  m_rwpSEIConstituentPictureMatchingFlag;
  Int                   m_rwpSEINumPackedRegions;
  Int                   m_rwpSEIProjPictureWidth;
  Int                   m_rwpSEIProjPictureHeight;
  Int                   m_rwpSEIPackedPictureWidth;
  Int                   m_rwpSEIPackedPictureHeight;
  std::vector<UChar>    m_rwpSEIRwpTransformType;
  std::vector<Bool>     m_rwpSEIRwpGuardBandFlag;
  std::vector<UInt>     m_rwpSEIProjRegionWidth;
  std::vector<UInt>     m_rwpSEIProjRegionHeight;
  std::vector<UInt>     m_rwpSEIRwpSEIProjRegionTop;
  std::vector<UInt>     m_rwpSEIProjRegionLeft;
  std::vector<UShort>   m_rwpSEIPackedRegionWidth;
  std::vector<UShort>   m_rwpSEIPackedRegionHeight;
  std::vector<UShort>   m_rwpSEIPackedRegionTop;
  std::vector<UShort>   m_rwpSEIPackedRegionLeft;
  std::vector<UChar>    m_rwpSEIRwpLeftGuardBandWidth;
  std::vector<UChar>    m_rwpSEIRwpRightGuardBandWidth;
  std::vector<UChar>    m_rwpSEIRwpTopGuardBandHeight;
  std::vector<UChar>    m_rwpSEIRwpBottomGuardBandHeight;
  std::vector<Bool>     m_rwpSEIRwpGuardBandNotUsedForPredFlag;
  std::vector<UChar>    m_rwpSEIRwpGuardBandType;
  Bool                  m_gopBasedTemporalFilterEnabled;               ///< GOP-based Temporal Filter enable/disable
  Int                   m_gopBasedTemporalFilterPastRefs;
  Int                   m_gopBasedTemporalFilterFutureRefs;
  Int                   m_firstValidFrame;
  Int                   m_lastValidFrame;
  std::map<Int, Double> m_gopBasedTemporalFilterStrengths;             ///< Filter strength per frame for the GOP-based Temporal Filter
#if JVET_Y0077_BIM
  Bool                  m_bimEnabled;
#endif

  std::string           m_arSEIFileRoot;
  Bool                    m_fisheyeVIdeoInfoSEIEnabled;
  TComSEIFisheyeVideoInfo m_fisheyeVideoInfoSEI;
  // weighted prediction
  Bool      m_useWeightedPred;                    ///< Use of weighted prediction in P slices
  Bool      m_useWeightedBiPred;                  ///< Use of bi-directional weighted prediction in B slices
  WeightedPredictionMethod m_weightedPredictionMethod;

  UInt      m_log2ParallelMergeLevel;                         ///< Parallel merge estimation region
  UInt      m_maxNumMergeCand;                                ///< Max number of merge candidates

  Int       m_TMVPModeId;
  Bool      m_signDataHidingEnabledFlag;
  Bool      m_RCEnableRateControl;                ///< enable rate control or not
  Int       m_RCTargetBitrate;                    ///< target bitrate when rate control is enabled
  Int       m_RCKeepHierarchicalBit;              ///< 0: equal bit allocation; 1: fixed ratio bit allocation; 2: adaptive ratio bit allocation
  Bool      m_RCLCULevelRC;                       ///< true: LCU level rate control; false: picture level rate control NOTE: code-tidy - rename to m_RCCtuLevelRC
  Bool      m_RCUseLCUSeparateModel;              ///< use separate R-lambda model at LCU level                        NOTE: code-tidy - rename to m_RCUseCtuSeparateModel
  Int       m_RCInitialQP;                        ///< inital QP for rate control
  Bool      m_RCForceIntraQP;                     ///< force all intra picture to use initial QP or not
  Bool      m_RCCpbSaturationEnabled;             ///< enable target bits saturation to avoid CPB overflow and underflow
  UInt      m_RCCpbSize;                          ///< CPB size
  Double    m_RCInitialCpbFullness;               ///< initial CPB fullness

#if KWU_RC_VIEWRC_E0227
  vector<Int>     m_viewTargetBits;
  Bool      m_viewWiseRateCtrl;                              ///< Flag for using view-wise rate control
#endif
#if KWU_RC_MADPRED_E0227
  UInt       m_depthMADPred;
#endif

  ScalingListMode m_useScalingListId;                         ///< using quantization matrix
  std::string m_scalingListFileName;                          ///< quantization matrix file name

  Bool      m_TransquantBypassEnabledFlag;                    ///< transquant_bypass_enabled_flag setting in PPS.
  Bool      m_CUTransquantBypassFlagForce;                    ///< if transquant_bypass_enabled_flag, then, if true, all CU transquant bypass flags will be set to true.
  CostMode  m_costMode;                                       ///< Cost mode to use

  Bool      m_recalculateQPAccordingToLambda;                 ///< recalculate QP value according to the lambda value
  Bool      m_useStrongIntraSmoothing;                        ///< enable strong intra smoothing for 32x32 blocks where the reference samples are flat
  Int       m_activeParameterSetsSEIEnabled;

  Bool      m_vuiParametersPresentFlag;                       ///< enable generation of VUI parameters
  Bool      m_aspectRatioInfoPresentFlag;                     ///< Signals whether aspect_ratio_idc is present
  Int       m_aspectRatioIdc;                                 ///< aspect_ratio_idc
  Int       m_sarWidth;                                       ///< horizontal size of the sample aspect ratio
  Int       m_sarHeight;                                      ///< vertical size of the sample aspect ratio
  Bool      m_overscanInfoPresentFlag;                        ///< Signals whether overscan_appropriate_flag is present
  Bool      m_overscanAppropriateFlag;                        ///< Indicates whether conformant decoded pictures are suitable for display using overscan
  Bool      m_videoSignalTypePresentFlag;                     ///< Signals whether video_format, video_full_range_flag, and colour_description_present_flag are present
  Int       m_videoFormat;                                    ///< Indicates representation of pictures
  Bool      m_videoFullRangeFlag;                             ///< Indicates the black level and range of luma and chroma signals
  Bool      m_colourDescriptionPresentFlag;                   ///< Signals whether colour_primaries, transfer_characteristics and matrix_coefficients are present
  Int       m_colourPrimaries;                                ///< Indicates chromaticity coordinates of the source primaries
  Int       m_transferCharacteristics;                        ///< Indicates the opto-electronic transfer characteristics of the source
  Int       m_matrixCoefficients;                             ///< Describes the matrix coefficients used in deriving luma and chroma from RGB primaries
  Bool      m_chromaLocInfoPresentFlag;                       ///< Signals whether chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field are present
  Int       m_chromaSampleLocTypeTopField;                    ///< Specifies the location of chroma samples for top field
  Int       m_chromaSampleLocTypeBottomField;                 ///< Specifies the location of chroma samples for bottom field
  Bool      m_neutralChromaIndicationFlag;                    ///< Indicates that the value of all decoded chroma samples is equal to 1<<(BitDepthCr-1)
  Bool      m_defaultDisplayWindowFlag;                       ///< Indicates the presence of the default window parameters
  Int       m_defDispWinLeftOffset;                           ///< Specifies the left offset from the conformance window of the default window
  Int       m_defDispWinRightOffset;                          ///< Specifies the right offset from the conformance window of the default window
  Int       m_defDispWinTopOffset;                            ///< Specifies the top offset from the conformance window of the default window
  Int       m_defDispWinBottomOffset;                         ///< Specifies the bottom offset from the conformance window of the default window
  Bool      m_frameFieldInfoPresentFlag;                      ///< Indicates that pic_struct values are present in picture timing SEI messages
  Bool      m_pocProportionalToTimingFlag;                    ///< Indicates that the POC value is proportional to the output time w.r.t. first picture in CVS
  Int       m_numTicksPocDiffOneMinus1;                       ///< Number of ticks minus 1 that for a POC difference of one
  Bool      m_bitstreamRestrictionFlag;                       ///< Signals whether bitstream restriction parameters are present
  Bool      m_tilesFixedStructureFlag;                        ///< Indicates that each active picture parameter set has the same values of the syntax elements related to tiles
  Bool      m_motionVectorsOverPicBoundariesFlag;             ///< Indicates that no samples outside the picture boundaries are used for inter prediction
  Int       m_minSpatialSegmentationIdc;                      ///< Indicates the maximum size of the spatial segments in the pictures in the coded video sequence
  Int       m_maxBytesPerPicDenom;                            ///< Indicates a number of bytes not exceeded by the sum of the sizes of the VCL NAL units associated with any coded picture
  Int       m_maxBitsPerMinCuDenom;                           ///< Indicates an upper bound for the number of bits of coding_unit() data
  Int       m_log2MaxMvLengthHorizontal;                      ///< Indicate the maximum absolute value of a decoded horizontal MV component in quarter-pel luma units
  Int       m_log2MaxMvLengthVertical;                        ///< Indicate the maximum absolute value of a decoded vertical MV component in quarter-pel luma units
  std::string m_colourRemapSEIFileRoot;
  std::string m_regionalNestingSEIFileRoot;
#if SHUTTER_INTERVAL_SEI_MESSAGE
  Bool      m_siiSEIEnabled;
  UInt      m_siiSEINumUnitsInShutterInterval;
  UInt      m_siiSEITimeScale;
  std::vector<UInt>     m_siiSEISubLayerNumUnitsInSI;
#endif
#if JVET_AE0101_PHASE_INDICATION_SEI_MESSAGE
  bool      m_phaseIndicationSEIEnabledFullResolution;
  int       m_piHorPhaseNumFullResolution;
  int       m_piHorPhaseDenMinus1FullResolution;
  int       m_piVerPhaseNumFullResolution;
  int       m_piVerPhaseDenMinus1FullResolution;
#endif

#if JVET_AL0061_ENCODER_OPTIMIZATION_INFORMATION_SEI
  bool  m_eoiSEIEnabled;
  bool  m_eoiSEICancelFlag;
  bool  m_eoiSEIPersistenceFlag;
  uint32_t m_eoiSEIForHumanViewingIdc;
  uint32_t m_eoiSEIForMachineAnalysisIdc;
  uint32_t m_eoiSEIType;
  uint32_t m_eoiSEIObjectBasedIdc;
  uint32_t m_eoiSEIQuantThresholdDelta;
  bool     m_eoiSEIPicQuantObjectFlag;
  bool m_eoiSEITemporalResamplingTypeFlag;
  uint32_t m_eoiSEINumIntPics;
  bool     m_eoiSEISrcPicFlag;;
  bool     m_eoiSEIOrigPicDimensionsFlag;
  uint32_t m_eoiSEIOrigPicWidth;
  uint32_t m_eoiSEIOrigPicHeight;
  bool m_eoiSEISpatialResamplingTypeFlag;
  uint32_t m_eoiSEIPrivacyProtectionTypeIdc;
  uint32_t m_eoiSEIPrivacyProtectedInfoType;
#endif

#if SEI_ENCODER_CONTROL
  // film grain characterstics sei
  Bool      m_fgcSEIEnabled;
  Bool      m_fgcSEICancelFlag;
  Bool      m_fgcSEIPersistenceFlag;
  UInt      m_fgcSEIModelID;
  Bool      m_fgcSEISepColourDescPresentFlag;
  UInt      m_fgcSEIBlendingModeID;
  UInt      m_fgcSEILog2ScaleFactor;
#if JVET_AL0339_SPATIAL_RESOLUTION_FOR_FGC_SEI
  UInt      m_fgcSEIPicWidthInLumaSamples;
  UInt      m_fgcSEIPicHeightInLumaSamples;
#endif
  Bool      m_fgcSEICompModelPresent[MAX_NUM_COMPONENT];
#if JVET_X0048_X0103_FILM_GRAIN
  Bool      m_fgcSEIAnalysisEnabled;
  std::string m_fgcSEIExternalMask;
  std::string m_fgcSEIExternalDenoised;
  Bool      m_fgcSEIPerPictureSEI;
  UInt      m_fgcSEINumIntensityIntervalMinus1[MAX_NUM_COMPONENT];
  UInt      m_fgcSEINumModelValuesMinus1[MAX_NUM_COMPONENT];
  UInt      m_fgcSEIIntensityIntervalLowerBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UInt      m_fgcSEIIntensityIntervalUpperBound[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES];
  UInt      m_fgcSEICompModelValue[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES][FG_MAX_NUM_MODEL_VALUES];
#endif
  // content light level SEI
  Bool      m_cllSEIEnabled;
  UInt      m_cllSEIMaxContentLevel;
  UInt      m_cllSEIMaxPicAvgLevel;
  // ambient viewing environment sei
  Bool      m_aveSEIEnabled;
  UInt      m_aveSEIAmbientIlluminance;
  UInt      m_aveSEIAmbientLightX;
  UInt      m_aveSEIAmbientLightY;
#endif
#if JVET_AK0194_DSC_SEI
  EncCfgParam::CfgSEIDigitallySignedContent m_cfgDigitallySignedContentSEI;
#endif
  std::string m_summaryOutFilename;                           ///< filename to use for producing summary output file.
  std::string m_summaryPicFilenameBase;                       ///< Base filename to use for producing summary picture output files. The actual filenames used will have I.txt, P.txt and B.txt appended.
  UInt        m_summaryVerboseness;                           ///< Specifies the level of the verboseness of the text output.

#if JVET_AK2006_SPTI_SEI_MESSAGE
  bool m_sptiSEIEnabled;
  bool m_sptiSourceTimingEqualsOutputTimingFlag;
  uint32_t m_sptiSourceType;
  uint32_t m_sptiTimeScale;
  uint32_t m_sptiNumUnitsInElementalInterval;
  bool m_sptiDirectionFlag;
#endif
#if JVET_AJ0207_GFV
  bool                                 m_generativeFaceVideoEnabled;
  uint32_t                             m_generativeFaceVideoSEINumber;
  bool                                 m_generativeFaceVideoSEIBasePicFlag;
  bool                                 m_generativeFaceVideoSEINNPresentFlag;
  uint32_t                             m_generativeFaceVideoSEINNModeIdc;
  std::string                          m_generativeFaceVideoSEINNTagURI;
  std::string                          m_generativeFaceVideoSEINNURI;
  bool                                 m_generativeFaceVideoSEIChromaKeyInfoPresentFlag;
  std::vector<bool>                    m_generativeFaceVideoSEIChromaKeyValuePresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIChromaKeyValue;
  std::vector<bool>                    m_generativeFaceVideoSEIChromaKeyThrPresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIChromaKeyThrValue;
  std::vector<bool>                    m_generativeFaceVideoSEIDrivePicFusionFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIId;
  std::vector<uint32_t>                m_generativeFaceVideoSEICnt;
  std::vector<bool>                    m_generativeFaceVideoSEILowConfidenceFaceParameterFlag;
  std::vector<bool>                    m_generativeFaceVideoSEICoordinatePresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEICoordinateQuantizationFactor;
  std::vector<bool>                    m_generativeFaceVideoSEICoordinatePredFlag;
  std::vector<bool>                    m_generativeFaceVideoSEI3DCoordinateFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEICoordinatePointNum;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateXTesonr;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateYTesonr;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIZCoordinateMaxValue;
  std::vector<std::vector<double>>     m_generativeFaceVideoSEICoordinateZTesonr;
  std::vector<bool>                    m_generativeFaceVideoSEIMatrixPresentFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEIMatrixElementPrecisionFactor;
  std::vector<bool>                    m_generativeFaceVideoSEIMatrixPredFlag;
  std::vector<uint32_t>                m_generativeFaceVideoSEINumMatrixType;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixTypeIdx;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricestoNumKpsFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatricesInfo;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrix3DSpaceFlag;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEINumMatrices;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixWidth;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoSEIMatrixHeight;
  std::vector < std::vector<std::vector<std::vector<std::vector<double>>>>>   m_generativeFaceVideoSEIMatrixElement;
  std::string                          m_generativeFaceVideoSEIPayloadFilename;
#endif
#if JVET_AK0239_GEFV
  bool                                 m_generativeFaceVideoEnhancementEnabled;
  uint32_t                             m_generativeFaceVideoEnhancementSEINumber;
  bool                                 m_generativeFaceVideoEnhancementSEIBasePicFlag;
  bool                                 m_generativeFaceVideoEnhancementSEINNPresentFlag;
  uint32_t                             m_generativeFaceVideoEnhancementSEINNModeIdc;
  std::string                          m_generativeFaceVideoEnhancementSEINNTagURI;
  std::string                          m_generativeFaceVideoEnhancementSEINNURI;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIId;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIGFVId;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIGFVCnt;
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPresentFlag;
  std::vector<bool>                    m_generativeFaceVideoEnhancementSEIMatrixPredFlag;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEINumMatrices;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoEnhancementSEIMatrixWidth;
  std::vector<std::vector<uint32_t>>   m_generativeFaceVideoEnhancementSEIMatrixHeight;
  std::vector<std::vector<std::vector<std::vector<double>>>>   m_generativeFaceVideoEnhancementSEIMatrixElement;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIPupilPresentIdx;
  std::vector<uint32_t>                m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX;
  std::vector<double>                  m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY;
  std::string                          m_generativeFaceVideoEnhancementSEIPayloadFilename;
#endif

#if EXTENSION_360_VIDEO
  TExt360AppEncCfg m_ext360;
  friend class TExt360AppEncCfg;
  friend class TExt360AppEncTop;
#endif

#if NH_MV
  Bool              m_outputVpsInfo;
  TChar*            m_pchBaseViewCameraNumbers;
#endif

#if JCTVC_AD0021_SEI_MANIFEST
  Bool       m_SEIManifestSEIEnabled;
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  Bool       m_SEIPrefixIndicationSEIEnabled;
#endif

#if JVET_AK0107_MODALITY_INFORMATION
  Bool      m_miSEIEnabled;
  Bool      m_miCancelFlag;
  Bool      m_miPersistenceFlag;
  Int       m_miModalityType; 
  Bool      m_miSpectrumRangePresentFlag;
  Int       m_miMinWavelengthMantissa; 
  Int       m_miMinWavelengthExponentPlus15; 
  Int       m_miMaxWavelengthMantissa; 
  Int       m_miMaxWavelengthExponentPlus15; 
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  bool     m_priSEIEnabled;
  bool     m_priSEICancelFlag;
  bool     m_priSEIPersistenceFlag;
  uint32_t m_priSEINumRegionsMinus1;
  bool     m_priSEIMultilayerFlag;
  bool     m_priSEIUseMaxDimensionsFlag;
  uint32_t m_priSEILog2UnitSize;
  uint32_t m_priSEIRegionSizeLenMinus1;
  bool     m_priSEIRegionIdPresentFlag;
  bool     m_priSEITargetPicParamsPresentFlag;
  uint32_t m_priSEITargetPicWidthMinus1;
  uint32_t m_priSEITargetPicHeightMinus1;
  uint32_t m_priSEINumResamplingRatiosMinus1;
  std::vector<uint32_t> m_priSEIResamplingWidthNumMinus1;
  std::vector<uint32_t> m_priSEIResamplingWidthDenomMinus1;
  std::vector<bool>     m_priSEIFixedAspectRatioFlag;
  std::vector<uint32_t> m_priSEIResamplingHeightNumMinus1;
  std::vector<uint32_t> m_priSEIResamplingHeightDenomMinus1;
  std::vector<uint32_t> m_priSEIRegionId;
  std::vector<uint32_t> m_priSEIRegionLayerId;
  std::vector<bool>     m_priSEIRegionIsALayerFlag;
  std::vector<uint32_t> m_priSEIRegionTopLeftInUnitsX;
  std::vector<uint32_t> m_priSEIRegionTopLeftInUnitsY;
  std::vector<uint32_t> m_priSEIRegionWidthInUnitsMinus1;
  std::vector<uint32_t> m_priSEIRegionHeightInUnitsMinus1;
  std::vector<uint32_t> m_priSEIResamplingRatioIdx;
  std::vector<uint32_t> m_priSEITargetRegionTopLeftInUnitsX;
  std::vector<uint32_t> m_priSEITargetRegionTopLeftInUnitsY;
#endif

  // internal member functions
  Void  xCheckParameter ();                                   ///< check validity of configuration values
  Void  xPrintParameter ();                                   ///< print configuration values
  Void  xPrintUsage     ();                                   ///< print usage
#if DPB_ENCODER_USAGE_CHECK
  Int   xDPBUsage(std::ostream *pOs);                         ///> Calculates maximum number of frames stored in DPB. Can optionally output usage to a stream
#endif
#if NH_MV
  Void xConfirmRepFormat( const TComVPS& vps);
  Void xDeriveProfAndConstrFlags( const TComVPS& vps );
  Void xCheckProfiles           ( const TComVPS& vps );

  Void xPrintProfiles();

  template< typename T >
  Void xConfirmSingleRepFormat(Bool& checkFailed, std::string name, Int curLayer, Int refLayer, T valCur, T valRef )
  {
    if ( valCur != valRef )
    {
      printf("Error: %s of layer %d and its reference layer %d must be equal. \n", name.c_str(), curLayer, refLayer );
      checkFailed = true;
    }
  }

  GOPEntry*  xGetGopEntries( Int layerIdInVps );
  GOPEntry*  xGetGopEntry( Int layerIdInVps, Int poc );

  template<typename T>
  Void xReadStrToEnum(string in, std::vector<T> &val)
  {
    val.clear();

    char* cElement = NULL;
    char* cString = new char[in.size()+1];
    strcpy(cString, in.c_str());

    cElement = strtok( cString, " " );
    while ( cElement != NULL )
    {
      T profile;
      std::istringstream ss( cElement );
      ss >> profile;
      val.push_back( profile );
      cElement = strtok( NULL, " " );
    }
    delete[] cString;
  }


  template <typename T>
  Void xResizeVector(  std::vector<T> & rpcVector )
  {
    for( Int layer = 0; rpcVector.size() < m_numberOfLayers; layer++ )
    {
      assert( rpcVector.size() > 0 );
      rpcVector.push_back( rpcVector[layer] );
    }

    for( ; rpcVector.size() > m_numberOfLayers; )
    {
      rpcVector.pop_back( );
    }
  }

  template <typename T>
  Void xResizeVector(  std::vector<T> & rpcVector, UInt n )
  {
    for( Int layer = 0; rpcVector.size() < n; layer++ )
    {
      assert( rpcVector.size() > 0 );
      rpcVector.push_back( rpcVector[layer] );
    }

    for( ; rpcVector.size() > n; )
    {
      rpcVector.pop_back( );
    }
  }


  template <typename T>
  Void xPrintParaVector( std::string description, std::vector<T> & rpcVector )
  {
    Int iSpace = max(1, ENC_CFG_CONSOUT_SPACE - (Int) description.length() );
    
    for ( Int i = 0; i < iSpace; i++ )
    {
      description.append( " " );
    }
      
    description.append( ":" );
    printf( "%s", description.c_str() );

    for(Int i=0;i<rpcVector.size();i++)
    {
      xPrintVectorElem( rpcVector[i] );
    }

    printf("\n");
  }
  
  Void xPrintVectorElem( UInt   elem ) { printf(" %d"   , elem            );};
  Void xPrintVectorElem( Int    elem ) { printf(" %d"   , elem            );};
  
  Void xPrintVectorElem( Double elem ) { printf(" %5.2f", elem            );};
  Void xPrintVectorElem( Bool   elem ) { printf(" %d"   , ( elem ? 1 : 0 ));};
  Void xParseSeiCfg();

  Int   getGOPSize() { return m_iGOPSize; }
#endif
public:
  TAppEncCfg();
  virtual ~TAppEncCfg();

public:
  Void  create    ();                                         ///< create option handling class
  Void  destroy   ();                                         ///< destroy option handling class
  Bool  parseCfg  ( Int argc, TChar* argv[] );                ///< parse configuration file to fill member variables

#if NH_MV
private:
  Void   xConvertRepFormatParameters(
    IntAry2d& tmpPad                       ,
    IntAry2d& tmpInputBitDepth             ,
    IntAry2d& tmpOutputBitDepth            ,
    IntAry2d& tmpMSBExtendedBitDepth       ,
    IntAry2d& tmpInternalBitDepth          ,
    IntAry1d& tmpInputChromaFormat         ,
    IntAry1d& tmpChromaFormat
    );

  Void xGetMaxValuesOfApplicableLayers(const TComVPS& vps, Int vpsPtlIdx, Int& maxBitDepthLuma, Int& maxBitDepthChroma, ChromaFormat& maxChromaFormatIdc, Int& maxNumRefLayers);
#endif

};// END CLASS DEFINITION TAppEncCfg

//! \}

#endif // __TAPPENCCFG__

