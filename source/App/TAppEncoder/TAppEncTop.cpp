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

/** \file     TAppEncTop.cpp
    \brief    Encoder application class
*/

#include <list>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <iomanip>

#include "TAppEncTop.h"
#include "TLibEncoder/TEncTemporalFilter.h"
#include "TLibEncoder/AnnexBwrite.h"

#if EXTENSION_360_VIDEO
#include "TAppEncHelper360/TExt360AppEncTop.h"
#endif

using namespace std;

//! \ingroup TAppEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / initialization / destroy
// ====================================================================================================================

TAppEncTop::TAppEncTop()
#if NH_MV
  : m_spsMap( MAX_NUM_SPS ),
    m_ppsMap( MAX_NUM_PPS )
#endif
{
#if NH_MV
  m_vps = new TComVPS; 
#else
  m_iFrameRcvd = 0;
#endif
  m_totalBytes = 0;
  m_essentialBytes = 0;
}

TAppEncTop::~TAppEncTop()
{
#if NH_MV
  if (m_vps)
  {
   delete m_vps;
  };
#endif
}

Void TAppEncTop::xInitLibCfg()
{
#if NH_MV
  TComVPS& vps = (*m_vps);   
#else
  TComVPS vps;
#endif

#if NH_MV
  Int maxTempLayer = -1;
  for (Int j = 0; j < m_numberOfLayers; j++)
  {
    maxTempLayer = max( m_maxTempLayerMvc[ j ], maxTempLayer );
  }

  vps.setMaxTLayers                       ( maxTempLayer );
  if ( maxTempLayer )
  {
    vps.setTemporalNestingFlag(true);
  }
  vps.setMaxLayersMinus1( m_numberOfLayers - 1);
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    Int maxNumReOrderPics  = 0;
    Int maxDecPicBuffering = 0;
    for (Int j = 0; j < m_numberOfLayers; j++)
    {
      maxNumReOrderPics  = max( maxNumReOrderPics,  m_numReorderPicsMvc    [ j ][ i ] );
      maxDecPicBuffering = max( maxDecPicBuffering, m_maxDecPicBufferingMvc[ j ][ i ] );
    }

    vps.setNumReorderPics                 ( maxNumReOrderPics  ,i );
    vps.setMaxDecPicBuffering             ( maxDecPicBuffering ,i );
  }
#else
  vps.setMaxTLayers                                               ( m_maxTempLayer );
  if (m_maxTempLayer == 1)
  {
    vps.setTemporalNestingFlag(true);
  }
  vps.setMaxLayers                                                ( 1 );
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    vps.setNumReorderPics                                         ( m_numReorderPics[i], i );
    vps.setMaxDecPicBuffering                                     ( m_maxDecPicBuffering[i], i );
  }
#endif

#if NH_MV
  xSetTimingInfo           ( vps );
  xSetHrdParameters        ( vps );
  xSetLayerIds             ( vps );
  xSetDimensionIdAndLength ( vps );
  xSetDependencies         ( vps );
  xSetRepFormat            ( vps );
  xSetLayerSets            ( vps );
  xSetProfileTierLevel     ( vps );
  xSetDpbSize              ( vps );
  xSetVPSVUI               ( vps );

  xDeriveParameterSetIds( vps );

  if ( m_targetEncLayerIdList.size() == 0 )
  {
    for (Int i = 0; i < m_numberOfLayers; i++ )
    {
      m_targetEncLayerIdList.push_back( vps.getLayerIdInNuh( i ) );
    }
  }
  for( Int i = (Int) m_targetEncLayerIdList.size()-1 ; i >= 0 ; i--)
  {
    Int iNuhLayerId = m_targetEncLayerIdList[i];
    Bool allRefLayersPresent = true;
    for( Int j = 0; j < vps.getNumRefLayers( iNuhLayerId ); j++)
    {
      allRefLayersPresent = allRefLayersPresent && xLayerIdInTargetEncLayerIdList( vps.getIdRefLayer( iNuhLayerId, j) );
    }
    if ( !allRefLayersPresent )
    {
      printf("\nCannot encode layer with nuh_layer_id equal to %d since not all reference layers are in TargetEncLayerIdList\n", iNuhLayerId);
      m_targetEncLayerIdList.erase( m_targetEncLayerIdList.begin() + i  );
    }
  }

  if ( m_outputVpsInfo )
  {
    vps.printScalabilityId();
    vps.printLayerDependencies();
    vps.printLayerSets();
    vps.printPTL();
    vps.printRepFormat();
  }

  /// Create encoders and set profiles profiles
  for(Int layerIdInVps = 0; layerIdInVps < m_numberOfLayers; layerIdInVps++)
  {
    m_frameRcvd                 .push_back(0);
#if NH_MV
    m_acTEncTopList             .push_back(new TEncTop( m_spsMap, m_ppsMap ) );
#else
    m_acTEncTopList             .push_back(new TEncTop);
#endif
    m_acTVideoIOYuvInputFileList.push_back(new TVideoIOYuv);
    m_acTVideoIOYuvReconFileList.push_back(new TVideoIOYuv);
#if SHUTTER_INTERVAL_SEI_PROCESSING
    m_cTVideoIOYuvSIIPreFileList.push_back(new TVideoIOYuv);
#endif
  }


  for(Int layerIdInVps = 0; layerIdInVps < m_numberOfLayers; layerIdInVps++)
  {
    Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layerIdInVps];
    m_cListPicYuvRec            .push_back(new TComList<TComPicYuv*>) ;
    TEncTop& m_cTEncTop = *m_acTEncTopList[ layerIdInVps ];  // It is not a member, but this name helps avoiding code duplication !!!

    Int layerId = vps.getLayerIdInNuh          ( layerIdInVps );
    m_ivPicLists.getSubDpb( layerId, true );

    m_cTEncTop.setLayerIdInVps                 ( layerIdInVps );
    m_cTEncTop.setLayerId                      ( layerId );
    m_cTEncTop.setViewId                       ( vps.getViewId      (  layerId ) );
    m_cTEncTop.setViewIndex                    ( vps.getViewIndex   (  layerId ) );
    m_cTEncTop.setSendParameterSets            ( m_sendParameterSets[ layerIdInVps ]  );
    m_cTEncTop.setParameterSetId               ( m_parameterSetId   [ layerIdInVps ]  );
    m_cTEncTop.setIvPicLists                   ( &m_ivPicLists );
#endif  // NH_MV
  m_cTEncTop.setVPS(&vps);

#if NH_MV
  // These values go to the SPS of the base layer only and should apply as follows:
  //   If the profile_tier_level( ) syntax structure is included in an active SPS for the base layer
  //    or is the profile_tier_level( ) syntax structure VpsProfileTierLevel[ 0 ],
  //    it applies to the OLS containing all layers in the bitstream but with only the base layer being the output layer.
  //   Otherwise, if the profile_tier_level( ) syntax structure is included in an active SPS
  //    for an independent non-base layer with nuh_layer_id equal to layerId, it applies to the output bitstream
  //    of the independent non-base layer rewriting process of clause F.10.2 with the input variables assignedBaseLayerId equal to layerId and tIdTarget equal to 6.

  m_cTEncTop.setProfile                                           ( m_profiles[0]                       );
  m_cTEncTop.setLevel                                             ( m_levelTier[0], m_level[0]          );
  m_cTEncTop.setProgressiveSourceFlag                             ( m_progressiveSourceFlags        [0] );
  m_cTEncTop.setInterlacedSourceFlag                              ( m_interlacedSourceFlags         [0] );
  m_cTEncTop.setNonPackedConstraintFlag                           ( m_nonPackedConstraintFlags      [0] );
  m_cTEncTop.setFrameOnlyConstraintFlag                           ( m_frameOnlyConstraintFlags      [0] );
  m_cTEncTop.setBitDepthConstraintValue                           ( m_bitDepthConstraints           [0] );
  m_cTEncTop.setChromaFormatConstraintValue                       ( m_chromaFormatConstraints       [0] );
  m_cTEncTop.setIntraConstraintFlag                               ( m_intraConstraintFlags          [0] );
  m_cTEncTop.setOnePictureOnlyConstraintFlag                      ( m_onePictureOnlyConstraintFlags [0] );
  m_cTEncTop.setLowerBitRateConstraintFlag                        ( m_lowerBitRateConstraintFlags   [0] );
#else
  m_cTEncTop.setProfile                                           ( m_profile);
  m_cTEncTop.setLevel                                             ( m_levelTier, m_level);
  m_cTEncTop.setProgressiveSourceFlag                             ( m_progressiveSourceFlag);
  m_cTEncTop.setInterlacedSourceFlag                              ( m_interlacedSourceFlag);
  m_cTEncTop.setNonPackedConstraintFlag                           ( m_nonPackedConstraintFlag);
  m_cTEncTop.setFrameOnlyConstraintFlag                           ( m_frameOnlyConstraintFlag);
  m_cTEncTop.setBitDepthConstraintValue                           ( m_bitDepthConstraint );
  m_cTEncTop.setChromaFormatConstraintValue                       ( m_chromaFormatConstraint );
  m_cTEncTop.setIntraConstraintFlag                               ( m_intraConstraintFlag );
  m_cTEncTop.setOnePictureOnlyConstraintFlag                      ( m_onePictureOnlyConstraintFlag );
  m_cTEncTop.setLowerBitRateConstraintFlag                        ( m_lowerBitRateConstraintFlag );
#endif

  m_cTEncTop.setPrintMSEBasedSequencePSNR                         ( m_printMSEBasedSequencePSNR);
  m_cTEncTop.setPrintHexPsnr                                      ( m_printHexPsnr);
  m_cTEncTop.setPrintFrameMSE                                     ( m_printFrameMSE);
  m_cTEncTop.setPrintSequenceMSE                                  ( m_printSequenceMSE);
  m_cTEncTop.setPrintMSSSIM                                       ( m_printMSSSIM );

  m_cTEncTop.setXPSNREnableFlag                                   ( m_bXPSNREnableFlag);
  for (Int id = 0 ; id < MAX_NUM_COMPONENT; id++)
  {
    m_cTEncTop.setXPSNRWeight                                     ( m_dXPSNRWeight[id], ComponentID(id));
  }

#if SHUTTER_INTERVAL_SEI_PROCESSING
  m_cTEncTop.setShutterFilterFlag                                 ( m_ShutterFilterEnable );
#endif
#if JVET_AK0194_DSC_SEI
  m_cTEncTop.setDigitallySignedContentSEICfg                      (m_cfgDigitallySignedContentSEI);
#endif

#if JVET_AK0140_PACKED_REGIONS_INFORMATION_SEI
  m_cTEncTop.setPriSEIEnabled(m_priSEIEnabled);
  m_cTEncTop.setPriSEICancelFlag(m_priSEICancelFlag);
  m_cTEncTop.setPriSEIPersistenceFlag(m_priSEIPersistenceFlag);
  m_cTEncTop.setPriSEINumRegionsMinus1(m_priSEINumRegionsMinus1);
  m_cTEncTop.setPriSEIMultilayerFlag(false); // Only single layer in HM encoder
  m_cTEncTop.setPriSEIUseMaxDimensionsFlag(m_priSEIUseMaxDimensionsFlag);
  m_cTEncTop.setPriSEILog2UnitSize(m_priSEILog2UnitSize);
  m_cTEncTop.setPriSEIRegionSizeLenMinus1(m_priSEIRegionSizeLenMinus1);
  m_cTEncTop.setPriSEIRegionIdPresentFlag(m_priSEIRegionIdPresentFlag);
  m_cTEncTop.setPriSEITargetPicParamsPresentFlag(m_priSEITargetPicParamsPresentFlag);
  m_cTEncTop.setPriSEITargetPicWidthMinus1(m_priSEITargetPicWidthMinus1);
  m_cTEncTop.setPriSEITargetPicHeightMinus1(m_priSEITargetPicHeightMinus1);
  m_cTEncTop.setPriSEINumResamplingRatiosMinus1(m_priSEINumResamplingRatiosMinus1);
  m_cTEncTop.setPriSEIResamplingWidthNumMinus1(m_priSEIResamplingWidthNumMinus1);
  m_cTEncTop.setPriSEIResamplingWidthDenomMinus1(m_priSEIResamplingWidthDenomMinus1);
  m_cTEncTop.setPriSEIFixedAspectRatioFlag(m_priSEIFixedAspectRatioFlag);
  m_cTEncTop.setPriSEIResamplingHeightNumMinus1(m_priSEIResamplingHeightNumMinus1);
  m_cTEncTop.setPriSEIResamplingHeightDenomMinus1(m_priSEIResamplingHeightDenomMinus1);
  m_cTEncTop.setPriSEIRegionId(m_priSEIRegionId);
  m_cTEncTop.setPriSEIRegionTopLeftInUnitsX(m_priSEIRegionTopLeftInUnitsX);
  m_cTEncTop.setPriSEIRegionTopLeftInUnitsY(m_priSEIRegionTopLeftInUnitsY);
  m_cTEncTop.setPriSEIRegionWidthInUnitsMinus1(m_priSEIRegionWidthInUnitsMinus1);
  m_cTEncTop.setPriSEIRegionHeightInUnitsMinus1(m_priSEIRegionHeightInUnitsMinus1);
  m_cTEncTop.setPriSEIResamplingRatioIdx(m_priSEIResamplingRatioIdx);
  m_cTEncTop.setPriSEITargetRegionTopLeftInUnitsX(m_priSEITargetRegionTopLeftInUnitsX);
  m_cTEncTop.setPriSEITargetRegionTopLeftInUnitsY(m_priSEITargetRegionTopLeftInUnitsY);
#endif

  m_cTEncTop.setCabacZeroWordPaddingEnabled                       ( m_cabacZeroWordPaddingEnabled );

  m_cTEncTop.setFrameRate                                         ( m_iFrameRate );
  m_cTEncTop.setFrameSkip                                         ( m_FrameSkip );
  m_cTEncTop.setTemporalSubsampleRatio                            ( m_temporalSubsampleRatio );
#if NH_MV
  m_cTEncTop.setSourceWidth                                       ( m_iSourceWidths [repFormatIdx] );
  m_cTEncTop.setSourceHeight                                      ( m_iSourceHeights[repFormatIdx] );
  m_cTEncTop.setConformanceWindow                                 ( m_confWinLefts[repFormatIdx], m_confWinRights[repFormatIdx], m_confWinTops[repFormatIdx], m_confWinBottoms[repFormatIdx] );
#else
  m_cTEncTop.setSourceWidth                                       ( m_sourceWidth );
  m_cTEncTop.setSourceHeight                                      ( m_sourceHeight );
  m_cTEncTop.setConformanceWindow                                 ( m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom );
#endif
  m_cTEncTop.setFramesToBeEncoded                                 ( m_framesToBeEncoded );

  //====== Coding Structure ========
#if NH_MV
  m_cTEncTop.setIntraPeriod                                       ( m_iIntraPeriod[ layerIdInVps ] );
#else
  m_cTEncTop.setIntraPeriod                                       ( m_iIntraPeriod );
#endif
  m_cTEncTop.setDecodingRefreshType                               ( m_iDecodingRefreshType );
  m_cTEncTop.setGOPSize                                           ( m_iGOPSize );
  m_cTEncTop.setReWriteParamSetsFlag                              ( m_bReWriteParamSetsFlag );
#if NH_MV
  m_cTEncTop.setGopList                                           ( xGetGopEntries(layerIdInVps) );
  m_cTEncTop.setExtraRPSs                                         ( m_extraRPSsMvc[layerIdInVps] );
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    m_cTEncTop.setNumReorderPics                                  ( m_numReorderPicsMvc[layerIdInVps][i], i );
    m_cTEncTop.setMaxDecPicBuffering                              ( m_maxDecPicBufferingMvc[layerIdInVps][i], i );
  }
#else
  m_cTEncTop.setGopList                                           ( m_GOPList );
  m_cTEncTop.setExtraRPSs                                         ( m_extraRPSs );
  for(Int i = 0; i < MAX_TLAYER; i++)
  {
    m_cTEncTop.setNumReorderPics                                  ( m_numReorderPics[i], i );
    m_cTEncTop.setMaxDecPicBuffering                              ( m_maxDecPicBuffering[i], i );
  }
#endif
  for( UInt uiLoop = 0; uiLoop < MAX_TLAYER; ++uiLoop )
  {
    m_cTEncTop.setLambdaModifier                                  ( uiLoop, m_adLambdaModifier[ uiLoop ] );
  }
  m_cTEncTop.setIntraLambdaModifier                               ( m_adIntraLambdaModifier );
  m_cTEncTop.setIntraQpFactor                                     ( m_dIntraQpFactor );

#if NH_MV
  m_cTEncTop.setQP                                                ( m_iQP[layerIdInVps] );
#else
  m_cTEncTop.setQP                                                ( m_iQP );
#endif

  m_cTEncTop.setIntraQPOffset                                     ( m_intraQPOffset );
  m_cTEncTop.setLambdaFromQPEnable                                ( m_lambdaFromQPEnable );
#if NH_MV
  m_cTEncTop.setPad                                               ( &m_aiPads[ repFormatIdx ][0] );
#else
  m_cTEncTop.setSourcePadding                                     ( m_sourcePadding );
#endif

  m_cTEncTop.setAccessUnitDelimiter                               ( m_AccessUnitDelimiter );

#if NH_MV
  m_cTEncTop.setMaxTempLayer                                      ( m_maxTempLayerMvc[layerIdInVps] );
#else
  m_cTEncTop.setMaxTempLayer                                      ( m_maxTempLayer );
#endif
  m_cTEncTop.setUseAMP( m_enableAMP );

  //===== Slice ========

  //====== Loop/Deblock Filter ========
