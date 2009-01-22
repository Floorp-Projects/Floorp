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
    last mod: $Id: fragment.c 15469 2008-10-30 12:49:42Z tterribe $

 ********************************************************************/

#include "../internal.h"

void oc_frag_recon_intra(const oc_theora_state *_state,unsigned char *_dst,
 int _dst_ystride,const ogg_int16_t *_residue){
  _state->opt_vtable.frag_recon_intra(_dst,_dst_ystride,_residue);
}

void oc_frag_recon_intra_c(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue){
  int i;
  for(i=0;i<8;i++){
    int j;
    for(j=0;j<8;j++){
      int res;
      res=*_residue++;
      _dst[j]=OC_CLAMP255(res+128);
    }
    _dst+=_dst_ystride;
  }
}

void oc_frag_recon_inter(const oc_theora_state *_state,unsigned char *_dst,
 int _dst_ystride,const unsigned char *_src,int _src_ystride,
 const ogg_int16_t *_residue){
  _state->opt_vtable.frag_recon_inter(_dst,_dst_ystride,_src,_src_ystride,
   _residue);
}

void oc_frag_recon_inter_c(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,const ogg_int16_t *_residue){
  int i;
  for(i=0;i<8;i++){
    int j;
    for(j=0;j<8;j++){
      int res;
      res=*_residue++;
      _dst[j]=OC_CLAMP255(res+_src[j]);
    }
    _dst+=_dst_ystride;
    _src+=_src_ystride;
  }
}

void oc_frag_recon_inter2(const oc_theora_state *_state,unsigned char *_dst,
 int _dst_ystride,const unsigned char *_src1,int _src1_ystride,
 const unsigned char *_src2,int _src2_ystride,const ogg_int16_t *_residue){
  _state->opt_vtable.frag_recon_inter2(_dst,_dst_ystride,_src1,_src1_ystride,
   _src2,_src2_ystride,_residue);
}

void oc_frag_recon_inter2_c(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  int i;
  for(i=0;i<8;i++){
    int j;
    for(j=0;j<8;j++){
      int res;
      res=*_residue++;
      _dst[j]=OC_CLAMP255(res+((int)_src1[j]+_src2[j]>>1));
    }
    _dst+=_dst_ystride;
    _src1+=_src1_ystride;
    _src2+=_src2_ystride;
  }
}

/*Computes the predicted DC value for the given fragment.
  This requires that the fully decoded DC values be available for the left,
   upper-left, upper, and upper-right fragments (if they exist).
  _frag:      The fragment to predict the DC value for.
  _fplane:    The fragment plane the fragment belongs to.
  _x:         The x-coordinate of the fragment.
  _y:         The y-coordinate of the fragment.
  _pred_last: The last fully-decoded DC value for each predictor frame
               (OC_FRAME_GOLD, OC_FRAME_PREV and OC_FRAME_SELF).
              This should be initialized to 0's for the first fragment in each
               color plane.
  Return: The predicted DC value for this fragment.*/
int oc_frag_pred_dc(const oc_fragment *_frag,
 const oc_fragment_plane *_fplane,int _x,int _y,int _pred_last[3]){
  static const int PRED_SCALE[16][4]={
    /*0*/
    {0,0,0,0},
    /*OC_PL*/
    {1,0,0,0},
    /*OC_PUL*/
    {1,0,0,0},
    /*OC_PL|OC_PUL*/
    {1,0,0,0},
    /*OC_PU*/
    {1,0,0,0},
    /*OC_PL|OC_PU*/
    {1,1,0,0},
    /*OC_PUL|OC_PU*/
    {0,1,0,0},
    /*OC_PL|OC_PUL|PC_PU*/
    {29,-26,29,0},
    /*OC_PUR*/
    {1,0,0,0},
    /*OC_PL|OC_PUR*/
    {75,53,0,0},
    /*OC_PUL|OC_PUR*/
    {1,1,0,0},
    /*OC_PL|OC_PUL|OC_PUR*/
    {75,0,53,0},
    /*OC_PU|OC_PUR*/
    {1,0,0,0},
    /*OC_PL|OC_PU|OC_PUR*/
    {75,0,53,0},
    /*OC_PUL|OC_PU|OC_PUR*/
    {3,10,3,0},
    /*OC_PL|OC_PUL|OC_PU|OC_PUR*/
    {29,-26,29,0}
  };
  static const int PRED_SHIFT[16]={0,0,0,0,0,1,0,5,0,7,1,7,0,7,4,5};
  static const int PRED_RMASK[16]={0,0,0,0,0,1,0,31,0,127,1,127,0,127,15,31};
  static const int BC_MASK[8]={
    /*No boundary condition.*/
    OC_PL|OC_PUL|OC_PU|OC_PUR,
    /*Left column.*/
    OC_PU|OC_PUR,
    /*Top row.*/
    OC_PL,
    /*Top row, left column.*/
    0,
    /*Right column.*/
    OC_PL|OC_PUL|OC_PU,
    /*Right and left column.*/
    OC_PU,
    /*Top row, right column.*/
    OC_PL,
    /*Top row, right and left column.*/
    0
  };
  /*Predictor fragments, left, up-left, up, up-right.*/
  const oc_fragment *predfr[4];
  /*The frame used for prediction for this fragment.*/
  int                pred_frame;
  /*The boundary condition flags.*/
  int                bc;
  /*DC predictor values: left, up-left, up, up-right, missing values
     skipped.*/
  int                p[4];
  /*Predictor count.*/
  int                np;
  /*Which predictor constants to use.*/
  int                pflags;
  /*The predicted DC value.*/
  int                ret;
  int                i;
  pred_frame=OC_FRAME_FOR_MODE[_frag->mbmode];
  bc=(_x==0)+((_y==0)<<1)+((_x+1==_fplane->nhfrags)<<2);
  predfr[0]=_frag-1;
  predfr[1]=_frag-_fplane->nhfrags-1;
  predfr[2]=predfr[1]+1;
  predfr[3]=predfr[2]+1;
  np=0;
  pflags=0;
  for(i=0;i<4;i++){
    int pflag;
    pflag=1<<i;
    if((BC_MASK[bc]&pflag)&&predfr[i]->coded&&
     OC_FRAME_FOR_MODE[predfr[i]->mbmode]==pred_frame){
      p[np++]=predfr[i]->dc;
      pflags|=pflag;
    }
  }
  if(pflags==0)return _pred_last[pred_frame];
  else{
    ret=PRED_SCALE[pflags][0]*p[0];
    /*LOOP VECTORIZES.*/
    for(i=1;i<np;i++)ret+=PRED_SCALE[pflags][i]*p[i];
    ret=OC_DIV_POW2(ret,PRED_SHIFT[pflags],PRED_RMASK[pflags]);
  }
  if((pflags&(OC_PL|OC_PUL|OC_PU))==(OC_PL|OC_PUL|OC_PU)){
    if(abs(ret-p[2])>128)ret=p[2];
    else if(abs(ret-p[0])>128)ret=p[0];
    else if(abs(ret-p[1])>128)ret=p[1];
  }
  return ret;
}
