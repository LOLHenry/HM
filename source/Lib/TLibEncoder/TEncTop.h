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

/** \file     TEncTop.h
    \brief    encoder class (header)
*/

#ifndef __TENCTOP__
#define __TENCTOP__

// Include files
#include "TLibCommon/TComList.h"
#include "TLibCommon/TComPrediction.h"
#include "TLibCommon/TComTrQuant.h"
#include "TLibCommon/TComLoopFilter.h"
#include "TLibCommon/AccessUnit.h"

#include "Utilities/TVideoIOYuv.h"

#include "TEncCfg.h"
#include "TEncGOP.h"
#include "TEncSlice.h"
#include "TEncEntropy.h"
#include "TEncCavlc.h"
#include "TEncSbac.h"
#include "TEncSearch.h"
#include "TEncSampleAdaptiveOffset.h"
#include "TEncPreanalyzer.h"
#include "TEncRateCtrl.h"
//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

#if KWU_RC_MADPRED_E0227
class TAppEncTop;
#endif
/// encoder class
class TEncTop : public TEncCfg
{
private:
  // picture
  Int                     m_iPOCLast;                     ///< time index (POC)
  Int                     m_iNumPicRcvd;                  ///< number of received pictures
  UInt                    m_uiNumAllPicCoded;             ///< number of coded pictures
#if !NH_MV
  TComList<TComPic*>      m_cListPic;                     ///< dynamic list of pictures
#endif

#if NH_MV
  TComPicLists*           m_ivPicLists;                   ///< access to picture lists of other layers
#endif
  // encoder search
  TEncSearch              m_cSearch;                      ///< encoder search class
  //TEncEntropy*            m_pcEntropyCoder;                     ///< entropy encoder
  TEncCavlc*              m_pcCavlcCoder;                       ///< CAVLC encoder
  // coding tool
  TComTrQuant             m_cTrQuant;                     ///< transform & quantization class
  TComLoopFilter          m_cLoopFilter;                  ///< deblocking filter class
  TEncSampleAdaptiveOffset m_cEncSAO;                     ///< sample adaptive offset class
  TEncEntropy             m_cEntropyCoder;                ///< entropy encoder
  TEncCavlc               m_cCavlcCoder;                  ///< CAVLC encoder
  TEncSbac                m_cSbacCoder;                   ///< SBAC encoder
  TEncBinCABAC            m_cBinCoderCABAC;               ///< bin coder CABAC

  // processing unit
  TEncGOP                 m_cGOPEncoder;                  ///< GOP encoder
  TEncSlice               m_cSliceEncoder;                ///< slice encoder
  TEncCu                  m_cCuEncoder;                   ///< CU encoder
  // SPS
#if NH_MV
  ParameterSetMap<TComSPS>& m_spsMap;                      ///< SPS. This is the base value. This is copied to TComPicSym
  ParameterSetMap<TComPPS>& m_ppsMap;                      ///< PPS. This is the base value. This is copied to TComPicSym
#else
  ParameterSetMap<TComSPS> m_spsMap;                      ///< SPS. This is the base value. This is copied to TComPicSym
  ParameterSetMap<TComPPS> m_ppsMap;                      ///< PPS. This is the base value. This is copied to TComPicSym
#endif
  // RD cost computation
  TComRdCost              m_cRdCost;                      ///< RD cost computation class
  TEncSbac***             m_pppcRDSbacCoder;              ///< temporal storage for RD computation
  TEncSbac                m_cRDGoOnSbacCoder;             ///< going on SBAC model for RD stage
#if FAST_BIT_EST
  TEncBinCABACCounter***  m_pppcBinCoderCABAC;            ///< temporal CABAC state storage for RD computation
  TEncBinCABACCounter     m_cRDGoOnBinCoderCABAC;         ///< going on bin coder CABAC for RD stage
#else
  TEncBinCABAC***         m_pppcBinCoderCABAC;            ///< temporal CABAC state storage for RD computation
  TEncBinCABAC            m_cRDGoOnBinCoderCABAC;         ///< going on bin coder CABAC for RD stage
#endif

  // quality control
  TEncPreanalyzer         m_cPreanalyzer;                 ///< image characteristics analyzer for TM5-step3-like adaptive QP