#if NH_MV
  m_cTEncTop.setLoopFilterDisable                                 ( m_bLoopFilterDisable[layerIdInVps]);
#else
  m_cTEncTop.setLoopFilterDisable                                 ( m_bLoopFilterDisable       );
#endif
  m_cTEncTop.setLoopFilterOffsetInPPS                             ( m_loopFilterOffsetInPPS );
  m_cTEncTop.setLoopFilterBetaOffset                              ( m_loopFilterBetaOffsetDiv2  );
  m_cTEncTop.setLoopFilterTcOffset                                ( m_loopFilterTcOffsetDiv2    );
  m_cTEncTop.setDeblockingFilterMetric                            ( m_deblockingFilterMetric );

  //====== Motion search ========
  m_cTEncTop.setDisableIntraPUsInInterSlices                      ( m_bDisableIntraPUsInInterSlices );
  m_cTEncTop.setMotionEstimationSearchMethod                      ( m_motionEstimationSearchMethod  );
  m_cTEncTop.setSearchRange                                       ( m_iSearchRange );
  m_cTEncTop.setBipredSearchRange                                 ( m_bipredSearchRange );
  m_cTEncTop.setClipForBiPredMeEnabled                            ( m_bClipForBiPredMeEnabled );
  m_cTEncTop.setFastMEAssumingSmootherMVEnabled                   ( m_bFastMEAssumingSmootherMVEnabled );
  m_cTEncTop.setMinSearchWindow                                   ( m_minSearchWindow );
  m_cTEncTop.setRestrictMESampling                                ( m_bRestrictMESampling );

#if NH_MV
  m_cTEncTop.setUseDisparitySearchRangeRestriction                ( m_bUseDisparitySearchRangeRestriction );
  m_cTEncTop.setVerticalDisparitySearchRange                      ( m_iVerticalDisparitySearchRange );
#endif
  //====== Quality control ========
  m_cTEncTop.setMaxDeltaQP                                        ( m_iMaxDeltaQP  );
  m_cTEncTop.setMaxCuDQPDepth                                     ( m_iMaxCuDQPDepth  );
  m_cTEncTop.setDiffCuChromaQpOffsetDepth                         ( m_diffCuChromaQpOffsetDepth );
  m_cTEncTop.setChromaCbQpOffset                                  ( m_cbQpOffset     );
  m_cTEncTop.setChromaCrQpOffset                                  ( m_crQpOffset  );
  m_cTEncTop.setWCGChromaQpControl                                ( m_wcgChromaQpControl );
  m_cTEncTop.setSliceChromaOffsetQpIntraOrPeriodic                ( m_sliceChromaQpOffsetPeriodicity, m_sliceChromaQpOffsetIntraOrPeriodic );
#if NH_MV
  m_cTEncTop.setChromaFormatIdc                                   ( m_chromaFormatIDCs[ repFormatIdx ] );
#else
  m_cTEncTop.setChromaFormatIdc                                   ( m_chromaFormatIDC  );
#endif
#if ADAPTIVE_QP_SELECTION
  m_cTEncTop.setUseAdaptQpSelect                                  ( m_bUseAdaptQpSelect   );
#endif
#if  JVET_V0078
  m_cTEncTop.setSmoothQPReductionEnable                           (m_bSmoothQPReductionEnable);
  m_cTEncTop.setSmoothQPReductionThreshold                        (m_dSmoothQPReductionThreshold);
  m_cTEncTop.setSmoothQPReductionModelScale                       (m_dSmoothQPReductionModelScale);
  m_cTEncTop.setSmoothQPReductionModelOffset                      (m_dSmoothQPReductionModelOffset);
  m_cTEncTop.setSmoothQPReductionLimit                            (m_iSmoothQPReductionLimit);
  m_cTEncTop.setSmoothQPReductionPeriodicity                      (m_iSmoothQPReductionPeriodicity);
#endif

  m_cTEncTop.setUseAdaptiveQP                                     ( m_bUseAdaptiveQP  );
  m_cTEncTop.setQPAdaptationRange                                 ( m_iQPAdaptationRange );
  m_cTEncTop.setExtendedPrecisionProcessingFlag                   ( m_extendedPrecisionProcessingFlag );
  m_cTEncTop.setHighPrecisionOffsetsEnabledFlag                   ( m_highPrecisionOffsetsEnabledFlag );

  m_cTEncTop.setWeightedPredictionMethod( m_weightedPredictionMethod );

  //====== Tool list ========
  m_cTEncTop.setLumaLevelToDeltaQPControls                        ( m_lumaLevelToDeltaQPMapping );
  m_cTEncTop.setDeltaQpRD( (m_costMode==COST_LOSSLESS_CODING) ? 0 : m_uiDeltaQpRD );
  m_cTEncTop.setFastDeltaQp                                       ( m_bFastDeltaQP  );
  m_cTEncTop.setUseASR                                            ( m_bUseASR      );
  m_cTEncTop.setUseHADME                                          ( m_bUseHADME    );
#if NH_MV
  m_cTEncTop.setdQPs                                              ( m_aidQP[layerIdInVps]   );
#else
  m_cTEncTop.setdQPs                                              ( m_aidQP        );
#endif
  m_cTEncTop.setUseRDOQ                                           ( m_useRDOQ     );
  m_cTEncTop.setUseRDOQTS                                         ( m_useRDOQTS   );
  m_cTEncTop.setUseSelectiveRDOQ                                  ( m_useSelectiveRDOQ );
  m_cTEncTop.setRDpenalty                                         ( m_rdPenalty );
  m_cTEncTop.setMaxCUWidth                                        ( m_uiMaxCUWidth );
  m_cTEncTop.setMaxCUHeight                                       ( m_uiMaxCUHeight );
#if NH_MV
  m_cTEncTop.setMaxTotalCUDepth                                   ( m_uiMaxTotalCUDepth[ repFormatIdx] );
#else
  m_cTEncTop.setMaxTotalCUDepth                                   ( m_uiMaxTotalCUDepth );
#endif
  m_cTEncTop.setLog2DiffMaxMinCodingBlockSize                     ( m_uiLog2DiffMaxMinCodingBlockSize );
  m_cTEncTop.setQuadtreeTULog2MaxSize                             ( m_uiQuadtreeTULog2MaxSize );
  m_cTEncTop.setQuadtreeTULog2MinSize                             ( m_uiQuadtreeTULog2MinSize );
  m_cTEncTop.setQuadtreeTUMaxDepthInter                           ( m_uiQuadtreeTUMaxDepthInter );
  m_cTEncTop.setQuadtreeTUMaxDepthIntra                           ( m_uiQuadtreeTUMaxDepthIntra );
  m_cTEncTop.setFastInterSearchMode                               ( m_fastInterSearchMode );
  m_cTEncTop.setUseEarlyCU                                        ( m_bUseEarlyCU  );
  m_cTEncTop.setUseFastDecisionForMerge                           ( m_useFastDecisionForMerge  );
  m_cTEncTop.setUseCbfFastMode                                    ( m_bUseCbfFastMode  );
  m_cTEncTop.setUseEarlySkipDetection                             ( m_useEarlySkipDetection );
  m_cTEncTop.setCrossComponentPredictionEnabledFlag               ( m_crossComponentPredictionEnabledFlag );
  m_cTEncTop.setUseReconBasedCrossCPredictionEstimate             ( m_reconBasedCrossCPredictionEstimate );
#if NH_MV
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_LUMA  , m_log2SaoOffsetScale[layerIdInVps][CHANNEL_TYPE_LUMA]   );
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_CHROMA, m_log2SaoOffsetScale[layerIdInVps][CHANNEL_TYPE_CHROMA] );
#else
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_LUMA  , m_log2SaoOffsetScale[CHANNEL_TYPE_LUMA]   );
  m_cTEncTop.setLog2SaoOffsetScale                                ( CHANNEL_TYPE_CHROMA, m_log2SaoOffsetScale[CHANNEL_TYPE_CHROMA] );
#endif
  m_cTEncTop.setUseTransformSkip                                  ( m_useTransformSkip      );
  m_cTEncTop.setUseTransformSkipFast                              ( m_useTransformSkipFast  );
  m_cTEncTop.setTransformSkipRotationEnabledFlag                  ( m_transformSkipRotationEnabledFlag );
  m_cTEncTop.setTransformSkipContextEnabledFlag                   ( m_transformSkipContextEnabledFlag   );
  m_cTEncTop.setPersistentRiceAdaptationEnabledFlag               ( m_persistentRiceAdaptationEnabledFlag );
  m_cTEncTop.setCabacBypassAlignmentEnabledFlag                   ( m_cabacBypassAlignmentEnabledFlag );
  m_cTEncTop.setLog2MaxTransformSkipBlockSize                     ( m_log2MaxTransformSkipBlockSize  );
  for (UInt signallingModeIndex = 0; signallingModeIndex < NUMBER_OF_RDPCM_SIGNALLING_MODES; signallingModeIndex++)
  {
    m_cTEncTop.setRdpcmEnabledFlag                                ( RDPCMSignallingMode(signallingModeIndex), m_rdpcmEnabledFlag[signallingModeIndex]);
  }
  m_cTEncTop.setUseConstrainedIntraPred                           ( m_bUseConstrainedIntraPred );
  m_cTEncTop.setFastUDIUseMPMEnabled                              ( m_bFastUDIUseMPMEnabled );
  m_cTEncTop.setFastMEForGenBLowDelayEnabled                      ( m_bFastMEForGenBLowDelayEnabled );
  m_cTEncTop.setUseBLambdaForNonKeyLowDelayPictures               ( m_bUseBLambdaForNonKeyLowDelayPictures );
  m_cTEncTop.setPCMLog2MinSize                                    ( m_uiPCMLog2MinSize);
  m_cTEncTop.setUsePCM                                            ( m_usePCM );

  // set internal bit-depth and constants
  for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++)
  {
#if NH_MV
    m_cTEncTop.setBitDepth((ChannelType)channelType, m_internalBitDepths[repFormatIdx][channelType]);
    m_cTEncTop.setPCMBitDepth((ChannelType)channelType, m_bPCMInputBitDepthFlag ? m_MSBExtendedBitDepths[repFormatIdx][channelType] : m_internalBitDepths[repFormatIdx][channelType]);
#else
    m_cTEncTop.setBitDepth((ChannelType)channelType, m_internalBitDepth[channelType]);
    m_cTEncTop.setPCMBitDepth((ChannelType)channelType, m_bPCMInputBitDepthFlag ? m_MSBExtendedBitDepth[channelType] : m_internalBitDepth[channelType]);
#if JVET_X0048_X0103_FILM_GRAIN
    m_cTEncTop.setBitDepthInput((ChannelType)channelType, m_inputBitDepth[channelType]);
#endif
#endif
  }

  m_cTEncTop.setPCMLog2MaxSize                                    ( m_pcmLog2MaxSize);
  m_cTEncTop.setMaxNumMergeCand                                   ( m_maxNumMergeCand );


  //====== Weighted Prediction ========
  m_cTEncTop.setUseWP                                             ( m_useWeightedPred     );
  m_cTEncTop.setWPBiPred                                          ( m_useWeightedBiPred   );

  //====== Parallel Merge Estimation ========
  m_cTEncTop.setLog2ParallelMergeLevelMinus2                      ( m_log2ParallelMergeLevel - 2 );

  //====== Slice ========
  m_cTEncTop.setSliceMode                                         ( m_sliceMode );
  m_cTEncTop.setSliceArgument                                     ( m_sliceArgument );

  //====== Dependent Slice ========
  m_cTEncTop.setSliceSegmentMode                                  ( m_sliceSegmentMode );
  m_cTEncTop.setSliceSegmentArgument                              ( m_sliceSegmentArgument );

  if(m_sliceMode == NO_SLICES )
  {
    m_bLFCrossSliceBoundaryFlag = true;
  }
  m_cTEncTop.setLFCrossSliceBoundaryFlag                          ( m_bLFCrossSliceBoundaryFlag );
#if NH_MV
  m_cTEncTop.setUseSAO ( m_bUseSAO[layerIdInVps] );
#else
  m_cTEncTop.setUseSAO                                            ( m_bUseSAO );
#endif
  m_cTEncTop.setTestSAODisableAtPictureLevel                      ( m_bTestSAODisableAtPictureLevel );
  m_cTEncTop.setSaoEncodingRate                                   ( m_saoEncodingRate );
  m_cTEncTop.setSaoEncodingRateChroma                             ( m_saoEncodingRateChroma );
  m_cTEncTop.setMaxNumOffsetsPerPic                               ( m_maxNumOffsetsPerPic);

  m_cTEncTop.setSaoCtuBoundary                                    ( m_saoCtuBoundary);
  m_cTEncTop.setResetEncoderStateAfterIRAP                        ( m_resetEncoderStateAfterIRAP );
  m_cTEncTop.setPCMInputBitDepthFlag                              ( m_bPCMInputBitDepthFlag);
  m_cTEncTop.setPCMFilterDisableFlag                              ( m_bPCMFilterDisableFlag);

  m_cTEncTop.setIntraSmoothingDisabledFlag                        (!m_enableIntraReferenceSmoothing );
  m_cTEncTop.setDecodedPictureHashSEIType                         ( m_decodedPictureHashSEIType );
  m_cTEncTop.setRecoveryPointSEIEnabled                           ( m_recoveryPointSEIEnabled );
  m_cTEncTop.setBufferingPeriodSEIEnabled                         ( m_bufferingPeriodSEIEnabled );
  m_cTEncTop.setPictureTimingSEIEnabled                           ( m_pictureTimingSEIEnabled );
  m_cTEncTop.setToneMappingInfoSEIEnabled                         ( m_toneMappingInfoSEIEnabled );
  m_cTEncTop.setTMISEIToneMapId                                   ( m_toneMapId );
  m_cTEncTop.setTMISEIToneMapCancelFlag                           ( m_toneMapCancelFlag );
  m_cTEncTop.setTMISEIToneMapPersistenceFlag                      ( m_toneMapPersistenceFlag );
  m_cTEncTop.setTMISEICodedDataBitDepth                           ( m_toneMapCodedDataBitDepth );
  m_cTEncTop.setTMISEITargetBitDepth                              ( m_toneMapTargetBitDepth );
  m_cTEncTop.setTMISEIModelID                                     ( m_toneMapModelId );
  m_cTEncTop.setTMISEIMinValue                                    ( m_toneMapMinValue );
  m_cTEncTop.setTMISEIMaxValue                                    ( m_toneMapMaxValue );
  m_cTEncTop.setTMISEISigmoidMidpoint                             ( m_sigmoidMidpoint );
  m_cTEncTop.setTMISEISigmoidWidth                                ( m_sigmoidWidth );
  m_cTEncTop.setTMISEIStartOfCodedInterva                         ( m_startOfCodedInterval );
  m_cTEncTop.setTMISEINumPivots                                   ( m_numPivots );
  m_cTEncTop.setTMISEICodedPivotValue                             ( m_codedPivotValue );
  m_cTEncTop.setTMISEITargetPivotValue                            ( m_targetPivotValue );
  m_cTEncTop.setTMISEICameraIsoSpeedIdc                           ( m_cameraIsoSpeedIdc );
  m_cTEncTop.setTMISEICameraIsoSpeedValue                         ( m_cameraIsoSpeedValue );
  m_cTEncTop.setTMISEIExposureIndexIdc                            ( m_exposureIndexIdc );
  m_cTEncTop.setTMISEIExposureIndexValue                          ( m_exposureIndexValue );
  m_cTEncTop.setTMISEIExposureCompensationValueSignFlag           ( m_exposureCompensationValueSignFlag );
  m_cTEncTop.setTMISEIExposureCompensationValueNumerator          ( m_exposureCompensationValueNumerator );
  m_cTEncTop.setTMISEIExposureCompensationValueDenomIdc           ( m_exposureCompensationValueDenomIdc );
  m_cTEncTop.setTMISEIRefScreenLuminanceWhite                     ( m_refScreenLuminanceWhite );
  m_cTEncTop.setTMISEIExtendedRangeWhiteLevel                     ( m_extendedRangeWhiteLevel );
  m_cTEncTop.setTMISEINominalBlackLevelLumaCodeValue              ( m_nominalBlackLevelLumaCodeValue );
  m_cTEncTop.setTMISEINominalWhiteLevelLumaCodeValue              ( m_nominalWhiteLevelLumaCodeValue );
  m_cTEncTop.setTMISEIExtendedWhiteLevelLumaCodeValue             ( m_extendedWhiteLevelLumaCodeValue );
  m_cTEncTop.setChromaResamplingFilterHintEnabled                 ( m_chromaResamplingFilterSEIenabled );
  m_cTEncTop.setChromaResamplingHorFilterIdc                      ( m_chromaResamplingHorFilterIdc );
  m_cTEncTop.setChromaResamplingVerFilterIdc                      ( m_chromaResamplingVerFilterIdc );
  m_cTEncTop.setFramePackingArrangementSEIEnabled                 ( m_framePackingSEIEnabled );
  m_cTEncTop.setFramePackingArrangementSEIType                    ( m_framePackingSEIType );
  m_cTEncTop.setFramePackingArrangementSEIId                      ( m_framePackingSEIId );
  m_cTEncTop.setFramePackingArrangementSEIQuincunx                ( m_framePackingSEIQuincunx );
  m_cTEncTop.setFramePackingArrangementSEIInterpretation          ( m_framePackingSEIInterpretation );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIEnabled    ( m_segmentedRectFramePackingSEIEnabled );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEICancel     ( m_segmentedRectFramePackingSEICancel );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIType       ( m_segmentedRectFramePackingSEIType );
  m_cTEncTop.setSegmentedRectFramePackingArrangementSEIPersistence( m_segmentedRectFramePackingSEIPersistence );
  m_cTEncTop.setDisplayOrientationSEIAngle                        ( m_displayOrientationSEIAngle );
  m_cTEncTop.setTemporalLevel0IndexSEIEnabled                     ( m_temporalLevel0IndexSEIEnabled );
  m_cTEncTop.setGradualDecodingRefreshInfoEnabled                 ( m_gradualDecodingRefreshInfoEnabled );
  m_cTEncTop.setNoDisplaySEITLayer                                ( m_noDisplaySEITLayer );
  m_cTEncTop.setDecodingUnitInfoSEIEnabled                        ( m_decodingUnitInfoSEIEnabled );
  m_cTEncTop.setSOPDescriptionSEIEnabled                          ( m_SOPDescriptionSEIEnabled );
  m_cTEncTop.setScalableNestingSEIEnabled                         ( m_scalableNestingSEIEnabled );
