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

/** \file     TAppEncTop.h
    \brief    Encoder application class (header)
*/

#ifndef __TAPPENCTOP__
#define __TAPPENCTOP__

#include <list>
#include <ostream>

#include "TLibEncoder/TEncTop.h"
#include "Utilities/TVideoIOYuv.h"
#include "TLibCommon/AccessUnit.h"
#include "TAppEncCfg.h"

#if KWU_RC_MADPRED_E0227
class TEncTop;
#endif
//! \ingroup TAppEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder application class
class TAppEncTop : public TAppEncCfg
{
private:
  // class interface
#if NH_MV
  std::vector<TEncTop*>      m_acTEncTopList ;              ///< encoder class per layer
  std::vector<TVideoIOYuv*>  m_acTVideoIOYuvInputFileList;  ///< input YUV file
  std::vector<TVideoIOYuv*>  m_acTVideoIOYuvReconFileList;  ///< output reconstruction file
#if SHUTTER_INTERVAL_SEI_PROCESSING
  std::vector<TVideoIOYuv*>  m_cTVideoIOYuvSIIPreFileList;      ///< output pre-filtered file
#endif
  std::vector<TComList<TComPicYuv*>*>  m_cListPicYuvRec;         ///< list of reconstruction YUV files

  std::vector<Int>           m_frameRcvd;                   ///< number of received frames
  TComPicLists               m_ivPicLists;                  ///< picture buffers of encoder instances

  ParameterSetMap<TComSPS>   m_spsMap;
  ParameterSetMap<TComPPS>   m_ppsMap;
  
  IntAry1d                   m_parameterSetId;
  BoolAry1d                  m_sendParameterSets;


#if NH_MV
  TComVPS*                   m_vps;                         ///< vps
#else
  TComVPS                    m_vps;                         ///< vps
#endif
#else
  TEncTop                    m_cTEncTop;                    ///< encoder class
  TVideoIOYuv                m_cTVideoIOYuvInputFile;       ///< input YUV file
  TVideoIOYuv                m_cTVideoIOYuvReconFile;       ///< output reconstruction file
#if SHUTTER_INTERVAL_SEI_PROCESSING
  TVideoIOYuv                m_cTVideoIOYuvSIIPreFile;      ///< output pre-filtered file
#endif

  TComList<TComPicYuv*>      m_cListPicYuvRec;              ///< list of reconstruction YUV files

  Int                        m_iFrameRcvd;                  ///< number of received frames
#endif

  UInt m_essentialBytes;
  UInt m_totalBytes;

protected:
  // initialization
  Void  xCreateLib        ();                               ///< create files & encoder class
  Void  xInitLibCfg       ();                               ///< initialize internal variables
  Void  xInitLib          (Bool isFieldCoding);             ///< initialize encoder class
  Void  xDestroyLib       ();                               ///< destroy encoder class

  /// obtain required buffers
#if NH_MV
  Void  xGetBuffer(TComPicYuv*& rpcPicYuvRec, UInt layer);
#else
  Void xGetBuffer(TComPicYuv*& rpcPicYuvRec);
#endif

  /// delete allocated buffers
  Void  xDeleteBuffer     ();

  // file I/O
#if NH_MV
  Void xWriteOutput(std::ostream& bitstreamFile, Int iNumEncoded, std::list<AccessUnit>& accessUnits, UInt layerId); ///< write bitstream to file
#else
  Void xWriteOutput(std::ostream& bitstreamFile, Int iNumEncoded, const std::list<AccessUnit>& accessUnits); ///< write bitstream to file
#endif
  Void rateStatsAccum(const AccessUnit& au, const std::vector<UInt>& stats);
  Void printRateSummary();
  Void printChromaFormat();

#if NH_MV
  Void xSetTimingInfo             ( TComVPS& vps );
  Void xSetHrdParameters          ( TComVPS& vps );
  Void xSetLayerIds               ( TComVPS& vps );
  Void xSetDimensionIdAndLength   ( TComVPS& vps );
  Void xSetDependencies           ( TComVPS& vps );
  Void xSetLayerSets              ( TComVPS& vps );
  Void xSetProfileTierLevel       ( TComVPS& vps );