  TEncRateCtrl            m_cRateCtrl;                    ///< Rate control class

#if KWU_RC_MADPRED_E0227
  TAppEncTop*             m_pcTAppEncTop;
  TAppComCamPara*         m_cCamParam;
#endif
#if NH_MV
  TEncAnalyze             m_cAnalyzeAll;
  TEncAnalyze             m_cAnalyzeI;
  TEncAnalyze             m_cAnalyzeP;
  TEncAnalyze             m_cAnalyzeB;
  TEncAnalyze             m_cAnalyzeAll_in;
#endif

protected:
  Void  xGetNewPicBuffer  ( TComPic*& rpcPic, Int ppsId ); ///< get picture buffer which will be processed. If ppsId<0, then the ppsMap will be queried for the first match.
  Void  xInitVPS          (TComVPS &vps, const TComSPS &sps); ///< initialize VPS from encoder options
  Void  xInitSPS          (TComSPS &sps);                 ///< initialize SPS from encoder options
  Void  xInitPPS          (TComPPS &pps, const TComSPS &sps); ///< initialize PPS from encoder options
  Void  xInitScalingLists (TComSPS &sps, TComPPS &pps);   ///< initialize scaling lists
  Void  xInitHrdParameters(TComSPS &sps);                 ///< initialize HRD parameters

#if NH_MV
  Int   xGetPsIdAndPsLayerId      ( );

  Void  xInitPPSforTiles  (TComPPS &pps) const ;
#else
  Void  xInitPPSforTiles  (TComPPS &pps);
#endif
  Void  xInitRPS          (TComSPS &sps, Bool isFieldCoding);           ///< initialize PPS from encoder options

public:
#if NH_MV
  TEncTop( ParameterSetMap<TComSPS>& spsMap, ParameterSetMap<TComPPS>& ppsMap );
#else
  TEncTop();
#endif
  virtual ~TEncTop();

  Void      create          ();
  Void      destroy         ();
#if KWU_RC_MADPRED_E0227
  Void      init            ( TAppEncTop* pcTAppEncTop, Bool isFieldCoding );
#else
  Void      init            (Bool isFieldCoding);
#endif
#if NH_MV
  TComPicLists* getIvPicLists() { return m_ivPicLists; }
#endif
  Void      deletePicBuffer ();
#if NH_MV
  Void      initNewPic(TComPicYuv* pcPicYuvOrg);
#endif
  // -------------------------------------------------------------------------------------------------------------------
  // member access functions
  // -------------------------------------------------------------------------------------------------------------------

#if NH_MV
  TComList<TComPic*>*     getListPic            () { return  m_ivPicLists->getSubDpb( getLayerId(), false);             }
#else
  TComList<TComPic*>*     getListPic            () { return  &m_cListPic;             }
#endif
  TEncSearch*             getPredSearch         () { return  &m_cSearch;              }

  TComTrQuant*            getTrQuant            () { return  &m_cTrQuant;             }
  TComLoopFilter*         getLoopFilter         () { return  &m_cLoopFilter;          }
  TEncSampleAdaptiveOffset* getSAO              () { return  &m_cEncSAO;              }
  TEncGOP*                getGOPEncoder         () { return  &m_cGOPEncoder;          }
  TEncSlice*              getSliceEncoder       () { return  &m_cSliceEncoder;        }
  TEncCu*                 getCuEncoder          () { return  &m_cCuEncoder;           }
  TEncEntropy*            getEntropyCoder       () { return  &m_cEntropyCoder;        }
  TEncCavlc*              getCavlcCoder         () { return  &m_cCavlcCoder;          }
  TEncSbac*               getSbacCoder          () { return  &m_cSbacCoder;           }
  TEncBinCABAC*           getBinCABAC           () { return  &m_cBinCoderCABAC;       }