#if JVET_AE0101_PHASE_INDICATION_SEI_MESSAGE
  m_cTEncTop.setPhaseIndicationSEIEnabledFullResolution            ( m_phaseIndicationSEIEnabledFullResolution );
  m_cTEncTop.setHorPhaseNumFullResolution                          ( m_piHorPhaseNumFullResolution );
  m_cTEncTop.setHorPhaseDenMinus1FullResolution                    ( m_piHorPhaseDenMinus1FullResolution );
  m_cTEncTop.setVerPhaseNumFullResolution                          ( m_piVerPhaseNumFullResolution );
  m_cTEncTop.setVerPhaseDenMinus1FullResolution                    ( m_piVerPhaseDenMinus1FullResolution );
#endif
#if JVET_AK0107_MODALITY_INFORMATION
  // Modality Information SEI
  m_cTEncTop.setMiSEIEnabled                                      (m_miSEIEnabled);
  m_cTEncTop.setMiCancelFlag                                      (m_miCancelFlag);
  m_cTEncTop.setMiPersistenceFlag                                 (m_miPersistenceFlag);
  m_cTEncTop.setMiModalityType                                    (m_miModalityType);
  m_cTEncTop.setMiSpectrumRangePresentFlag                        (m_miSpectrumRangePresentFlag);
  m_cTEncTop.setMiMinWavelengthMantissa                           (m_miMinWavelengthMantissa);
  m_cTEncTop.setMiMinWavelengthExponentPlus15                     (m_miMinWavelengthExponentPlus15);
  m_cTEncTop.setMiMaxWavelengthMantissa                           (m_miMaxWavelengthMantissa);
  m_cTEncTop.setMiMaxWavelengthExponentPlus15                     (m_miMaxWavelengthExponentPlus15);
#endif
  m_cTEncTop.setTMCTSSEIEnabled                                   ( m_tmctsSEIEnabled );
#if MCTS_ENC_CHECK
  m_cTEncTop.setTMCTSSEITileConstraint                            ( m_tmctsSEITileConstraint );
#endif
#if MCTS_EXTRACTION
  m_cTEncTop.setTMCTSExtractionSEIEnabled                         ( m_tmctsExtractionSEIEnabled);
#endif
  m_cTEncTop.setTimeCodeSEIEnabled                                ( m_timeCodeSEIEnabled );
  m_cTEncTop.setNumberOfTimeSets                                  ( m_timeCodeSEINumTs );
  for(Int i = 0; i < m_timeCodeSEINumTs; i++)
  {
    m_cTEncTop.setTimeSet(m_timeSetArray[i], i);
  }
  m_cTEncTop.setKneeSEIEnabled                                    ( m_kneeSEIEnabled );
  m_cTEncTop.setKneeFunctionInformationSEI                        ( m_kneeFunctionInformationSEI );
  m_cTEncTop.setCcvSEIEnabled                                     (m_ccvSEIEnabled);
  m_cTEncTop.setCcvSEICancelFlag                                  (m_ccvSEICancelFlag);
  m_cTEncTop.setCcvSEIPersistenceFlag                             (m_ccvSEIPersistenceFlag);
  
  m_cTEncTop.setCcvSEIEnabled                                     (m_ccvSEIEnabled);
  m_cTEncTop.setCcvSEICancelFlag                                  (m_ccvSEICancelFlag);
  m_cTEncTop.setCcvSEIPersistenceFlag                             (m_ccvSEIPersistenceFlag);
  m_cTEncTop.setCcvSEIPrimariesPresentFlag                        (m_ccvSEIPrimariesPresentFlag);
  m_cTEncTop.setCcvSEIMinLuminanceValuePresentFlag                (m_ccvSEIMinLuminanceValuePresentFlag);
  m_cTEncTop.setCcvSEIMaxLuminanceValuePresentFlag                (m_ccvSEIMaxLuminanceValuePresentFlag);
  m_cTEncTop.setCcvSEIAvgLuminanceValuePresentFlag                (m_ccvSEIAvgLuminanceValuePresentFlag);
  for(Int i = 0; i < MAX_NUM_COMPONENT; i++) {
    m_cTEncTop.setCcvSEIPrimariesX                                (m_ccvSEIPrimariesX[i], i);
    m_cTEncTop.setCcvSEIPrimariesY                                (m_ccvSEIPrimariesY[i], i);
  }
  m_cTEncTop.setCcvSEIMinLuminanceValue                           (m_ccvSEIMinLuminanceValue);
  m_cTEncTop.setCcvSEIMaxLuminanceValue                           (m_ccvSEIMaxLuminanceValue);
  m_cTEncTop.setCcvSEIAvgLuminanceValue                           (m_ccvSEIAvgLuminanceValue);
  m_cTEncTop.setErpSEIEnabled                                     ( m_erpSEIEnabled );           
  m_cTEncTop.setErpSEICancelFlag                                  ( m_erpSEICancelFlag );        
  m_cTEncTop.setErpSEIPersistenceFlag                             ( m_erpSEIPersistenceFlag );   
  m_cTEncTop.setErpSEIGuardBandFlag                               ( m_erpSEIGuardBandFlag );     
  m_cTEncTop.setErpSEIGuardBandType                               ( m_erpSEIGuardBandType );     
  m_cTEncTop.setErpSEILeftGuardBandWidth                          ( m_erpSEILeftGuardBandWidth );
  m_cTEncTop.setErpSEIRightGuardBandWidth                         ( m_erpSEIRightGuardBandWidth );
  m_cTEncTop.setSphereRotationSEIEnabled                          ( m_sphereRotationSEIEnabled );
  m_cTEncTop.setSphereRotationSEICancelFlag                       ( m_sphereRotationSEICancelFlag );
  m_cTEncTop.setSphereRotationSEIPersistenceFlag                  ( m_sphereRotationSEIPersistenceFlag );
  m_cTEncTop.setSphereRotationSEIYaw                              ( m_sphereRotationSEIYaw );
  m_cTEncTop.setSphereRotationSEIPitch                            ( m_sphereRotationSEIPitch );
  m_cTEncTop.setSphereRotationSEIRoll                             ( m_sphereRotationSEIRoll );
  m_cTEncTop.setOmniViewportSEIEnabled                            ( m_omniViewportSEIEnabled );          
  m_cTEncTop.setOmniViewportSEIId                                 ( m_omniViewportSEIId );               
  m_cTEncTop.setOmniViewportSEICancelFlag                         ( m_omniViewportSEICancelFlag );       
  m_cTEncTop.setOmniViewportSEIPersistenceFlag                    ( m_omniViewportSEIPersistenceFlag );  
  m_cTEncTop.setOmniViewportSEICntMinus1                          ( m_omniViewportSEICntMinus1 );        
  m_cTEncTop.setOmniViewportSEIAzimuthCentre                      ( m_omniViewportSEIAzimuthCentre );    
  m_cTEncTop.setOmniViewportSEIElevationCentre                    ( m_omniViewportSEIElevationCentre );  
  m_cTEncTop.setOmniViewportSEITiltCentre                         ( m_omniViewportSEITiltCentre );       
  m_cTEncTop.setOmniViewportSEIHorRange                           ( m_omniViewportSEIHorRange );         
  m_cTEncTop.setOmniViewportSEIVerRange                           ( m_omniViewportSEIVerRange );         
  m_cTEncTop.setGopBasedTemporalFilterEnabled                     ( m_gopBasedTemporalFilterEnabled );
#if JVET_Y0077_BIM
  m_cTEncTop.setBIM                                               ( m_bimEnabled );
#endif
  m_cTEncTop.setCmpSEIEnabled                                     (m_cmpSEIEnabled);
  m_cTEncTop.setCmpSEICmpCancelFlag                               (m_cmpSEICmpCancelFlag);
  m_cTEncTop.setCmpSEICmpPersistenceFlag                          (m_cmpSEICmpPersistenceFlag);
  m_cTEncTop.setRwpSEIEnabled                                     (m_rwpSEIEnabled);
  m_cTEncTop.setRwpSEIRwpCancelFlag                               (m_rwpSEIRwpCancelFlag);
  m_cTEncTop.setRwpSEIRwpPersistenceFlag                          (m_rwpSEIRwpPersistenceFlag);
  m_cTEncTop.setRwpSEIConstituentPictureMatchingFlag              (m_rwpSEIConstituentPictureMatchingFlag);
  m_cTEncTop.setRwpSEINumPackedRegions                            (m_rwpSEINumPackedRegions);
  m_cTEncTop.setRwpSEIProjPictureWidth                            (m_rwpSEIProjPictureWidth);
  m_cTEncTop.setRwpSEIProjPictureHeight                           (m_rwpSEIProjPictureHeight);
  m_cTEncTop.setRwpSEIPackedPictureWidth                          (m_rwpSEIPackedPictureWidth);
  m_cTEncTop.setRwpSEIPackedPictureHeight                         (m_rwpSEIPackedPictureHeight);
  m_cTEncTop.setRwpSEIRwpTransformType                            (m_rwpSEIRwpTransformType);
  m_cTEncTop.setRwpSEIRwpGuardBandFlag                            (m_rwpSEIRwpGuardBandFlag);
  m_cTEncTop.setRwpSEIProjRegionWidth                             (m_rwpSEIProjRegionWidth);
  m_cTEncTop.setRwpSEIProjRegionHeight                            (m_rwpSEIProjRegionHeight);
  m_cTEncTop.setRwpSEIRwpSEIProjRegionTop                         (m_rwpSEIRwpSEIProjRegionTop);
  m_cTEncTop.setRwpSEIProjRegionLeft                              (m_rwpSEIProjRegionLeft);
  m_cTEncTop.setRwpSEIPackedRegionWidth                           (m_rwpSEIPackedRegionWidth);
  m_cTEncTop.setRwpSEIPackedRegionHeight                          (m_rwpSEIPackedRegionHeight);
  m_cTEncTop.setRwpSEIPackedRegionTop                             (m_rwpSEIPackedRegionTop);
  m_cTEncTop.setRwpSEIPackedRegionLeft                            (m_rwpSEIPackedRegionLeft);
  m_cTEncTop.setRwpSEIRwpLeftGuardBandWidth                       (m_rwpSEIRwpLeftGuardBandWidth);
  m_cTEncTop.setRwpSEIRwpRightGuardBandWidth                      (m_rwpSEIRwpRightGuardBandWidth);
  m_cTEncTop.setRwpSEIRwpTopGuardBandHeight                       (m_rwpSEIRwpTopGuardBandHeight);
  m_cTEncTop.setRwpSEIRwpBottomGuardBandHeight                    (m_rwpSEIRwpBottomGuardBandHeight);
  m_cTEncTop.setRwpSEIRwpGuardBandNotUsedForPredFlag              (m_rwpSEIRwpGuardBandNotUsedForPredFlag);
  m_cTEncTop.setRwpSEIRwpGuardBandType                            (m_rwpSEIRwpGuardBandType);
#if SHUTTER_INTERVAL_SEI_MESSAGE
  m_cTEncTop.setSiiSEIEnabled                                     (m_siiSEIEnabled);
  m_cTEncTop.setSiiSEINumUnitsInShutterInterval                   (m_siiSEINumUnitsInShutterInterval);
  m_cTEncTop.setSiiSEITimeScale                                   (m_siiSEITimeScale);
  m_cTEncTop.setSiiSEISubLayerNumUnitsInSI                        (m_siiSEISubLayerNumUnitsInSI);
#endif
#if JVET_AL0061_ENCODER_OPTIMIZATION_INFORMATION_SEI
  m_cTEncTop.setEOISEIEnabled(m_eoiSEIEnabled);
  m_cTEncTop.setEOISEICancelFlag(m_eoiSEICancelFlag);
  m_cTEncTop.setEOISEIPersistenceFlag(m_eoiSEIPersistenceFlag);
  m_cTEncTop.setEOISEIForHumanViewingIdc(m_eoiSEIForHumanViewingIdc);
  m_cTEncTop.setEOISEIForMachineAnalysisIdc(m_eoiSEIForMachineAnalysisIdc);
  m_cTEncTop.setEOISEIType(m_eoiSEIType);
  m_cTEncTop.setEOISEIObjectBasedIdc(m_eoiSEIObjectBasedIdc);
  m_cTEncTop.setEOISEIQuantThresholdDelta(m_eoiSEIQuantThresholdDelta);
  m_cTEncTop.setEOISEIPicQuantObjectFlag(m_eoiSEIPicQuantObjectFlag);
  m_cTEncTop.setEOISEITemporalResamplingTypeFlag(m_eoiSEITemporalResamplingTypeFlag);
  m_cTEncTop.setEOISEINumIntPics(m_eoiSEINumIntPics);
  m_cTEncTop.setEOISEISrcPicFlag(m_eoiSEISrcPicFlag);
  m_cTEncTop.setEOISEIOrigPicDimensionsFlag(m_eoiSEIOrigPicDimensionsFlag);
  m_cTEncTop.setEOISEIOrigPicWidth(m_eoiSEIOrigPicWidth);
  m_cTEncTop.setEOISEIOrigPicHeight(m_eoiSEIOrigPicHeight);
  m_cTEncTop.setEOISEISpatialResamplingTypeFlag(m_eoiSEISpatialResamplingTypeFlag);
  m_cTEncTop.setEOISEIPrivacyProtectionTypeIdc(m_eoiSEIPrivacyProtectionTypeIdc);
  m_cTEncTop.setEOISEIPrivacyProtectedInfoType(m_eoiSEIPrivacyProtectedInfoType);