  Void xSetProfileTierLevel       ( TComVPS& vps, Int profileTierLevelIdx, Int subLayer,
                                    Profile::Name profile, Level::Name level, Level::Tier tier,
                                    Bool progressiveSourceFlag, Bool interlacedSourceFlag,
                                    Bool nonPackedConstraintFlag, Bool frameOnlyConstraintFlag,
                                    Bool inbldFlag );
  Void xSetRepFormat              ( TComVPS& vps );
  Void xSetDpbSize                ( TComVPS& vps );
  Void xSetVPSVUI                 ( TComVPS& vps );

  Void xDeriveParameterSetIds( TComVPS& vps )
  {

    m_parameterSetId  .clear();
    m_sendParameterSets.clear();

    m_parameterSetId  .resize( m_numberOfLayers, -1 );
    m_sendParameterSets.resize( m_numberOfLayers, false );
    if ( !m_shareParameterSets )
    {
      AOT( m_numberOfLayers > 16 );
      for (Int curVpsLayerId = 0; curVpsLayerId < m_numberOfLayers; curVpsLayerId++ )
      {
        m_parameterSetId   [curVpsLayerId] = curVpsLayerId;
        m_sendParameterSets[curVpsLayerId] = true         ;
      }
    }
    else
    {
      // The spec requires e.g.:
      // - It is a requirement of bitstream conformance that, when present, the value of chroma_format_idc shall be less than or equal to chroma_format_vps_idc
      //   of the vps_rep_format_idx[ j ]-th rep_format( ) syntax structure in the active VPS, where j is equal to LayerIdxInVps[ layerIdCurr ].

      // Consequently, a depth layer with chorma format 4:0:0 cannot refer to a base layer SPS with chroma format 4:2:0

      // Furthermore, it is required that
      // - the SPS RBSP shall have nuh_layer_id equal to 0, nuhLayerId, or IdRefLayer[ nuhLayerId ][ i ] with any value of i in the range of 0 to NumRefLayers[ nuhLayerId ] - 1, inclusive.

      // Because of these requirements and when parameter set sharing is enabled, HTM sends the parameter sets for a current layer in its smallest reference layer having the same representation format.


      Int curPsId = 0;

      // Loop all layers
      for (Int curVpsLayerId = 0; curVpsLayerId < m_numberOfLayers; curVpsLayerId++ )
      {
        // Get smallest reference layer with same rep format idx
        Int curNuhLayerId = vps.getLayerIdInNuh(curVpsLayerId);
        Int smallestRefNuhLIdSameRepFmt   = curNuhLayerId;

        Int curRepFormatIdx = m_layerIdxInVpsToRepFormatIdx[ curVpsLayerId ];

        for (Int j = 0; j < vps.getNumRefLayers( curNuhLayerId ); j++ )
        {
          Int refNuhLayerId = vps.getIdRefLayer( curNuhLayerId, j );
          Int refVpsLayerId = vps.getLayerIdInVps( refNuhLayerId );

          if ( smallestRefNuhLIdSameRepFmt > refNuhLayerId &&  m_layerIdxInVpsToRepFormatIdx[refVpsLayerId] == curRepFormatIdx  )
          {
            smallestRefNuhLIdSameRepFmt = refNuhLayerId;
          }
        }

        Int smallestRefVpsLIdSameRepFmt =   vps.getLayerIdInVps(  smallestRefNuhLIdSameRepFmt );

        if (smallestRefVpsLIdSameRepFmt == curVpsLayerId )
        {
          m_sendParameterSets[ curVpsLayerId ] = true;
          m_parameterSetId   [ curVpsLayerId ] = curPsId;
          curPsId++;
          AOT( curPsId > 15 );
        }
        else
        {
          AOT( m_parameterSetId   [ smallestRefVpsLIdSameRepFmt  ] == -1 );
          m_parameterSetId   [ curVpsLayerId ] = m_parameterSetId   [ smallestRefVpsLIdSameRepFmt  ];
        }

        
      }
    }
  }

  Int  xGetMax( std::vector<Int>& vec);
  Bool xLayerIdInTargetEncLayerIdList( Int nuhLayerId );
#endif

public:
  TAppEncTop();
  virtual ~TAppEncTop();

  Void        encode      ();                               ///< main encoding function
#if NH_MV
  TEncTop*    getTEncTop( UInt layer ) { return  m_acTEncTopList[layer]; }  ///< return pointer to encoder class for specific layer
#else
  TEncTop&    getTEncTop  ()   { return  m_cTEncTop; }      ///< return encoder class pointer reference
#endif

};// END CLASS DEFINITION TAppEncTop

//! \}

#endif // __TAPPENCTOP__