  TComRdCost*             getRdCost             () { return  &m_cRdCost;              }
  TEncSbac***             getRDSbacCoder        () { return  m_pppcRDSbacCoder;       }
  TEncSbac*               getRDGoOnSbacCoder    () { return  &m_cRDGoOnSbacCoder;     }
  TEncRateCtrl*           getRateCtrl           () { return &m_cRateCtrl;             }
#if KWU_RC_MADPRED_E0227
  TAppEncTop*             getEncTop             () { return m_pcTAppEncTop; }
  TAppComCamPara*         getCamParam()                 { return m_cCamParam;}
  Void                    setCamParam(TAppComCamPara * pCamparam)                 { m_cCamParam = pCamparam;}
#endif
  Void selectReferencePictureSet(TComSlice* slice, Int POCCurr, Int GOPid );
  Int getReferencePictureSetIdxForSOP(Int POCCurr, Int GOPid );
#if NH_MV
  TEncAnalyze*            getAnalyzeAll         () { return &m_cAnalyzeAll; }
  TEncAnalyze*            getAnalyzeI           () { return &m_cAnalyzeI;   }
  TEncAnalyze*            getAnalyzeP           () { return &m_cAnalyzeP;   }
  TEncAnalyze*            getAnalyzeB           () { return &m_cAnalyzeB;   }
  Int                     getNumAllPicCoded     () { return m_uiNumAllPicCoded; }
  Int                     getFrameId            (Int iGOPid);
  TComPic*                getPic                ( Int poc );
  Void                    setIvPicLists         ( TComPicLists* picLists) { m_ivPicLists = picLists; }
#endif

#if NH_MV
  Void                   setParamSetChanged(Int iD, Bool isPps );
#else
  Void                   setParamSetChanged(Int spsId, Int ppsId);
#endif
  Bool                   PPSNeedsWriting(Int ppsId);
  Bool                   SPSNeedsWriting(Int spsId);

  // -------------------------------------------------------------------------------------------------------------------
  // encoder function
  // -------------------------------------------------------------------------------------------------------------------

  /// encode several number of pictures until end-of-sequence
#if NH_MV
  Void encode( Bool bEos,
    TComPicYuv* pcPicYuvOrg,
    TComPicYuv* pcPicYuvTrueOrg,
    const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, // used for SNR calculations. Picture in original colour space.
    TComList<TComPicYuv*>& rcListPicYuvRecOut,
    std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded, Int gopId );

  /// encode several number of pictures until end-of-sequence
  Void encode( Bool bEos, TComPicYuv* pcPicYuvOrg,
    TComPicYuv* pcPicYuvTrueOrg,
    const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, // used for SNR calculations. Picture in original colour space.
    TComList<TComPicYuv*>& rcListPicYuvRecOut,
    std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded, Bool isTff, Int gopId);
#else
#if JVET_X0048_X0103_FILM_GRAIN
  Void encode(Bool bEos,
    TComPicYuv* pcPicYuvOrg,
    TComPicYuv* pcPicYuvTrueOrg,
    TComPicYuv* pcfilteredOrgPicForFG,
    const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, // used for SNR calculations. Picture in original colour space.
    TComList<TComPicYuv*>& rcListPicYuvRecOut,
    std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded);
#else
  Void encode( Bool bEos,
               TComPicYuv* pcPicYuvOrg,
               TComPicYuv* pcPicYuvTrueOrg,
               const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, // used for SNR calculations. Picture in original colour space.
               TComList<TComPicYuv*>& rcListPicYuvRecOut,
               std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded );
#endif

  /// encode several number of pictures until end-of-sequence
  Void encode( Bool bEos, TComPicYuv* pcPicYuvOrg,
               TComPicYuv* pcPicYuvTrueOrg,
               const InputColourSpaceConversion ipCSC, const InputColourSpaceConversion snrCSC, // used for SNR calculations. Picture in original colour space.
               TComList<TComPicYuv*>& rcListPicYuvRecOut,
               std::list<AccessUnit>& accessUnitsOut, Int& iNumEncoded, Bool isTff);
#endif

  TEncAnalyze::OutputLogControl getOutputLogControl() const
  {
    TEncAnalyze::OutputLogControl outputLogCtrl;
    outputLogCtrl.printFrameMSE=m_printFrameMSE;
    outputLogCtrl.printMSEBasedSNR=m_printMSEBasedSequencePSNR;
    outputLogCtrl.printMSSSIM=m_printMSSSIM;
    outputLogCtrl.printSequenceMSE=m_printSequenceMSE;
    outputLogCtrl.printXPSNR=m_bXPSNREnableFlag;
    outputLogCtrl.printHexPerPOCPSNRs=m_printHexPsnr;
    return outputLogCtrl;
  }

  Void printSummary(Bool isField)
  {
    m_cGOPEncoder.printOutSummary (m_uiNumAllPicCoded, isField, getOutputLogControl(), m_spsMap.getFirstPS()->getBitDepths());
  }

};

//! \}

#endif // __TENCTOP__