#endif
#if SEI_ENCODER_CONTROL
// film grain charcteristics
  m_cTEncTop.setFilmGrainCharactersticsSEIEnabled                 (m_fgcSEIEnabled);
  m_cTEncTop.setFilmGrainCharactersticsSEICancelFlag              (m_fgcSEICancelFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIPersistenceFlag         (m_fgcSEIPersistenceFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIModelID                 ((UChar)m_fgcSEIModelID);
  m_cTEncTop.setFilmGrainCharactersticsSEISepColourDescPresent    (m_fgcSEISepColourDescPresentFlag);
  m_cTEncTop.setFilmGrainCharactersticsSEIBlendingModeID          ((UChar)m_fgcSEIBlendingModeID);
  m_cTEncTop.setFilmGrainCharactersticsSEILog2ScaleFactor         ((UChar)m_fgcSEILog2ScaleFactor);
#if JVET_AL0339_SPATIAL_RESOLUTION_FOR_FGC_SEI
  m_cTEncTop.setFilmGrainCharactersticsSEIPicWidthInLumaSamples   (m_fgcSEIPicWidthInLumaSamples);
  m_cTEncTop.setFilmGrainCharactersticsSEIPicHeightInLumaSamples  (m_fgcSEIPicHeightInLumaSamples);
#endif
#if JVET_X0048_X0103_FILM_GRAIN
  m_cTEncTop.setFilmGrainAnalysisEnabled                          (m_fgcSEIAnalysisEnabled);
  m_cTEncTop.setFilmGrainExternalMask                             (m_fgcSEIExternalMask);
  m_cTEncTop.setFilmGrainExternalDenoised                         (m_fgcSEIExternalDenoised);
  m_cTEncTop.setFilmGrainCharactersticsSEIPerPictureSEI           (m_fgcSEIPerPictureSEI);
#endif
  for (Int i = 0; i < MAX_NUM_COMPONENT; i++) {
    m_cTEncTop.setFGCSEICompModelPresent                          (m_fgcSEICompModelPresent[i], i);
#if JVET_X0048_X0103_FILM_GRAIN	
    if (m_fgcSEICompModelPresent[i])
    {
      m_cTEncTop.setFGCSEINumIntensityIntervalMinus1              ((UChar)m_fgcSEINumIntensityIntervalMinus1[i], i);
      m_cTEncTop.setFGCSEINumModelValuesMinus1                    ((UChar)m_fgcSEINumModelValuesMinus1[i], i);
      for (UInt j = 0; j <= m_fgcSEINumIntensityIntervalMinus1[i]; j++)
      {
        m_cTEncTop.setFGCSEIIntensityIntervalLowerBound           ((UChar)m_fgcSEIIntensityIntervalLowerBound[i][j], i, j);
        m_cTEncTop.setFGCSEIIntensityIntervalUpperBound           ((UChar)m_fgcSEIIntensityIntervalUpperBound[i][j], i, j);
        for (UInt k = 0; k <= m_fgcSEINumModelValuesMinus1[i]; k++)
        {
          m_cTEncTop.setFGCSEICompModelValue                      (m_fgcSEICompModelValue[i][j][k], i, j, k);
        }
      }
    }
#endif
  }
// content light level
  m_cTEncTop.setCLLSEIEnabled                                     (m_cllSEIEnabled);
  m_cTEncTop.setCLLSEIMaxContentLightLevel                        ((UShort)m_cllSEIMaxContentLevel);
  m_cTEncTop.setCLLSEIMaxPicAvgLightLevel                         ((UShort)m_cllSEIMaxPicAvgLevel);
// ambient viewing enviornment
  m_cTEncTop.setAmbientViewingEnvironmentSEIEnabled               (m_aveSEIEnabled);
  m_cTEncTop.setAmbientViewingEnvironmentSEIIlluminance           (m_aveSEIAmbientIlluminance);
  m_cTEncTop.setAmbientViewingEnvironmentSEIAmbientLightX         ((UShort)m_aveSEIAmbientLightX);
  m_cTEncTop.setAmbientViewingEnvironmentSEIAmbientLightY         ((UShort)m_aveSEIAmbientLightY);
#endif
  if (m_fisheyeVIdeoInfoSEIEnabled)
  {
    m_cTEncTop.setFviSEIEnabled(m_fisheyeVideoInfoSEI);
  }
  else
  {
    m_cTEncTop.setFviSEIDisabled();
  }
  m_cTEncTop.setColourRemapInfoSEIFileRoot                        ( m_colourRemapSEIFileRoot );
  m_cTEncTop.setMasteringDisplaySEI                               ( m_masteringDisplay );
  m_cTEncTop.setSEIAlternativeTransferCharacteristicsSEIEnable    ( m_preferredTransferCharacteristics>=0     );
  m_cTEncTop.setSEIPreferredTransferCharacteristics               ( UChar(m_preferredTransferCharacteristics) );
  m_cTEncTop.setSEIGreenMetadataInfoSEIEnable                     ( m_greenMetadataType > 0 );
  m_cTEncTop.setSEIGreenMetadataType                              ( UChar(m_greenMetadataType) );
  m_cTEncTop.setSEIXSDMetricType                                  ( UChar(m_xsdMetricType) );
#if NH_MV
  m_cTEncTop.setSeiMessages                                       ( &m_seiMessages );
#endif
  m_cTEncTop.setRegionalNestingSEIFileRoot                        ( m_regionalNestingSEIFileRoot );
  m_cTEncTop.setAnnotatedRegionSEIFileRoot                        (m_arSEIFileRoot);
  m_cTEncTop.setTileUniformSpacingFlag                            ( m_tileUniformSpacingFlag );
  m_cTEncTop.setNumColumnsMinus1                                  ( m_numTileColumnsMinus1 );
  m_cTEncTop.setNumRowsMinus1                                     ( m_numTileRowsMinus1 );
  if(!m_tileUniformSpacingFlag)
  {
    m_cTEncTop.setColumnWidth                                     ( m_tileColumnWidth );
    m_cTEncTop.setRowHeight                                       ( m_tileRowHeight );
  }
  m_cTEncTop.xCheckGSParameters();
  Int uiTilesCount = (m_numTileRowsMinus1+1) * (m_numTileColumnsMinus1+1);
  if(uiTilesCount == 1)
  {
    m_bLFCrossTileBoundaryFlag = true;
  }
  m_cTEncTop.setLFCrossTileBoundaryFlag                           ( m_bLFCrossTileBoundaryFlag );
  m_cTEncTop.setEntropyCodingSyncEnabledFlag                      ( m_entropyCodingSyncEnabledFlag );
  m_cTEncTop.setTMVPModeId                                        ( m_TMVPModeId );
  m_cTEncTop.setUseScalingListId                                  ( m_useScalingListId  );
  m_cTEncTop.setScalingListFileName                               ( m_scalingListFileName );
  m_cTEncTop.setSignDataHidingEnabledFlag                         ( m_signDataHidingEnabledFlag);

#if KWU_RC_VIEWRC_E0227 || KWU_RC_MADPRED_E0227
  if(!m_cTEncTop.getIsDepth())    //only for texture
  {
    m_cTEncTop.setUseRateCtrl                                     ( m_RCEnableRateControl );
  }
  else
  {
    m_cTEncTop.setUseRateCtrl                                     ( 0 );
  }
#else
  m_cTEncTop.setUseRateCtrl                                       ( m_RCEnableRateControl );
#endif
#if !KWU_RC_VIEWRC_E0227
  m_cTEncTop.setTargetBitrate                                     ( m_RCTargetBitrate );
#endif
  m_cTEncTop.setKeepHierBit                                       ( m_RCKeepHierarchicalBit );
  m_cTEncTop.setLCULevelRC                                        ( m_RCLCULevelRC );
  m_cTEncTop.setUseLCUSeparateModel                               ( m_RCUseLCUSeparateModel );
  m_cTEncTop.setInitialQP                                         ( m_RCInitialQP );
  m_cTEncTop.setForceIntraQP                                      ( m_RCForceIntraQP );
  m_cTEncTop.setCpbSaturationEnabled                              ( m_RCCpbSaturationEnabled );
  m_cTEncTop.setCpbSize                                           ( m_RCCpbSize );
  m_cTEncTop.setInitialCpbFullness                                ( m_RCInitialCpbFullness );
      
#if KWU_RC_MADPRED_E0227
  if(m_cTEncTop.getUseRateCtrl() && !m_cTEncTop.getIsDepth())
  {
    m_cTEncTop.setUseDepthMADPred(layerIdInVps ? m_depthMADPred       : 0);
    if(m_cTEncTop.getUseDepthMADPred())
    {
      m_cTEncTop.setCamParam(&m_cCameraData);
    }
  }
#endif
#if KWU_RC_VIEWRC_E0227
  if(m_cTEncTop.getUseRateCtrl() && !m_cTEncTop.getIsDepth())
  {
    m_cTEncTop.setUseViewWiseRateCtrl(m_viewWiseRateCtrl);
    if(m_iNumberOfViews == 1)
    {
      if(m_viewWiseRateCtrl)
      {
        m_cTEncTop.setTargetBitrate(m_viewTargetBits[layerIdInVps>>1]);
      }
      else
      {
        m_cTEncTop.setTargetBitrate       ( m_RCTargetBitrate );
      }
    }
    else
    {
      if(m_viewWiseRateCtrl)
      {
        m_cTEncTop.setTargetBitrate(m_viewTargetBits[layerIdInVps>>1]);
      }
      else
      {
        if(m_iNumberOfViews == 2)
        {
          if(m_cTEncTop.getViewId() == 0)
          {
            m_cTEncTop.setTargetBitrate              ( (m_RCTargetBitrate*80)/100 );
          }
          else if(m_cTEncTop.getViewId() == 1)
          {
            m_cTEncTop.setTargetBitrate              ( (m_RCTargetBitrate*20)/100 );
          }
        }
        else if(m_iNumberOfViews == 3)
        {
          if(m_cTEncTop.getViewId() == 0)
          {
            m_cTEncTop.setTargetBitrate              ( (m_RCTargetBitrate*66)/100 );
          }
          else if(m_cTEncTop.getViewId() == 1)
          {
            m_cTEncTop.setTargetBitrate              ( (m_RCTargetBitrate*17)/100 );
          }
          else if(m_cTEncTop.getViewId() == 2)
          {
            m_cTEncTop.setTargetBitrate              ( (m_RCTargetBitrate*17)/100 );
          }
        }
        else
        {
          m_cTEncTop.setTargetBitrate              ( m_RCTargetBitrate );
        }
      }
    }
  }
#endif
  m_cTEncTop.setTransquantBypassEnabledFlag                       ( m_TransquantBypassEnabledFlag );
  m_cTEncTop.setCUTransquantBypassFlagForceValue                  ( m_CUTransquantBypassFlagForce );
  m_cTEncTop.setCostMode                                          ( m_costMode );
  m_cTEncTop.setUseRecalculateQPAccordingToLambda                 ( m_recalculateQPAccordingToLambda );
  m_cTEncTop.setUseStrongIntraSmoothing                           ( m_useStrongIntraSmoothing );
  m_cTEncTop.setActiveParameterSetsSEIEnabled                     ( m_activeParameterSetsSEIEnabled );
  m_cTEncTop.setVuiParametersPresentFlag                          ( m_vuiParametersPresentFlag );
  m_cTEncTop.setAspectRatioInfoPresentFlag                        ( m_aspectRatioInfoPresentFlag);
  m_cTEncTop.setAspectRatioIdc                                    ( m_aspectRatioIdc );
  m_cTEncTop.setSarWidth                                          ( m_sarWidth );
  m_cTEncTop.setSarHeight                                         ( m_sarHeight );
  m_cTEncTop.setOverscanInfoPresentFlag                           ( m_overscanInfoPresentFlag );
  m_cTEncTop.setOverscanAppropriateFlag                           ( m_overscanAppropriateFlag );
  m_cTEncTop.setVideoSignalTypePresentFlag                        ( m_videoSignalTypePresentFlag );
  m_cTEncTop.setVideoFormat                                       ( m_videoFormat );
  m_cTEncTop.setVideoFullRangeFlag                                ( m_videoFullRangeFlag );
  m_cTEncTop.setColourDescriptionPresentFlag                      ( m_colourDescriptionPresentFlag );
  m_cTEncTop.setColourPrimaries                                   ( m_colourPrimaries );
  m_cTEncTop.setTransferCharacteristics                           ( m_transferCharacteristics );
  m_cTEncTop.setMatrixCoefficients                                ( m_matrixCoefficients );
  m_cTEncTop.setChromaLocInfoPresentFlag                          ( m_chromaLocInfoPresentFlag );
  m_cTEncTop.setChromaSampleLocTypeTopField                       ( m_chromaSampleLocTypeTopField );
  m_cTEncTop.setChromaSampleLocTypeBottomField                    ( m_chromaSampleLocTypeBottomField );
  m_cTEncTop.setNeutralChromaIndicationFlag                       ( m_neutralChromaIndicationFlag );
  m_cTEncTop.setDefaultDisplayWindow                              ( m_defDispWinLeftOffset, m_defDispWinRightOffset, m_defDispWinTopOffset, m_defDispWinBottomOffset );
  m_cTEncTop.setFrameFieldInfoPresentFlag                         ( m_frameFieldInfoPresentFlag );
  m_cTEncTop.setPocProportionalToTimingFlag                       ( m_pocProportionalToTimingFlag );
  m_cTEncTop.setNumTicksPocDiffOneMinus1                          ( m_numTicksPocDiffOneMinus1    );
  m_cTEncTop.setBitstreamRestrictionFlag                          ( m_bitstreamRestrictionFlag );
  m_cTEncTop.setTilesFixedStructureFlag                           ( m_tilesFixedStructureFlag );
  m_cTEncTop.setMotionVectorsOverPicBoundariesFlag                ( m_motionVectorsOverPicBoundariesFlag );
  m_cTEncTop.setMinSpatialSegmentationIdc                         ( m_minSpatialSegmentationIdc );
  m_cTEncTop.setMaxBytesPerPicDenom                               ( m_maxBytesPerPicDenom );
  m_cTEncTop.setMaxBitsPerMinCuDenom                              ( m_maxBitsPerMinCuDenom );
  m_cTEncTop.setLog2MaxMvLengthHorizontal                         ( m_log2MaxMvLengthHorizontal );
  m_cTEncTop.setLog2MaxMvLengthVertical                           ( m_log2MaxMvLengthVertical );
  m_cTEncTop.setEfficientFieldIRAPEnabled                         ( m_bEfficientFieldIRAPEnabled );
  m_cTEncTop.setHarmonizeGopFirstFieldCoupleEnabled               ( m_bHarmonizeGopFirstFieldCoupleEnabled );

  m_cTEncTop.setSummaryOutFilename                                ( m_summaryOutFilename );
  m_cTEncTop.setSummaryPicFilenameBase                            ( m_summaryPicFilenameBase );
  m_cTEncTop.setSummaryVerboseness                                ( m_summaryVerboseness );

#if NH_MV
  }
#endif

#if !NH_MV
#if JCTVC_AD0021_SEI_MANIFEST
  m_cTEncTop.setSEIManifestSEIEnabled(m_SEIManifestSEIEnabled);
#endif
#if JCTVC_AD0021_SEI_PREFIX_INDICATION
  m_cTEncTop.setSEIPrefixIndicationSEIEnabled(m_SEIPrefixIndicationSEIEnabled);
#endif
#endif
#if JVET_AJ0207_GFV
  m_cTEncTop.setGenerativeFaceVideoSEIEnabled(m_generativeFaceVideoEnabled);
  m_cTEncTop.setGenerativeFaceVideoSEINumber(m_generativeFaceVideoSEINumber);
  m_cTEncTop.setGenerativeFaceVideoSEIId(m_generativeFaceVideoSEIId);
  m_cTEncTop.setGenerativeFaceVideoSEICnt(m_generativeFaceVideoSEICnt);
  m_cTEncTop.setGenerativeFaceVideoSEIBasePicFlag(m_generativeFaceVideoSEIBasePicFlag);
  m_cTEncTop.setGenerativeFaceVideoSEINNPresentFlag(m_generativeFaceVideoSEINNPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEINNModeIdc(m_generativeFaceVideoSEINNModeIdc);
  m_cTEncTop.setGenerativeFaceVideoSEINNTagURI(m_generativeFaceVideoSEINNTagURI);
  m_cTEncTop.setGenerativeFaceVideoSEINNURI(m_generativeFaceVideoSEINNURI);
  m_cTEncTop.setGenerativeFaceVideoSEIDrivePicFusionFlag(m_generativeFaceVideoSEIDrivePicFusionFlag);
  m_cTEncTop.setGenerativeFaceVideoSEIChromaKeyInfoPresentFlag(m_generativeFaceVideoSEIChromaKeyInfoPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEIChromaKeyValuePresentFlag(m_generativeFaceVideoSEIChromaKeyValuePresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEIChromaKeyValue(m_generativeFaceVideoSEIChromaKeyValue);
  m_cTEncTop.setGenerativeFaceVideoSEIChromaKeyThrPresentFlag(m_generativeFaceVideoSEIChromaKeyThrPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEIChromaKeyThrValue(m_generativeFaceVideoSEIChromaKeyThrValue);
  m_cTEncTop.setGenerativeFaceVideoSEILowConfidenceFaceParameterFlag(m_generativeFaceVideoSEILowConfidenceFaceParameterFlag);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinatePresentFlag(m_generativeFaceVideoSEICoordinatePresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinateQuantizationFactor(m_generativeFaceVideoSEICoordinateQuantizationFactor);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinatePredFlag(m_generativeFaceVideoSEICoordinatePredFlag);
  m_cTEncTop.setGenerativeFaceVideoSEI3DCoordinateFlag(m_generativeFaceVideoSEI3DCoordinateFlag);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinatePointNum(m_generativeFaceVideoSEICoordinatePointNum);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinateXTesonr(m_generativeFaceVideoSEICoordinateXTesonr);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinateYTesonr(m_generativeFaceVideoSEICoordinateYTesonr);
  m_cTEncTop.setGenerativeFaceVideoSEIZCoordinateMaxValue(m_generativeFaceVideoSEIZCoordinateMaxValue);
  m_cTEncTop.setGenerativeFaceVideoSEICoordinateZTesonr(m_generativeFaceVideoSEICoordinateZTesonr);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixPresentFlag(m_generativeFaceVideoSEIMatrixPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixElementPrecisionFactor(m_generativeFaceVideoSEIMatrixElementPrecisionFactor);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixPredFlag(m_generativeFaceVideoSEIMatrixPredFlag);
  m_cTEncTop.setGenerativeFaceVideoSEINumMatricestoNumKpsFlag(m_generativeFaceVideoSEINumMatricestoNumKpsFlag);
  m_cTEncTop.setGenerativeFaceVideoSEINumMatricesInfo(m_generativeFaceVideoSEINumMatricesInfo);
  m_cTEncTop.setGenerativeFaceVideoSEINumMatrixType(m_generativeFaceVideoSEINumMatrixType);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixTypeIdx(m_generativeFaceVideoSEIMatrixTypeIdx);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrix3DSpaceFlag(m_generativeFaceVideoSEIMatrix3DSpaceFlag);
  m_cTEncTop.setGenerativeFaceVideoSEINumMatrices(m_generativeFaceVideoSEINumMatrices);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixWidth(m_generativeFaceVideoSEIMatrixWidth);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixHeight(m_generativeFaceVideoSEIMatrixHeight);
  m_cTEncTop.setGenerativeFaceVideoSEIMatrixElement(m_generativeFaceVideoSEIMatrixElement);
  m_cTEncTop.setGenerativeFaceVideoSEIPayloadFilename(m_generativeFaceVideoSEIPayloadFilename);
#endif
#if JVET_AK0239_GEFV
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIEnabled(m_generativeFaceVideoEnhancementEnabled);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINumber(m_generativeFaceVideoEnhancementSEINumber);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIId(m_generativeFaceVideoEnhancementSEIId);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIGFVCnt(m_generativeFaceVideoEnhancementSEIGFVCnt);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIGFVId(m_generativeFaceVideoEnhancementSEIGFVId);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIBasePicFlag(m_generativeFaceVideoEnhancementSEIBasePicFlag);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINNPresentFlag(m_generativeFaceVideoEnhancementSEINNPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINNModeIdc(m_generativeFaceVideoEnhancementSEINNModeIdc);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINNTagURI(m_generativeFaceVideoEnhancementSEINNTagURI);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINNURI(m_generativeFaceVideoEnhancementSEINNURI);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor(m_generativeFaceVideoEnhancementSEIMatrixElementPrecisionFactor);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixPredFlag(m_generativeFaceVideoEnhancementSEIMatrixPredFlag);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixPresentFlag(m_generativeFaceVideoEnhancementSEIMatrixPresentFlag);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEINumMatrices(m_generativeFaceVideoEnhancementSEINumMatrices);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixWidth(m_generativeFaceVideoEnhancementSEIMatrixWidth);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixHeight(m_generativeFaceVideoEnhancementSEIMatrixHeight);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIMatrixElement(m_generativeFaceVideoEnhancementSEIMatrixElement);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPayloadFilename(m_generativeFaceVideoEnhancementSEIPayloadFilename);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilPresentIdx(m_generativeFaceVideoEnhancementSEIPupilPresentIdx);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor(m_generativeFaceVideoEnhancementSEIPupilCoordinatePrecisionFactor);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX(m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateX);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY(m_generativeFaceVideoEnhancementSEIPupilLeftEyeCoordinateY);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX(m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateX);
  m_cTEncTop.setGenerativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY(m_generativeFaceVideoEnhancementSEIPupilRightEyeCoordinateY);
#endif
#if JVET_AK2006_SPTI_SEI_MESSAGE
  m_cTEncTop.setSptiSEIEnabled(m_sptiSEIEnabled);
  if (m_sptiSEIEnabled) 
  {
    m_cTEncTop.setmSptiSEISourceTimingEqualsOutputTimingFlag(m_sptiSourceTimingEqualsOutputTimingFlag);
    m_cTEncTop.setmSptiSEISourceType(m_sptiSourceType);
    m_cTEncTop.setmSptiSEITimeScale(m_sptiTimeScale);
    m_cTEncTop.setmSptiSEINumUnitsInElementalInterval(m_sptiNumUnitsInElementalInterval);
    m_cTEncTop.setmSptiSEIDirectionFlag(m_sptiDirectionFlag);
  }
#endif


}

Void TAppEncTop::xCreateLib()
{
#if NH_MV
  // initialize global variables
  initROM();

  for( Int layer=0; layer < m_numberOfLayers; layer++)
  {
    Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layer];
    m_acTVideoIOYuvInputFileList[layer]->open( m_pchInputFileList[layer],     false, &m_inputBitDepths[repFormatIdx][0], &m_MSBExtendedBitDepths[repFormatIdx][0], &m_internalBitDepths[repFormatIdx][0] );  // read  mode
    m_acTVideoIOYuvInputFileList[layer]->skipFrames( m_FrameSkip, m_iSourceWidths[repFormatIdx] - m_aiPads[repFormatIdx][0], m_iSourceHeights[repFormatIdx] - m_aiPads[repFormatIdx][1], m_InputChromaFormatIDC[repFormatIdx]);

    if (m_pchReconFileList[layer])
    {
      m_acTVideoIOYuvReconFileList[layer]->open( m_pchReconFileList[layer], true, &m_outputBitDepths[repFormatIdx][0], &m_outputBitDepths[repFormatIdx][0], &m_internalBitDepths[repFormatIdx][0]);  // write mode
    }
      
#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
    {
      m_cTVideoIOYuvSIIPreFileList[layer]->open( m_shutterIntervalPreFileName, true, &m_outputBitDepths[repFormatIdx][0], &m_outputBitDepths[repFormatIdx][0], &m_internalBitDepths[repFormatIdx][0]);  // write mode
    }
#endif
    m_acTEncTopList[layer]->create();
  }
#else
  // Video I/O
  m_cTVideoIOYuvInputFile.open( m_inputFileName,     false, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth );  // read  mode
  m_cTVideoIOYuvInputFile.skipFrames(m_FrameSkip, m_inputFileWidth, m_inputFileHeight, m_InputChromaFormatIDC);

  if (!m_reconFileName.empty())
  {
    m_cTVideoIOYuvReconFile.open(m_reconFileName, true, m_outputBitDepth, m_outputBitDepth, m_internalBitDepth);  // write mode
  }
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
  {
    m_cTVideoIOYuvSIIPreFile.open(m_shutterIntervalPreFileName, true, m_outputBitDepth, m_outputBitDepth, m_internalBitDepth);  // write mode
  }
#endif

  // Neo Decoder
  m_cTEncTop.create();
#endif
}

Void TAppEncTop::xDestroyLib()
{
#if NH_MV
  // destroy ROM
  destroyROM();

  for(Int layer=0; layer<m_numberOfLayers; layer++)
  {
    m_acTVideoIOYuvInputFileList[layer]->close();
    m_acTVideoIOYuvReconFileList[layer]->close();
#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
    {
      m_cTVideoIOYuvSIIPreFileList[layer]->close();
    }
#endif
    delete m_acTVideoIOYuvInputFileList[layer] ;
    m_acTVideoIOYuvInputFileList[layer] = NULL;
    delete m_acTVideoIOYuvReconFileList[layer] ;
    m_acTVideoIOYuvReconFileList[layer] = NULL;
#if SHUTTER_INTERVAL_SEI_PROCESSING
    delete m_cTVideoIOYuvSIIPreFileList[layer] ;
    m_cTVideoIOYuvSIIPreFileList[layer] = NULL;
#endif
    m_acTEncTopList[layer]->deletePicBuffer();
    m_acTEncTopList[layer]->destroy();
    delete m_acTEncTopList[layer] ;
    m_acTEncTopList[layer] = NULL;
    delete m_cListPicYuvRec[layer] ;
    m_cListPicYuvRec[layer] = NULL;
  }
#else
  // Video I/O
  m_cTVideoIOYuvInputFile.close();
  m_cTVideoIOYuvReconFile.close();
#if SHUTTER_INTERVAL_SEI_PROCESSING
  if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
  {
    m_cTVideoIOYuvSIIPreFile.close();
  }
#endif

  // Neo Decoder
  m_cTEncTop.destroy();
#endif
}

Void TAppEncTop::xInitLib(Bool isFieldCoding)
{
#if NH_MV
  for(Int layer=0; layer<m_numberOfLayers; layer++)
  {
#if KWU_RC_MADPRED_E0227
    m_acTEncTopList[layer]->init( isFieldCoding, this );
#else
    m_acTEncTopList[layer]->init( isFieldCoding );
#endif
  }
#else
  m_cTEncTop.init(isFieldCoding);
#endif
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
 - create internal class
 - initialize internal variable
 - until the end of input YUV file, call encoding function in TEncTop class
 - delete allocated buffers
 - destroy internal class
 .
 */
Void TAppEncTop::encode()
{
  fstream bitstreamFile(m_bitstreamFileName.c_str(), fstream::binary | fstream::out);
  if (!bitstreamFile)
  {
    fprintf(stderr, "\nfailed to open bitstream file `%s' for writing\n", m_bitstreamFileName.c_str());
    exit(EXIT_FAILURE);
  }

#if !NH_MV
  TComPicYuv*       pcPicYuvOrg = new TComPicYuv;
#endif
  TComPicYuv*       pcPicYuvRec = NULL;

#if JVET_X0048_X0103_FILM_GRAIN
  TComPicYuv* m_filteredOrgPicForFG;
  if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
  {
    m_filteredOrgPicForFG = new TComPicYuv;
#if !NH_MV
    m_filteredOrgPicForFG->create(m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true);
#else
    // NOTE: Use the first item of the parameters to avoid the build error for MV-HEVC encoding.
    m_filteredOrgPicForFG->create(m_iSourceWidths[0], m_iSourceHeights[0], m_chromaFormatIDCs[0], m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth[0], true);
#endif
  }
  else
  {
    m_filteredOrgPicForFG = NULL;
  }
#endif

  // initialize internal class & member variables
  xInitLibCfg();
  xCreateLib();
  xInitLib(m_isField);
  printChromaFormat();

  // main encoder loop
#if NH_MV
  Bool  allEos = false;
  std::vector<Bool>  eos ;
  std::vector<Bool>  flush ;
  
  Int gopSize    = 1;
  Int maxGopSize = 0;
  maxGopSize = (std::max)(maxGopSize, m_acTEncTopList[0]->getGOPSize());
  
  for(Int layer=0; layer < m_numberOfLayers; layer++ )
  {
    eos  .push_back( false );
    flush.push_back( false );
  }
#else
  Int   iNumEncoded = 0;
  Bool  bEos = false;
#endif

  const InputColourSpaceConversion ipCSC  =  m_inputColourSpaceConvert;
  const InputColourSpaceConversion snrCSC = (!m_snrInternalColourSpace) ? m_inputColourSpaceConvert : IPCOLOURSPACE_UNCHANGED;

  list<AccessUnit> outputAccessUnits; ///< list of access units to write out.  is populated by the encoding process

#if NH_MV
  std::vector<TComPicYuv*> picYuvOrg    ( m_numRepFormats );
  std::vector<TComPicYuv > picYuvTrueOrg( m_numRepFormats );
  for (Int d = 0; d < m_numRepFormats ; d++)
  {
    picYuvOrg[d] = new TComPicYuv;
    picYuvOrg[d]   ->create( m_iSourceWidths[d], m_isField ? m_iSourceHeightOrgs[d] : m_iSourceHeights[d], m_chromaFormatIDCs[d], m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth[d], true );
    picYuvTrueOrg[d].create( m_iSourceWidths[d], m_isField ? m_iSourceHeightOrgs[d] : m_iSourceHeights[d], m_chromaFormatIDCs[d], m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth[d], true );
  }
#else
  TComPicYuv cPicYuvTrueOrg;

  // allocate original YUV buffer
  if( m_isField )
  {
    pcPicYuvOrg->create  ( m_sourceWidth, m_sourceHeightOrg, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
    cPicYuvTrueOrg.create(m_sourceWidth, m_sourceHeightOrg, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true);
  }
  else
  {
    pcPicYuvOrg->create  ( m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
    cPicYuvTrueOrg.create(m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
  }
#endif

#if EXTENSION_360_VIDEO
  TExt360AppEncTop           ext360(*this, m_cTEncTop.getGOPEncoder()->getExt360Data(), *(m_cTEncTop.getGOPEncoder()), *pcPicYuvOrg);
#endif

#if !NH_MV
  TEncTemporalFilter temporalFilter;
#if JVET_Y0077_BIM
  if ( m_gopBasedTemporalFilterEnabled || m_bimEnabled )
#else
  if (m_gopBasedTemporalFilterEnabled)
#endif
  {
    temporalFilter.init(m_FrameSkip, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth, m_sourceWidth, m_sourceHeight,
      m_sourcePadding, m_framesToBeEncoded, m_bClipInputVideoToRec709Range, m_inputFileName, m_chromaFormatIDC,
      m_inputColourSpaceConvert, m_iQP, m_iGOPSize, m_gopBasedTemporalFilterStrengths,
      m_gopBasedTemporalFilterPastRefs, m_gopBasedTemporalFilterFutureRefs,
#if !JVET_Y0077_BIM
      m_firstValidFrame, m_lastValidFrame);
#else
      m_firstValidFrame, m_lastValidFrame,
      m_gopBasedTemporalFilterEnabled, m_cTEncTop.getAdaptQPmap(), m_bimEnabled);
#endif
  }
#if JVET_X0048_X0103_FILM_GRAIN
  TEncTemporalFilter m_temporalFilterForFG;
  if ( m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty() )
  {
    int  filteredFrame                 = 0;
    if ( m_iIntraPeriod < 1 )
      filteredFrame = 2 * m_iFrameRate;
    else
      filteredFrame = m_iIntraPeriod;

    map<int, double> filteredFramesAndStrengths = { { filteredFrame, 1.5 } };   // TODO: adjust MCTF and MCTF strenght

    m_temporalFilterForFG.init(m_FrameSkip, m_inputBitDepth, m_MSBExtendedBitDepth, m_internalBitDepth, m_sourceWidth, m_sourceHeight,
      m_sourcePadding, m_framesToBeEncoded, m_bClipInputVideoToRec709Range, m_inputFileName, m_chromaFormatIDC,
      m_inputColourSpaceConvert, m_iQP, m_iGOPSize, filteredFramesAndStrengths,
      m_gopBasedTemporalFilterPastRefs, m_gopBasedTemporalFilterFutureRefs,
#if !JVET_Y0077_BIM
      m_firstValidFrame, m_lastValidFrame);
#else
      m_firstValidFrame, m_lastValidFrame,
      m_gopBasedTemporalFilterEnabled, m_cTEncTop.getAdaptQPmap(), m_bimEnabled);
#endif
  }
#endif
#endif

#if NH_MV
  while ( (m_targetEncLayerIdList.size() != 0 ) && !allEos )
  {
    for(Int layer=0; layer < m_numberOfLayers; layer++ )
    {
      Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layer];

      TComPicYuv* pcPicYuvOrg    =  picYuvOrg    [ repFormatIdx ];
      TComPicYuv& cPicYuvTrueOrg =  picYuvTrueOrg[ repFormatIdx ];

      if (!xLayerIdInTargetEncLayerIdList( m_vps->getLayerIdInNuh( layer ) ))
      {
        continue;
      }

      Int frmCnt = 0;
      while ( !eos[layer] && !(frmCnt == gopSize))
      {
        // get buffers
        xGetBuffer(pcPicYuvRec, layer);

        // read input YUV file
        m_acTVideoIOYuvInputFileList[layer]->read      ( pcPicYuvOrg, &cPicYuvTrueOrg, ipCSC, &m_aiPads[repFormatIdx][0], m_InputChromaFormatIDC[repFormatIdx] );
        m_acTEncTopList             [layer]->initNewPic( pcPicYuvOrg );

        // increase number of received frames
        m_frameRcvd[layer]++;
        
        frmCnt++;

        eos[layer] = (m_frameRcvd[layer] == m_framesToBeEncoded);
        allEos = allEos||eos[layer];

        // if end of file (which is only detected on a read failure) flush the encoder of any queued pictures
        if (m_acTVideoIOYuvInputFileList[layer]->isEof())
        {
          flush          [layer] = true;
          eos            [layer] = true;
          m_frameRcvd    [layer]--;
          m_acTEncTopList[layer]->setFramesToBeEncoded(m_frameRcvd[layer]);
        }
      }
    }
    for ( Int gopId=0; gopId < gopSize; gopId++ )
    {
      for(Int layer=0; layer < m_numberOfLayers; layer++ )
      {
        Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layer];
#if NH_MV
        TComPicYuv* pcPicYuvOrg    =  picYuvOrg    [ repFormatIdx ];
        TComPicYuv& cPicYuvTrueOrg =  picYuvTrueOrg[ repFormatIdx ];
#endif
        if (!xLayerIdInTargetEncLayerIdList( m_vps->getLayerIdInNuh( layer ) ))
        {
          continue;
        }

        Int   iNumEncoded = 0;

        // call encoding function for one frame
        m_acTEncTopList[layer]->encode( eos[layer], flush[layer] ? 0 : pcPicYuvOrg, flush[layer] ? 0 : &cPicYuvTrueOrg, ipCSC, snrCSC, *m_cListPicYuvRec[layer], outputAccessUnits, iNumEncoded, gopId );
        xWriteOutput(bitstreamFile, iNumEncoded, outputAccessUnits, layer);
        outputAccessUnits.clear();
      }
    }

    gopSize = maxGopSize;
  }
  for(Int layer=0; layer < m_numberOfLayers; layer++ )
  {
    if (!xLayerIdInTargetEncLayerIdList( m_vps->getLayerIdInNuh( layer ) ))
    {
      continue;
    }
    m_acTEncTopList[layer]->printSummary(m_isField);
  }
#else

  while ( !bEos )
  {
    // get buffers
    xGetBuffer(pcPicYuvRec);

    // read input YUV file
#if EXTENSION_360_VIDEO
    if (ext360.isEnabled())
    {
      ext360.read(m_cTVideoIOYuvInputFile, *pcPicYuvOrg, cPicYuvTrueOrg, ipCSC);
    }
    else
    {
      m_cTVideoIOYuvInputFile.read( pcPicYuvOrg, &cPicYuvTrueOrg, ipCSC, m_sourcePadding, m_InputChromaFormatIDC, m_bClipInputVideoToRec709Range );
    }
#else
    m_cTVideoIOYuvInputFile.read( pcPicYuvOrg, &cPicYuvTrueOrg, ipCSC, m_sourcePadding, m_InputChromaFormatIDC, m_bClipInputVideoToRec709Range );
#endif

#if JVET_X0048_X0103_FILM_GRAIN
    if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
    {
      pcPicYuvOrg->copyToPic(m_filteredOrgPicForFG);
      m_temporalFilterForFG.filter(m_filteredOrgPicForFG, m_iFrameRcvd);
    }
#endif

#if JVET_Y0077_BIM
    if ( m_gopBasedTemporalFilterEnabled || m_bimEnabled )
#else
    if (m_gopBasedTemporalFilterEnabled)
#endif
    {
      temporalFilter.filter(pcPicYuvOrg, m_iFrameRcvd);
    }

    // increase number of received frames
    m_iFrameRcvd++;

    bEos = (m_isField && (m_iFrameRcvd == (m_framesToBeEncoded >> 1) )) || ( !m_isField && (m_iFrameRcvd == m_framesToBeEncoded) );

    Bool flush = 0;
    // if end of file (which is only detected on a read failure) flush the encoder of any queued pictures
    if (m_cTVideoIOYuvInputFile.isEof())
    {
      flush = true;
      bEos = true;
      m_iFrameRcvd--;
      m_cTEncTop.setFramesToBeEncoded(m_iFrameRcvd);
    }

    // call encoding function for one frame
    if ( m_isField )
    {
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded, m_isTopFieldFirst );
    }
    else
    {
#if JVET_X0048_X0103_FILM_GRAIN
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, flush ? 0 : m_filteredOrgPicForFG, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded);
#else
      m_cTEncTop.encode( bEos, flush ? 0 : pcPicYuvOrg, flush ? 0 : &cPicYuvTrueOrg, ipCSC, snrCSC, m_cListPicYuvRec, outputAccessUnits, iNumEncoded );
#endif
    }

#if SHUTTER_INTERVAL_SEI_PROCESSING
    if (m_ShutterFilterEnable && !m_shutterIntervalPreFileName.empty())
    {
      m_cTVideoIOYuvSIIPreFile.write(pcPicYuvOrg, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom,
        NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
    }
#endif

    // write bistream to file if necessary
    if ( iNumEncoded > 0 )
    {
      xWriteOutput(bitstreamFile, iNumEncoded, outputAccessUnits);
      outputAccessUnits.clear();
    }
    // temporally skip frames
    if( m_temporalSubsampleRatio > 1 )
    {
      m_cTVideoIOYuvInputFile.skipFrames(m_temporalSubsampleRatio-1, m_inputFileWidth, m_inputFileHeight, m_InputChromaFormatIDC);
    }
  }

  m_cTEncTop.printSummary(m_isField);
#endif

#if NH_MV
  // delete original YUV buffer
  for (Int d = 0; d < m_numRepFormats; d++)
  {
    picYuvOrg[d]->destroy();
    delete picYuvOrg[d];
    picYuvOrg[d] = NULL;

    picYuvTrueOrg[d].destroy();
  }
#else
  // delete original YUV buffer
  pcPicYuvOrg->destroy();
  delete pcPicYuvOrg;
  pcPicYuvOrg = NULL;
#endif

#if JVET_X0048_X0103_FILM_GRAIN
  if (m_fgcSEIAnalysisEnabled && m_fgcSEIExternalDenoised.empty())
  {
    m_filteredOrgPicForFG->destroy();
    delete m_filteredOrgPicForFG;
    m_filteredOrgPicForFG = NULL;
  }
#endif

#if !NH_MV
  // delete used buffers in encoder class
  m_cTEncTop.deletePicBuffer();
  cPicYuvTrueOrg.destroy();
#endif

  // delete buffers & classes
  xDeleteBuffer();
  xDestroyLib();

  printRateSummary();

  return;
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - application has picture buffer list with size of GOP
 - picture buffer list acts as ring buffer
 - end of the list has the latest picture
 .
 */
#if NH_MV
Void TAppEncTop::xGetBuffer( TComPicYuv*& rpcPicYuvRec, UInt layer)
#else
Void TAppEncTop::xGetBuffer( TComPicYuv*& rpcPicYuvRec)
#endif
{
  assert( m_iGOPSize > 0 );

  // org. buffer
#if NH_MV
  Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layer];

  if ( m_cListPicYuvRec[layer]->size() == (UInt)m_iGOPSize )
  {
    rpcPicYuvRec = m_cListPicYuvRec[layer]->popFront();
#else
  if ( m_cListPicYuvRec.size() >= (UInt)m_iGOPSize ) // buffer will be 1 element longer when using field coding, to maintain first field whilst processing second.
  {
    rpcPicYuvRec = m_cListPicYuvRec.popFront();
#endif
  }
  else
  {
    rpcPicYuvRec = new TComPicYuv;
#if NH_MV
    rpcPicYuvRec->create( m_iSourceWidths[ repFormatIdx ], m_iSourceHeights[ repFormatIdx ], m_chromaFormatIDCs[ repFormatIdx ], m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth[repFormatIdx], true );
#else
    rpcPicYuvRec->create( m_sourceWidth, m_sourceHeight, m_chromaFormatIDC, m_uiMaxCUWidth, m_uiMaxCUHeight, m_uiMaxTotalCUDepth, true );
#endif

  }
#if NH_MV
  m_cListPicYuvRec[layer]->pushBack( rpcPicYuvRec );
#else
  m_cListPicYuvRec.pushBack( rpcPicYuvRec );
#endif
}

Void TAppEncTop::xDeleteBuffer( )
{
#if NH_MV
  for(Int layer=0; layer<m_cListPicYuvRec.size(); layer++)
  {
    if(m_cListPicYuvRec[layer])
    {
      TComList<TComPicYuv*>::iterator iterPicYuvRec  = m_cListPicYuvRec[layer]->begin();
      Int iSize = Int( m_cListPicYuvRec[layer]->size() );
#else
  TComList<TComPicYuv*>::iterator iterPicYuvRec  = m_cListPicYuvRec.begin();

  Int iSize = Int( m_cListPicYuvRec.size() );
#endif

  for ( Int i = 0; i < iSize; i++ )
  {
    TComPicYuv*  pcPicYuvRec  = *(iterPicYuvRec++);
    pcPicYuvRec->destroy();
    delete pcPicYuvRec; pcPicYuvRec = NULL;
  }
#if NH_MV
}
  }
#endif

}

/** 
  Write access units to output file.
  \param bitstreamFile  target bitstream file
  \param iNumEncoded    number of encoded frames
  \param accessUnits    list of access units to be written
 */
#if NH_MV
Void TAppEncTop::xWriteOutput(std::ostream& bitstreamFile, Int iNumEncoded, std::list<AccessUnit>& accessUnits, UInt layerIdx)
#else
Void TAppEncTop::xWriteOutput(std::ostream& bitstreamFile, Int iNumEncoded, const std::list<AccessUnit>& accessUnits)
#endif
{
  const InputColourSpaceConversion ipCSC = (!m_outputInternalColourSpace) ? m_inputColourSpaceConvert : IPCOLOURSPACE_UNCHANGED;

  if (m_isField)
  {
    //Reinterlace fields
    Int i;
#if NH_MV
    if( iNumEncoded > 0 )
    {
      TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec[layerIdx]->end();
#else
    TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec.end();
    list<AccessUnit>::const_iterator iterBitstream = accessUnits.begin();
#endif

    for ( i = 0; i < iNumEncoded; i++ )
    {
      --iterPicYuvRec;
    }

    for ( i = 0; i < iNumEncoded/2; i++ )
    {
      TComPicYuv*  pcPicYuvRecTop  = *(iterPicYuvRec++);
      TComPicYuv*  pcPicYuvRecBottom  = *(iterPicYuvRec++);

#if NH_MV
      if (m_pchReconFileList[layerIdx])
      {
        Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layerIdx];
        m_acTVideoIOYuvReconFileList[layerIdx]->write( pcPicYuvRecTop, pcPicYuvRecBottom, ipCSC, m_confWinLefts[repFormatIdx], m_confWinRights[repFormatIdx], m_confWinTops[repFormatIdx], m_confWinBottoms[repFormatIdx], NUM_CHROMA_FORMAT, m_isTopFieldFirst );
      }
    }
  }

  if( ! accessUnits.empty() )
  {
    list<AccessUnit>::iterator aUIter;
    for( aUIter = accessUnits.begin(); aUIter != accessUnits.end(); aUIter++ )
    {
      const vector<UInt>& stats = writeAnnexB(bitstreamFile, *aUIter);
      rateStatsAccum(*aUIter, stats);
    }
  }
#else
      if (!m_reconFileName.empty())
      {
        m_cTVideoIOYuvReconFile.write( pcPicYuvRecTop, pcPicYuvRecBottom, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom, NUM_CHROMA_FORMAT, m_isTopFieldFirst );
      }

      const AccessUnit& auTop = *(iterBitstream++);
      const vector<UInt>& statsTop = writeAnnexB(bitstreamFile, auTop);
      rateStatsAccum(auTop, statsTop);

      const AccessUnit& auBottom = *(iterBitstream++);
      const vector<UInt>& statsBottom = writeAnnexB(bitstreamFile, auBottom);
      rateStatsAccum(auBottom, statsBottom);
    }
#endif
  }
  else
  {
    Int i;
#if NH_MV
    if( iNumEncoded > 0 )
    {
      TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec[layerIdx]->end();
#else
    TComList<TComPicYuv*>::iterator iterPicYuvRec = m_cListPicYuvRec.end();
    list<AccessUnit>::const_iterator iterBitstream = accessUnits.begin();
#endif

    for ( i = 0; i < iNumEncoded; i++ )
    {
      --iterPicYuvRec;
    }

    for ( i = 0; i < iNumEncoded; i++ )
    {
      TComPicYuv*  pcPicYuvRec  = *(iterPicYuvRec++);
#if NH_MV
      Int repFormatIdx = m_layerIdxInVpsToRepFormatIdx[layerIdx];
      if (m_pchReconFileList[layerIdx])
      {
        m_acTVideoIOYuvReconFileList[layerIdx]->write( pcPicYuvRec, ipCSC, m_confWinLefts[repFormatIdx], m_confWinRights[repFormatIdx], m_confWinTops[repFormatIdx], m_confWinBottoms[repFormatIdx] );
      }
    }
  }
  if( ! accessUnits.empty() )
  {
    list<AccessUnit>::iterator aUIter;
    for( aUIter = accessUnits.begin(); aUIter != accessUnits.end(); aUIter++ )
    {
      const vector<unsigned>& stats = writeAnnexB(bitstreamFile, *aUIter);
      rateStatsAccum(*aUIter, stats);
    }
  }
#else
      if (!m_reconFileName.empty())
      {
        m_cTVideoIOYuvReconFile.write( pcPicYuvRec, ipCSC, m_confWinLeft, m_confWinRight, m_confWinTop, m_confWinBottom,
            NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range  );
      }

      const AccessUnit& au = *(iterBitstream++);
      const vector<UInt>& stats = writeAnnexB(bitstreamFile, au);
      rateStatsAccum(au, stats);
    }
#endif
  }
}

/**
 *
 */
Void TAppEncTop::rateStatsAccum(const AccessUnit& au, const std::vector<UInt>& annexBsizes)
{
  AccessUnit::const_iterator it_au = au.begin();
  vector<UInt>::const_iterator it_stats = annexBsizes.begin();

  for (; it_au != au.end(); it_au++, it_stats++)
  {
    switch ((*it_au)->m_nalUnitType)
    {
    case NAL_UNIT_CODED_SLICE_TRAIL_R:
    case NAL_UNIT_CODED_SLICE_TRAIL_N:
    case NAL_UNIT_CODED_SLICE_TSA_R:
    case NAL_UNIT_CODED_SLICE_TSA_N:
    case NAL_UNIT_CODED_SLICE_STSA_R:
    case NAL_UNIT_CODED_SLICE_STSA_N:
    case NAL_UNIT_CODED_SLICE_BLA_W_LP:
    case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
    case NAL_UNIT_CODED_SLICE_BLA_N_LP:
    case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
    case NAL_UNIT_CODED_SLICE_IDR_N_LP:
    case NAL_UNIT_CODED_SLICE_CRA:
    case NAL_UNIT_CODED_SLICE_RADL_N:
    case NAL_UNIT_CODED_SLICE_RADL_R:
    case NAL_UNIT_CODED_SLICE_RASL_N:
    case NAL_UNIT_CODED_SLICE_RASL_R:
    case NAL_UNIT_VPS:
    case NAL_UNIT_SPS:
    case NAL_UNIT_PPS:
      m_essentialBytes += *it_stats;
      break;
    default:
      break;
    }

    m_totalBytes += *it_stats;
  }
}

Void TAppEncTop::printRateSummary()
{
#if NH_MV
  Double time = (Double) m_frameRcvd[0] / m_iFrameRate * m_temporalSubsampleRatio;
  printf("\n");
#else
  Double time = (Double) m_iFrameRcvd / m_iFrameRate * m_temporalSubsampleRatio;
#endif
  printf("Bytes written to file: %u (%.3f kbps)\n", m_totalBytes, 0.008 * m_totalBytes / time);
  if (m_summaryVerboseness > 0)
  {
    printf("Bytes for SPS/PPS/Slice (Incl. Annex B): %u (%.3f kbps)\n", m_essentialBytes, 0.008 * m_essentialBytes / time);
  }
}

Void TAppEncTop::printChromaFormat()
{
#if NH_MV
  std::cout << "Input ChromaFormatIDC             : ";
  for (Int i = 0; i < m_numRepFormats; i++)
  {
    
    
    switch (m_InputChromaFormatIDC[i])
#else
  std::cout << std::setw(43) << "Input ChromaFormatIDC = ";
  switch (m_InputChromaFormatIDC)
#endif
  {
  case CHROMA_400:  std::cout << "  4:0:0"; break;
  case CHROMA_420:  std::cout << "  4:2:0"; break;
  case CHROMA_422:  std::cout << "  4:2:2"; break;
  case CHROMA_444:  std::cout << "  4:4:4"; break;
  default:
    std::cerr << "Invalid";
    exit(1);
  }
#if NH_MV
  std::cout << " ";
}
#endif

  std::cout << std::endl;

#if NH_MV
  std::cout << "Output (internal) ChromaFormatIDC : ";
  for (Int i = 0; i < m_numRepFormats; i++)
  {
    
    switch ( m_chromaFormatIDCs[i] )
#else
  std::cout << std::setw(43) << "Output (internal) ChromaFormatIDC = ";
  switch (m_cTEncTop.getChromaFormatIdc())
#endif
  {
  case CHROMA_400:  std::cout << "  4:0:0"; break;
  case CHROMA_420:  std::cout << "  4:2:0"; break;
  case CHROMA_422:  std::cout << "  4:2:2"; break;
  case CHROMA_444:  std::cout << "  4:4:4"; break;
  default:
    std::cerr << "Invalid";
    exit(1);
  }
#if NH_MV
    std::cout << " ";
  }
#endif
  std::cout << "\n" << std::endl;
}

#if NH_MV
Void TAppEncTop::xSetDimensionIdAndLength( TComVPS& vps )
{
  vps.setScalabilityMaskFlag( m_scalabilityMask );
  for( Int dim = 0; dim < m_dimIds.size(); dim++ )
  {
    vps.setDimensionIdLen( dim, m_dimensionIdLen[ dim ] );
    for( Int layer = 0; layer <= vps.getMaxLayersMinus1(); layer++ )
    {
      vps.setDimensionId( layer, dim, m_dimIds[ dim ][ layer ] );
    }
  }

  vps.initNumViews();
  Int maxViewId = xGetMax( m_viewId );

  Int viewIdLen = gCeilLog2( maxViewId + 1 );
  const Int maxViewIdLen = ( 1 << 4 ) - 1;
  assert( viewIdLen <= maxViewIdLen );
  vps.setViewIdLen( viewIdLen );
  for (Int i = 0; i < m_iNumberOfViews; i++)
  {
    vps.setViewIdVal( i, m_viewId[ i] );
  }

  assert( m_iNumberOfViews == vps.getNumViews() );
}

Void TAppEncTop::xSetDependencies( TComVPS& vps )
{
  // Direct dependency flags + dependency types
  for( Int depLayer = 1; depLayer < MAX_NUM_LAYERS; depLayer++ )
  {
    for( Int refLayer = 0; refLayer < MAX_NUM_LAYERS; refLayer++ )
    {
      vps.setDirectDependencyFlag( depLayer, refLayer, false);
      vps.setDirectDependencyType( depLayer, refLayer,    -1 );
    }
  }

  Int  defaultDirectDependencyType = -1;
  Bool defaultDirectDependencyFlag = false;

  Int directDepTypeLenMinus2 = 0;
  for( Int depLayer = 1; depLayer < m_numberOfLayers; depLayer++ )
  {
    Int numRefLayers = (Int) m_directRefLayers[depLayer].size();
    assert(  numRefLayers == (Int) m_dependencyTypes[depLayer].size() );
    for( Int i = 0; i < numRefLayers; i++ )
    {
      Int refLayer = m_directRefLayers[depLayer][i];
      vps.setDirectDependencyFlag( depLayer, refLayer, true);
      Int curDirectDependencyType = m_dependencyTypes[depLayer][i];
      directDepTypeLenMinus2 = std::max( directDepTypeLenMinus2, gCeilLog2( curDirectDependencyType + 1  ) - 2 );
      if ( defaultDirectDependencyType != -1 )
      {
        defaultDirectDependencyFlag = defaultDirectDependencyFlag && (curDirectDependencyType == defaultDirectDependencyType );
      }
      else
      {
        defaultDirectDependencyType = curDirectDependencyType;
        defaultDirectDependencyFlag = true;
      }
      
      vps.setDirectDependencyType( depLayer, refLayer, curDirectDependencyType);
    }
  }

  vps.setDefaultDirectDependencyFlag( defaultDirectDependencyFlag );
  vps.setDefaultDirectDependencyType( defaultDirectDependencyFlag ? defaultDirectDependencyType : -1 );

  assert( directDepTypeLenMinus2 <= 1 );
  vps.setDirectDepTypeLenMinus2( directDepTypeLenMinus2 );


  vps.setRefLayers();

  // Max sub layers, + presence flag
  Bool subLayersMaxMinus1PresentFlag = false;
  for (Int curLayerIdInVps = 0; curLayerIdInVps < m_numberOfLayers; curLayerIdInVps++ )
  {
    Int curSubLayersMaxMinus1 = 0;
    for( Int i = 0; i < getGOPSize(); i++ )
    {
      GOPEntry geCur =  xGetGopEntries(curLayerIdInVps)[i];
      curSubLayersMaxMinus1 = std::max( curSubLayersMaxMinus1, geCur.m_temporalId );
    }

    vps.setSubLayersVpsMaxMinus1( curLayerIdInVps, curSubLayersMaxMinus1 );
    subLayersMaxMinus1PresentFlag = subLayersMaxMinus1PresentFlag || ( curSubLayersMaxMinus1 != vps.getMaxSubLayersMinus1() );
  }

  vps.setVpsSubLayersMaxMinus1PresentFlag( subLayersMaxMinus1PresentFlag );

  // Max temporal id for inter layer reference pictures
  for ( Int refLayerIdInVps = 0; refLayerIdInVps < m_numberOfLayers; refLayerIdInVps++)
  {
    Int refLayerIdInNuh = vps.getLayerIdInNuh( refLayerIdInVps );
    for ( Int curLayerIdInVps = 1; curLayerIdInVps < m_numberOfLayers; curLayerIdInVps++)
    {
      Int curLayerIdInNuh = vps.getLayerIdInNuh( curLayerIdInVps );
      Int maxTid = -1;
      for( Int i = 0; i < ( getGOPSize() + 1); i++ )
      {
        GOPEntry geCur =  xGetGopEntries(curLayerIdInVps)[( i < getGOPSize()  ? i : MAX_GOP )];
        GOPEntry geRef =  xGetGopEntries(refLayerIdInVps)[( i < getGOPSize()  ? i : MAX_GOP )];
        for (Int j = 0; j < geCur.m_numActiveRefLayerPics; j++)
        {
          if ( vps.getIdDirectRefLayer( curLayerIdInNuh, geCur.m_interLayerPredLayerIdc[ j ] ) == refLayerIdInNuh )
          {
            Bool refLayerZero   = ( i == getGOPSize() ) && ( refLayerIdInVps == 0 );
            maxTid = std::max( maxTid, refLayerZero ? 0 : geRef.m_temporalId );
          }
        }
      }
    }  // Loop curLayerIdInVps
  } // Loop refLayerIdInVps

  // Max temporal id for inter layer reference pictures presence flag
  Bool maxTidRefPresentFlag = false;
  for ( Int refLayerIdInVps = 0; refLayerIdInVps < m_numberOfLayers; refLayerIdInVps++)
  {
    for ( Int curLayerIdInVps = 1; curLayerIdInVps < m_numberOfLayers; curLayerIdInVps++)
    {
        maxTidRefPresentFlag = maxTidRefPresentFlag || ( vps.getMaxTidIlRefPicsPlus1( refLayerIdInVps, curLayerIdInVps ) != 7 );
    }
  }
  vps.setMaxTidRefPresentFlag( maxTidRefPresentFlag );


  // Max one active ref layer flag
  Bool maxOneActiveRefLayerFlag = true;
  for ( Int layerIdInVps = 1; layerIdInVps < m_numberOfLayers && maxOneActiveRefLayerFlag; layerIdInVps++)
  {
    for( Int i = 0; i < ( getGOPSize() + 1) && maxOneActiveRefLayerFlag; i++ )
    {
      GOPEntry ge =  xGetGopEntries(layerIdInVps)[ ( i < getGOPSize()  ? i : MAX_GOP ) ];
      maxOneActiveRefLayerFlag =  maxOneActiveRefLayerFlag && (ge.m_numActiveRefLayerPics <= 1);
    }
  }

  vps.setMaxOneActiveRefLayerFlag( maxOneActiveRefLayerFlag );
  
  // Poc Lsb Not Present Flag
  for ( Int layerIdInVps = 1; layerIdInVps < m_numberOfLayers; layerIdInVps++)
  {
    if ( m_directRefLayers[ layerIdInVps ].size() == 0 )
    {
      vps.setPocLsbNotPresentFlag( layerIdInVps,  true );
    }
  }
  
  // All Ref layers active flag
  Bool allRefLayersActiveFlag = true;
  for ( Int layerIdInVps = 1; layerIdInVps < m_numberOfLayers && allRefLayersActiveFlag; layerIdInVps++)
  {
    Int layerIdInNuh = vps.getLayerIdInNuh( layerIdInVps );
    for( Int i = 0; i < ( getGOPSize() + 1) && allRefLayersActiveFlag; i++ )
    {
      GOPEntry ge =  xGetGopEntries(layerIdInVps)[ ( i < getGOPSize()  ? i : MAX_GOP ) ];
      Int tId = ge.m_temporalId;  // Should be equal for all layers.
      
      // check if all reference layers when allRefLayerActiveFlag is equal to 1 are reference layer pictures specified in the gop entry
      for (Int k = 0; k < vps.getNumDirectRefLayers( layerIdInNuh ) && allRefLayersActiveFlag; k++ )
      {
        Int refLayerIdInVps = vps.getLayerIdInVps( vps.getIdDirectRefLayer( layerIdInNuh , k ) );
        if ( vps.getSubLayersVpsMaxMinus1(refLayerIdInVps) >= tId  && ( tId == 0 || vps.getMaxTidIlRefPicsPlus1(refLayerIdInVps,layerIdInVps) > tId )  )
        {
          Bool gopEntryFoundFlag = false;
          for( Int l = 0; l < ge.m_numActiveRefLayerPics && !gopEntryFoundFlag; l++ )
          {
            gopEntryFoundFlag = gopEntryFoundFlag || ( ge.m_interLayerPredLayerIdc[l] == k );
          }
          allRefLayersActiveFlag = allRefLayersActiveFlag && gopEntryFoundFlag;
        }
      }

      // check if all inter layer reference pictures specified in the gop entry are valid reference layer pictures when allRefLayerActiveFlag is equal to 1
      // (Should actually always be true)
      Bool maxTidIlRefAndSubLayerMaxValidFlag = true;
      for( Int l = 0; l < ge.m_numActiveRefLayerPics; l++ )
      {
        Bool referenceLayerFoundFlag = false;
        for (Int k = 0; k < vps.getNumDirectRefLayers( layerIdInNuh ); k++ )
        {
          Int refLayerIdInVps = vps.getLayerIdInVps( vps.getIdDirectRefLayer( layerIdInNuh, k) );
          if ( vps.getSubLayersVpsMaxMinus1(refLayerIdInVps) >= tId  && ( tId == 0 || vps.getMaxTidIlRefPicsPlus1(refLayerIdInVps,layerIdInVps) > tId )  )
          {
            referenceLayerFoundFlag = referenceLayerFoundFlag || ( ge.m_interLayerPredLayerIdc[l] == k );
          }
        }
       maxTidIlRefAndSubLayerMaxValidFlag = maxTidIlRefAndSubLayerMaxValidFlag && referenceLayerFoundFlag;
      }
      assert ( maxTidIlRefAndSubLayerMaxValidFlag ); // Something wrong with MaxTidIlRefPicsPlus1 or SubLayersVpsMaxMinus1
    }
  }

  vps.setAllRefLayersActiveFlag( allRefLayersActiveFlag );
};


Void TAppEncTop::xSetTimingInfo( TComVPS& vps )
{
  vps.getTimingInfo()->setTimingInfoPresentFlag( false );
}

Void TAppEncTop::xSetHrdParameters( TComVPS& vps )
{
  vps.createHrdParamBuffer();
  for( Int i = 0; i < vps.getNumHrdParameters(); i++ )
  {
    vps.setHrdOpSetIdx( 0, i );
    vps.setCprmsPresentFlag( false, i );
  }
}

Void TAppEncTop::xSetLayerIds( TComVPS& vps )
{
  vps.setSplittingFlag     ( m_splittingFlag );

  Bool nuhLayerIdPresentFlag = false;
  

  vps.setVpsMaxLayerId( xGetMax( m_layerIdInNuh ) );

  for (Int i = 0; i < m_numberOfLayers; i++)
  {
    nuhLayerIdPresentFlag = nuhLayerIdPresentFlag || ( m_layerIdInNuh[i] != i );
  }

  vps.setVpsNuhLayerIdPresentFlag( nuhLayerIdPresentFlag );

  for (Int layer = 0; layer < m_numberOfLayers; layer++ )
  {
    vps.setLayerIdInNuh( layer, nuhLayerIdPresentFlag ? m_layerIdInNuh[ layer ] : layer );
    vps.setLayerIdInVps( vps.getLayerIdInNuh( layer ), layer );
  }
}

Int TAppEncTop::xGetMax( std::vector<Int>& vec )
{
  Int maxVec = 0;
  for ( Int i = 0; i < vec.size(); i++)
  {
    maxVec = max( vec[i], maxVec );
  }
  return maxVec;
}

Void TAppEncTop::xSetProfileTierLevel( TComVPS& vps )
{

  xDeriveProfAndConstrFlags( vps );
  xCheckProfiles           ( vps );
  xPrintProfiles           (  );
  
  // SET PTL
  assert( m_profiles.size() == m_level.size() && m_profiles.size() == m_levelTier.size() );
  vps.setVpsNumProfileTierLevelMinus1( (Int) m_profiles.size() - 1 );
  for ( Int ptlIdx = 0; ptlIdx <= vps.getVpsNumProfileTierLevelMinus1(); ptlIdx++ )
  {
    if ( ptlIdx > 1 )
    {
      Bool vpsProfilePresentFlag =
           ( m_profiles                [ptlIdx ] != m_profiles                [ptlIdx - 1] )
        || ( m_progressiveSourceFlags  [ptlIdx ] != m_progressiveSourceFlags  [ptlIdx - 1] )
        || ( m_interlacedSourceFlags   [ptlIdx ] != m_interlacedSourceFlags   [ptlIdx - 1] )
        || ( m_nonPackedConstraintFlags[ptlIdx ] != m_nonPackedConstraintFlags[ptlIdx - 1] )
        || ( m_frameOnlyConstraintFlags[ptlIdx ] != m_frameOnlyConstraintFlags[ptlIdx - 1] )
        || ( m_inblFlag                [ptlIdx ] != m_inblFlag                [ptlIdx - 1] );

      
      if ( m_profiles[ ptlIdx ] >= 4 && m_profiles[ ptlIdx ] <= 7 )
      {

        vpsProfilePresentFlag = vpsProfilePresentFlag
          || ( m_profiles                [ptlIdx ] != m_profiles                [ptlIdx - 1] )
          || ( m_progressiveSourceFlags  [ptlIdx ] != m_progressiveSourceFlags  [ptlIdx - 1] )
          || ( m_interlacedSourceFlags   [ptlIdx ] != m_interlacedSourceFlags   [ptlIdx - 1] )
          || ( m_nonPackedConstraintFlags[ptlIdx ] != m_nonPackedConstraintFlags[ptlIdx - 1] )
          || ( m_frameOnlyConstraintFlags[ptlIdx ] != m_frameOnlyConstraintFlags[ptlIdx - 1] )
          || ( m_inblFlag                [ptlIdx ] != m_inblFlag                [ptlIdx - 1] );

      }

      
      vps.setVpsProfilePresentFlag( ptlIdx, vpsProfilePresentFlag );
    }

    xSetProfileTierLevel( vps, ptlIdx, -1, m_profiles[ptlIdx], m_level[ptlIdx],
      m_levelTier[ ptlIdx ], m_progressiveSourceFlags[ptlIdx], m_interlacedSourceFlags[ptlIdx],
      m_nonPackedConstraintFlags[ptlIdx], m_frameOnlyConstraintFlags[ptlIdx],  m_inblFlag[ptlIdx] );
  }
}

Void TAppEncTop::xSetProfileTierLevel(TComVPS& vps, Int ptlIdx, Int subLayer, Profile::Name profile, Level::Name level, Level::Tier tier, Bool progressiveSourceFlag, Bool interlacedSourceFlag, Bool nonPackedConstraintFlag, Bool frameOnlyConstraintFlag, Bool inbldFlag)
{
  
  TComPTL* ptlStruct = vps.getPTL( ptlIdx );
  assert( ptlStruct != NULL );

  ProfileTierLevel* ptl;
  if ( subLayer == -1 )
  {
    ptl = ptlStruct->getGeneralPTL();
  }
  else
  {
    ptl = ptlStruct->getSubLayerPTL(  subLayer );
  }

  assert( ptl != NULL );

  ptl->setProfileIdc              ( m_profiles [ ptlIdx ] );
  ptl->setTierFlag                ( m_levelTier[ ptlIdx ] );
  ptl->setLevelIdc                ( m_level    [ ptlIdx ] );
  ptl->setProfileCompatibilityFlag( m_profiles [ ptlIdx ], true );
  ptl->setInbldFlag               ( m_inblFlag [ ptlIdx ] );

  Int        bitDepth = m_bitDepthConstraints[ptlIdx];
  ChromaFormat chroma = m_chromaFormatConstraints[ptlIdx];

  ptl->setMax12bitConstraintFlag      (  bitDepth <= 12  );
  ptl->setMax10bitConstraintFlag      (  bitDepth <= 10 );
  ptl->setMax8bitConstraintFlag       (  bitDepth <= 8 );
  ptl->setMax422chromaConstraintFlag  ( chroma == CHROMA_400 || chroma == CHROMA_420 || chroma == CHROMA_422    );
  ptl->setMax420chromaConstraintFlag  ( chroma == CHROMA_400 || chroma == CHROMA_420                            );                         ;
  ptl->setMaxMonochromeConstraintFlag ( chroma == CHROMA_400                       );
  ptl->setIntraConstraintFlag         ( m_intraConstraintFlags[ ptlIdx ]           );
  ptl->setOnePictureOnlyConstraintFlag( m_onePictureOnlyConstraintFlags[ ptlIdx ]  );
  ptl->setLowerBitRateConstraintFlag  ( m_lowerBitRateConstraintFlags[ ptlIdx ]    );
}

Void TAppEncTop::xSetRepFormat( TComVPS& vps )
{
  vps.setVpsNumRepFormatsMinus1 ( m_numRepFormats - 1 );


  std::vector<TComRepFormat> repFormat;
  repFormat.resize( vps.getVpsNumRepFormatsMinus1() + 1 );
  for ( Int j = 0; j <= vps.getVpsNumRepFormatsMinus1(); j++ )
  {
    repFormat[j].setBitDepthVpsChromaMinus8   ( m_internalBitDepths[j][CHANNEL_TYPE_LUMA  ] - 8 );
    repFormat[j].setBitDepthVpsLumaMinus8     ( m_internalBitDepths[j][CHANNEL_TYPE_CHROMA] - 8 );
    repFormat[j].setChromaFormatVpsIdc        ( m_chromaFormatIDCs[j] );
    repFormat[j].setPicHeightVpsInLumaSamples ( m_iSourceHeights[j] );
    repFormat[j].setPicWidthVpsInLumaSamples  ( m_iSourceWidths [j] );
    repFormat[j].setChromaAndBitDepthVpsPresentFlag( true );
    // ToDo not supported yet.
    //repFormat->setSeparateColourPlaneVpsFlag( );

    repFormat[j].setConformanceWindowVpsFlag( true );
    repFormat[j].setConfWinVpsLeftOffset    ( m_confWinLefts  [j] / TComSPS::getWinUnitX( repFormat[j].getChromaFormatVpsIdc() ) );
    repFormat[j].setConfWinVpsRightOffset   ( m_confWinRights [j] / TComSPS::getWinUnitX( repFormat[j].getChromaFormatVpsIdc() ) );
    repFormat[j].setConfWinVpsTopOffset     ( m_confWinTops   [j] / TComSPS::getWinUnitY( repFormat[j].getChromaFormatVpsIdc() ) );
    repFormat[j].setConfWinVpsBottomOffset  ( m_confWinBottoms[j] / TComSPS::getWinUnitY( repFormat[j].getChromaFormatVpsIdc() ) );
  }

  vps.setRepFormat( repFormat );


  if ( vps.getVpsNumRepFormatsMinus1() > 0 )
  {
    Bool repFormatIdxPresentFlag = false;
    for( Int i = vps.getVpsBaseLayerInternalFlag() ? 1 : 0; i <= vps.getMaxLayersMinus1(); i++ )
    {
      repFormatIdxPresentFlag = repFormatIdxPresentFlag ||  ( m_layerIdxInVpsToRepFormatIdx[i]  != vps.inferVpsRepFormatIdx( i ) );
    }
    vps.setRepFormatIdxPresentFlag( repFormatIdxPresentFlag );
  }

  for( Int i =  0; i <=  vps.getMaxLayersMinus1(); i++ )
  {
    // When base_layer_internal_flag is equal to 1, the first repFormatIdx cannot be signaled but is inferred.
    if( !vps.getRepFormatIdxPresentFlag() || ( vps.getVpsBaseLayerInternalFlag() && i == 0 )   )
    {
      vps.setVpsRepFormatIdx( i, vps.inferVpsRepFormatIdx( i ) );
      AOF( vps.getVpsRepFormatIdx( i ) == m_layerIdxInVpsToRepFormatIdx[i] );
    }
    else
    {
      vps.setVpsRepFormatIdx( i, m_layerIdxInVpsToRepFormatIdx[i] );
    }
  }


  xConfirmRepFormat( vps );

}

Void TAppEncTop::xSetDpbSize                ( TComVPS& vps )
{
  // These settings need to be verified

  TComDpbSize dpbSize;
  dpbSize.init( vps.getNumOutputLayerSets(), vps.getVpsMaxLayerId() + 1, vps.getMaxSubLayersMinus1() + 1 ) ;
  

  for( Int i = 0; i < vps.getNumOutputLayerSets(); i++ )
  {
    Int currLsIdx = vps.olsIdxToLsIdx( i );
    Bool subLayerFlagInfoPresentFlag = false;

    for( Int j = 0; j  <=  vps.getMaxSubLayersInLayerSetMinus1( currLsIdx ); j++ )
    {
      Bool subLayerDpbInfoPresentFlag = false;
      for( Int k = 0; k < vps.getNumLayersInIdList( currLsIdx ); k++ )
      {
        Int layerIdInVps = vps.getLayerIdInVps( vps.getLayerSetLayerIdList( currLsIdx, k ) );
        if ( vps.getNecessaryLayerFlag( i,k ) && ( vps.getVpsBaseLayerInternalFlag() || vps.getLayerSetLayerIdList( currLsIdx, k ) != 0 ) )
        {
          dpbSize.setMaxVpsDecPicBufferingMinus1( i, k, j, m_maxDecPicBufferingMvc[ layerIdInVps ][ j ] - 1 );
          if ( j > 0 )
          {
            subLayerDpbInfoPresentFlag = subLayerDpbInfoPresentFlag || ( dpbSize.getMaxVpsDecPicBufferingMinus1( i, k, j ) != dpbSize.getMaxVpsDecPicBufferingMinus1( i, k, j - 1 ) );
          }
        }
        else
        {
          if (vps.getNecessaryLayerFlag(i,k) && j == 0 && k == 0 )
          {
            dpbSize.setMaxVpsDecPicBufferingMinus1(i, k ,j, 0 );
          }
        }
      }

      Int maxNumReorderPics = MIN_INT;
      for ( Int idx = 0; idx < vps.getNumLayersInIdList( currLsIdx ); idx++ )
      {
        if (vps.getNecessaryLayerFlag(i, idx ))
        {
          Int layerIdInVps = vps.getLayerIdInVps( vps.getLayerSetLayerIdList(currLsIdx, idx) );
          maxNumReorderPics = std::max( maxNumReorderPics, m_numReorderPicsMvc[ layerIdInVps ][ j ] );
        }
      }
      assert( maxNumReorderPics != MIN_INT );

      dpbSize.setMaxVpsNumReorderPics( i, j, maxNumReorderPics );
      if ( j > 0 )
      {
        subLayerDpbInfoPresentFlag = subLayerDpbInfoPresentFlag || ( dpbSize.getMaxVpsNumReorderPics( i, j ) != dpbSize.getMaxVpsNumReorderPics( i, j - 1 ) );
      }

      // To Be Done !
      // dpbSize.setMaxVpsLatencyIncreasePlus1( i, j, xx );
      if ( j > 0 )
      {
        subLayerDpbInfoPresentFlag = subLayerDpbInfoPresentFlag || ( dpbSize.getMaxVpsLatencyIncreasePlus1( i, j ) != dpbSize.getMaxVpsLatencyIncreasePlus1( i, j - 1  ) );
      }

      if( j > 0 )
      {
        dpbSize.setSubLayerDpbInfoPresentFlag( i, j, subLayerDpbInfoPresentFlag );
        subLayerFlagInfoPresentFlag = subLayerFlagInfoPresentFlag || subLayerDpbInfoPresentFlag;
      }
    }
    dpbSize.setSubLayerFlagInfoPresentFlag( i, subLayerFlagInfoPresentFlag );
  }
  vps.setDpbSize( dpbSize );
}

Void TAppEncTop::xSetLayerSets( TComVPS& vps )
{
  // Layer sets
  vps.setVpsNumLayerSetsMinus1   ( m_vpsNumLayerSets - 1 );
    
  for (Int lsIdx = 0; lsIdx < m_vpsNumLayerSets; lsIdx++ )
  {
    for( Int layerId = 0; layerId < MAX_NUM_LAYER_IDS; layerId++ )
    {
      vps.setLayerIdIncludedFlag( false, lsIdx, layerId );
    }
    for ( Int i = 0; i < m_layerIdxInVpsInSets[lsIdx].size(); i++)
    {
      vps.setLayerIdIncludedFlag( true, lsIdx, vps.getLayerIdInNuh( m_layerIdxInVpsInSets[lsIdx][i] ) );
    }
  }
  vps.deriveLayerSetLayerIdList();

  Int numAddOuputLayerSets = (Int) m_outputLayerSetIdx.size();
  // Additional output layer sets + profileLevelTierIdx
  vps.setDefaultOutputLayerIdc      ( m_defaultOutputLayerIdc );
  if( vps.getNumIndependentLayers() == 0 && m_numAddLayerSets > 0  )
  {
    fprintf( stderr, "\nWarning: Ignoring additional layer sets since NumIndependentLayers is equal to 0.\n");
  }
  else
  {
    vps.setNumAddLayerSets( m_numAddLayerSets );
    if ( m_highestLayerIdxPlus1.size() < vps.getNumAddLayerSets() )
    {
      fprintf(stderr, "\nError: Number of highestLayerIdxPlus1 parameters must be greater than or equal to NumAddLayerSets\n");
      exit(EXIT_FAILURE);
    }

    for (Int i = 0; i < vps.getNumAddLayerSets(); i++)
    {
      if ( m_highestLayerIdxPlus1[ i ].size() < vps.getNumIndependentLayers() )
      {
        fprintf(stderr, "Error: Number of elements in highestLayerIdxPlus1[ %d ] parameters must be greater than or equal to NumIndependentLayers(= %d)\n", i, vps.getNumIndependentLayers());
        exit(EXIT_FAILURE);
      }

      for (Int j = 1; j < vps.getNumIndependentLayers(); j++)
      {
        if ( m_highestLayerIdxPlus1[ i ][ j ]  < 0 || m_highestLayerIdxPlus1[ i ][ j ] > vps.getNumLayersInTreePartition( j ) )
        {
          fprintf(stderr, "Error: highestLayerIdxPlus1[ %d ][ %d ] shall be in the range of 0 to NumLayersInTreePartition[ %d ] (= %d ), inclusive. \n", i, j, j, vps.getNumLayersInTreePartition( j ) );
          exit(EXIT_FAILURE);
        }
        vps.setHighestLayerIdxPlus1( i, j, m_highestLayerIdxPlus1[ i ][ j ] );
      }
      vps.deriveAddLayerSetLayerIdList( i );
    }
  }
  vps.setNumAddOlss                 ( numAddOuputLayerSets          );
  vps.initTargetLayerIdLists();

  for (Int olsIdx = 0; olsIdx < vps.getNumLayerSets() + numAddOuputLayerSets; olsIdx++)
  {
    Int addOutLsIdx = olsIdx - vps.getNumLayerSets();
    vps.setLayerSetIdxForOlsMinus1( olsIdx, ( ( addOutLsIdx < 0 ) ?  olsIdx  : m_outputLayerSetIdx[ addOutLsIdx ] ) - 1 );

    Int lsIdx = vps.olsIdxToLsIdx( olsIdx );
    if (vps.getDefaultOutputLayerIdc() == 2 || addOutLsIdx >= 0 )
    {
      for ( Int i = 0; i < vps.getNumLayersInIdList( lsIdx ); i++)
      {
        vps.setOutputLayerFlag( olsIdx, i, ( olsIdx == 0 && i == 0 ) ? vps.inferOutputLayerFlag(olsIdx, i ) : false ); // This is a software only fix for a bug in the spec. In spec outputLayerFlag neither present nor inferred for this case !
      }

      std::vector<Int>& outLayerIdList = ( addOutLsIdx >= 0 ) ? m_layerIdsInAddOutputLayerSet[addOutLsIdx] : m_layerIdsInDefOutputLayerSet[olsIdx];

      Bool outputLayerInLayerSetFlag = false;
      for (Int j = 0; j < outLayerIdList.size(); j++)
      {
        for ( Int i = 0; i < vps.getNumLayersInIdList( lsIdx ); i++)
        {
          if ( vps.getLayerSetLayerIdList( lsIdx, i ) == outLayerIdList[ j ] )
          {
            vps.setOutputLayerFlag( olsIdx, i, true );
            outputLayerInLayerSetFlag = true;
            break;
          }
        }
        if ( !outputLayerInLayerSetFlag )
        {
          fprintf(stderr, "Error: Output layer %d in output layer set %d not in corresponding layer set %d \n", outLayerIdList[ j ], olsIdx , lsIdx );
          exit(EXIT_FAILURE);
        }
      }
    }
    else
    {
      for ( Int i = 0; i < vps.getNumLayersInIdList( lsIdx ); i++)
      {
        vps.setOutputLayerFlag( olsIdx, i, vps.inferOutputLayerFlag( olsIdx, i ) );
      }
    }

    vps.deriveNecessaryLayerFlags( olsIdx );
    vps.deriveTargetLayerIdList(  olsIdx );

    // SET profile_tier_level_index.
    if ( olsIdx == 0 )
    {
      vps.setProfileTierLevelIdx( 0, 0 , vps.getMaxLayersMinus1() > 0 ? 1 : 0 );
    }
    else
    {
      if( (Int) m_profileTierLevelIdx[ olsIdx ].size() < vps.getNumLayersInIdList( lsIdx ) )
      {
        fprintf( stderr, "Warning: Not enough profileTierLevelIdx values given for the %d-th OLS. Inferring default values.\n", olsIdx );
      }
      for (Int j = 0; j < vps.getNumLayersInIdList( lsIdx ); j++)
      {
        if( j < (Int) m_profileTierLevelIdx[ olsIdx ].size() )
        {
          vps.setProfileTierLevelIdx(olsIdx, j, m_profileTierLevelIdx[olsIdx][j] );
          if( !vps.getNecessaryLayerFlag(olsIdx,j) && m_profileTierLevelIdx[ olsIdx ][ j ] != -1 )
          {
            fprintf( stderr, "Warning: The %d-th layer in the %d-th OLS is not necessary such that profileTierLevelIdx[%d][%d] will be ignored. Set value to -1 to suppress warning.\n", j,olsIdx,olsIdx,j );
          }
        }
        else if ( vps.getNecessaryLayerFlag(olsIdx,j) )
        {
          // setting default values
          if ( j == 0 || vps.getVpsNumProfileTierLevelMinus1() < 1 )
          {
            // set base layer as default
            vps.setProfileTierLevelIdx(olsIdx, j, 1 );
          }
          else
          {
            // set VpsProfileTierLevel[2] as default
            vps.setProfileTierLevelIdx(olsIdx, j, 2 );
          }
        }
      }
    }
   
    if ( vps.getNumOutputLayersInOutputLayerSet( olsIdx ) == 1 &&
        vps.getNumDirectRefLayers( vps.getOlsHighestOutputLayerId( olsIdx ) ) )
    {
      vps.setAltOutputLayerFlag( olsIdx , m_altOutputLayerFlag[ olsIdx ]);
    }
    else
    {
      vps.setAltOutputLayerFlag( olsIdx , false );
      if ( m_altOutputLayerFlag[ olsIdx ] )
      {
        printf( "\nWarning: Ignoring AltOutputLayerFlag for output layer set %d, since more than one output layer or no dependent layers.\n", olsIdx );
      }
    }
  }
}

Void TAppEncTop::xSetVPSVUI( TComVPS& vps )
{
  vps.setVpsVuiPresentFlag( m_vpsVuiPresentFlag );

  TComVPSVUI vpsVui;
  vpsVui.init(vps.getNumAddLayerSets(),vps.getMaxSubLayersMinus1() + 1, vps.getMaxLayersMinus1() + 1 );

  if ( m_vpsVuiPresentFlag )
  {
    // All this stuff could actually be derived by the encoder,
    // however preliminary setting it from input parameters

    vpsVui.setCrossLayerPicTypeAlignedFlag( m_crossLayerPicTypeAlignedFlag );
    vpsVui.setCrossLayerIrapAlignedFlag   ( m_crossLayerIrapAlignedFlag    );
    vpsVui.setAllLayersIdrAlignedFlag     ( m_allLayersIdrAlignedFlag      );
    vpsVui.setBitRatePresentVpsFlag( m_bitRatePresentVpsFlag );
    vpsVui.setPicRatePresentVpsFlag( m_picRatePresentVpsFlag );

    if( vpsVui.getBitRatePresentVpsFlag( )  ||  vpsVui.getPicRatePresentVpsFlag( ) )
    {
      for( Int i = 0; i  <  vps.getNumLayerSets(); i++ )
      {
        for( Int j = 0; j  <=  vps.getMaxTLayers(); j++ )
        {
          if( vpsVui.getBitRatePresentVpsFlag( ) && m_bitRatePresentFlag[i].size() > j )
          {
            vpsVui.setBitRatePresentFlag( i, j, m_bitRatePresentFlag[i][j] );
          }
          if( vpsVui.getPicRatePresentVpsFlag( ) && m_picRatePresentFlag[i].size() > j   )
          {
            vpsVui.setPicRatePresentFlag( i, j, m_picRatePresentFlag[i][j] );
          }
          if( vpsVui.getBitRatePresentFlag( i, j )  && m_avgBitRate[i].size() > j )
          {
            vpsVui.setAvgBitRate( i, j, m_avgBitRate[i][j] );
          }
          if( vpsVui.getBitRatePresentFlag( i, j )  && m_maxBitRate[i].size() > j )
          {
            vpsVui.setMaxBitRate( i, j, m_maxBitRate[i][j] );
          }
          if( vpsVui.getPicRatePresentFlag( i, j ) && m_constantPicRateIdc[i].size() > j )
          {
            vpsVui.setConstantPicRateIdc( i, j, m_constantPicRateIdc[i][j] );
          }
          if( vpsVui.getPicRatePresentFlag( i, j ) && m_avgPicRate[i].size() > j )
          {
            vpsVui.setAvgPicRate( i, j, m_avgPicRate[i][j] );
          }
        }
      }
    }

    vpsVui.setTilesNotInUseFlag( m_tilesNotInUseFlag );

    if( !vpsVui.getTilesNotInUseFlag() )
    {
      for( Int i = 0; i  <=  vps.getMaxLayersMinus1(); i++ )
      {
        vpsVui.setTilesInUseFlag( i, m_tilesInUseFlag[ i ] );
        if( vpsVui.getTilesInUseFlag( i ) )
        {
          vpsVui.setLoopFilterNotAcrossTilesFlag( i, m_loopFilterNotAcrossTilesFlag[ i ] );
        }
      }

      for( Int i = 1; i  <=  vps.getMaxLayersMinus1(); i++ )
      {
        for( Int j = 0; j < vps.getNumDirectRefLayers( vps.getLayerIdInNuh( i ) ) ; j++ )
        {
          Int layerIdx = vps.getLayerIdInVps( vps.getIdDirectRefLayer(vps.getLayerIdInNuh( i ) , j  ));
          if( vpsVui.getTilesInUseFlag( i )  &&  vpsVui.getTilesInUseFlag( layerIdx ) )
          {
            vpsVui.setTileBoundariesAlignedFlag( i, j, m_tileBoundariesAlignedFlag[i][j] );
          }
        }
      }
    }

    vpsVui.setWppNotInUseFlag( m_wppNotInUseFlag );

    if( !vpsVui.getWppNotInUseFlag( ) )
    {
      for( Int i = 1; i  <=  vps.getMaxLayersMinus1(); i++ )
      {
        vpsVui.setWppInUseFlag( i, m_wppInUseFlag[ i ]);
      }
    }

  vpsVui.setSingleLayerForNonIrapFlag( m_singleLayerForNonIrapFlag );
  vpsVui.setHigherLayerIrapSkipFlag( m_higherLayerIrapSkipFlag );

    vpsVui.setIlpRestrictedRefLayersFlag( m_ilpRestrictedRefLayersFlag );

    if( vpsVui.getIlpRestrictedRefLayersFlag( ) )
    {
      for( Int i = 1; i  <=  vps.getMaxLayersMinus1(); i++ )
      {
        for( Int j = 0; j < vps.getNumDirectRefLayers( vps.getLayerIdInNuh( i ) ); j++ )
        {
          if ( m_minSpatialSegmentOffsetPlus1[i].size() > j )
          {
            vpsVui.setMinSpatialSegmentOffsetPlus1( i, j, m_minSpatialSegmentOffsetPlus1[i][j] );
          }
          if( vpsVui.getMinSpatialSegmentOffsetPlus1( i, j ) > 0 )
          {
            if ( m_ctuBasedOffsetEnabledFlag[i].size() > j )
            {
              vpsVui.setCtuBasedOffsetEnabledFlag( i, j, m_ctuBasedOffsetEnabledFlag[i][j] );
            }
            if( vpsVui.getCtuBasedOffsetEnabledFlag( i, j ) )
            {
              if ( m_minHorizontalCtuOffsetPlus1[i].size() > j )
              {
                vpsVui.setMinHorizontalCtuOffsetPlus1( i, j, m_minHorizontalCtuOffsetPlus1[i][j] );
              }
            }
          }
        }
      }
    }
    vpsVui.setVideoSignalInfoIdxPresentFlag( true );
    vpsVui.setVpsNumVideoSignalInfoMinus1  ( 0    );

    std::vector<TComVideoSignalInfo> videoSignalInfos;
    videoSignalInfos.resize( vpsVui.getVpsNumVideoSignalInfoMinus1() + 1 );

    videoSignalInfos[0].setColourPrimariesVps        ( m_colourPrimaries );
    videoSignalInfos[0].setMatrixCoeffsVps           ( m_matrixCoefficients );
    videoSignalInfos[0].setTransferCharacteristicsVps( m_transferCharacteristics );
    videoSignalInfos[0].setVideoVpsFormat            ( m_videoFormat );
    videoSignalInfos[0].setVideoFullRangeVpsFlag     ( m_videoFullRangeFlag );

    vpsVui.setVideoSignalInfo( videoSignalInfos );

    for (Int i = 0; i < m_numberOfLayers; i++)
    {
      vpsVui.setVpsVideoSignalInfoIdx( i, 0 );
    }
    vpsVui.setVpsVuiBspHrdPresentFlag( false ); // TBD
  }
  else
  {
    //Default inference when not present.
    vpsVui.setCrossLayerIrapAlignedFlag   ( false   );
  }
  vps.setVPSVUI( vpsVui );
}

Bool TAppEncTop::xLayerIdInTargetEncLayerIdList(Int nuhLayerId)
{
  return  ( std::find(m_targetEncLayerIdList.begin(), m_targetEncLayerIdList.end(), nuhLayerId) != m_targetEncLayerIdList.end()) ;
}


#endif

//! \}
