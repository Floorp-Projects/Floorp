/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: dequant.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include "dequant.h"
#include "decint.h"

int oc_quant_params_unpack(oggpack_buffer *_opb,
 th_quant_info *_qinfo){
  th_quant_base *base_mats;
  long           val;
  int            nbase_mats;
  int            sizes[64];
  int            indices[64];
  int            nbits;
  int            bmi;
  int            ci;
  int            qti;
  int            pli;
  int            qri;
  int            qi;
  int            i;
  theorapackB_read(_opb,3,&val);
  nbits=(int)val;
  for(qi=0;qi<64;qi++){
    theorapackB_read(_opb,nbits,&val);
    _qinfo->loop_filter_limits[qi]=(unsigned char)val;
  }
  theorapackB_read(_opb,4,&val);
  nbits=(int)val+1;
  for(qi=0;qi<64;qi++){
    theorapackB_read(_opb,nbits,&val);
    _qinfo->ac_scale[qi]=(ogg_uint16_t)val;
  }
  theorapackB_read(_opb,4,&val);
  nbits=(int)val+1;
  for(qi=0;qi<64;qi++){
    theorapackB_read(_opb,nbits,&val);
    _qinfo->dc_scale[qi]=(ogg_uint16_t)val;
  }
  theorapackB_read(_opb,9,&val);
  nbase_mats=(int)val+1;
  base_mats=_ogg_malloc(nbase_mats*sizeof(base_mats[0]));
  for(bmi=0;bmi<nbase_mats;bmi++){
    for(ci=0;ci<64;ci++){
      theorapackB_read(_opb,8,&val);
      base_mats[bmi][ci]=(unsigned char)val;
    }
  }
  nbits=oc_ilog(nbase_mats-1);
  for(i=0;i<6;i++){
    th_quant_ranges *qranges;
    th_quant_base   *qrbms;
    int             *qrsizes;
    qti=i/3;
    pli=i%3;
    qranges=_qinfo->qi_ranges[qti]+pli;
    if(i>0){
      theorapackB_read1(_opb,&val);
      if(!val){
        int qtj;
        int plj;
        if(qti>0){
          theorapackB_read1(_opb,&val);
          if(val){
            qtj=qti-1;
            plj=pli;
          }
          else{
            qtj=(i-1)/3;
            plj=(i-1)%3;
          }
        }
        else{
          qtj=(i-1)/3;
          plj=(i-1)%3;
        }
        *qranges=*(_qinfo->qi_ranges[qtj]+plj);
        continue;
      }
    }
    theorapackB_read(_opb,nbits,&val);
    indices[0]=(int)val;
    for(qi=qri=0;qi<63;){
      theorapackB_read(_opb,oc_ilog(62-qi),&val);
      sizes[qri]=(int)val+1;
      qi+=(int)val+1;
      theorapackB_read(_opb,nbits,&val);
      indices[++qri]=(int)val;
    }
    /*Note: The caller is responsible for cleaning up any partially
       constructed qinfo.*/
    if(qi>63){
      _ogg_free(base_mats);
      return TH_EBADHEADER;
    }
    qranges->nranges=qri;
    qranges->sizes=qrsizes=(int *)_ogg_malloc(qri*sizeof(qrsizes[0]));
    memcpy(qrsizes,sizes,qri*sizeof(qrsizes[0]));
    qrbms=(th_quant_base *)_ogg_malloc((qri+1)*sizeof(qrbms[0]));
    qranges->base_matrices=(const th_quant_base *)qrbms;
    do{
      bmi=indices[qri];
      /*Note: The caller is responsible for cleaning up any partially
         constructed qinfo.*/
      if(bmi>=nbase_mats){
        _ogg_free(base_mats);
        return TH_EBADHEADER;
      }
      memcpy(qrbms[qri],base_mats[bmi],sizeof(qrbms[qri]));
    }
    while(qri-->0);
  }
  _ogg_free(base_mats);
  return 0;
}

void oc_quant_params_clear(th_quant_info *_qinfo){
  int i;
  for(i=6;i-->0;){
    int qti;
    int pli;
    qti=i/3;
    pli=i%3;
    /*Clear any duplicate pointer references.*/
    if(i>0){
      int qtj;
      int plj;
      qtj=(i-1)/3;
      plj=(i-1)%3;
      if(_qinfo->qi_ranges[qti][pli].sizes==
       _qinfo->qi_ranges[qtj][plj].sizes){
        _qinfo->qi_ranges[qti][pli].sizes=NULL;
      }
      if(_qinfo->qi_ranges[qti][pli].base_matrices==
       _qinfo->qi_ranges[qtj][plj].base_matrices){
        _qinfo->qi_ranges[qti][pli].base_matrices=NULL;
      }
    }
    if(qti>0){
      if(_qinfo->qi_ranges[1][pli].sizes==
       _qinfo->qi_ranges[0][pli].sizes){
        _qinfo->qi_ranges[1][pli].sizes=NULL;
      }
      if(_qinfo->qi_ranges[1][pli].base_matrices==
       _qinfo->qi_ranges[0][pli].base_matrices){
        _qinfo->qi_ranges[1][pli].base_matrices=NULL;
      }
    }
    /*Now free all the non-duplicate storage.*/
    _ogg_free((void *)_qinfo->qi_ranges[qti][pli].sizes);
    _ogg_free((void *)_qinfo->qi_ranges[qti][pli].base_matrices);
  }
}
