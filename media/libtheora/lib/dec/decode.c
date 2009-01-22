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
    last mod: $Id: decode.c 15403 2008-10-16 12:44:05Z tterribe $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include "decint.h"
#if defined(OC_DUMP_IMAGES)
# include <stdio.h>
# include "png.h"
#endif

/*No post-processing.*/
#define OC_PP_LEVEL_DISABLED  (0)
/*Keep track of DC qi for each block only.*/
#define OC_PP_LEVEL_TRACKDCQI (1)
/*Deblock the luma plane.*/
#define OC_PP_LEVEL_DEBLOCKY  (2)
/*Dering the luma plane.*/
#define OC_PP_LEVEL_DERINGY   (3)
/*Stronger luma plane deringing.*/
#define OC_PP_LEVEL_SDERINGY  (4)
/*Deblock the chroma planes.*/
#define OC_PP_LEVEL_DEBLOCKC  (5)
/*Dering the chroma planes.*/
#define OC_PP_LEVEL_DERINGC   (6)
/*Stronger chroma plane deringing.*/
#define OC_PP_LEVEL_SDERINGC  (7)
/*Maximum valid post-processing level.*/
#define OC_PP_LEVEL_MAX       (7)



/*The mode alphabets for the various mode coding schemes.
  Scheme 0 uses a custom alphabet, which is not stored in this table.*/
static const int OC_MODE_ALPHABETS[7][OC_NMODES]={
  /*Last MV dominates */
  {
    OC_MODE_INTER_MV_LAST,OC_MODE_INTER_MV_LAST2,OC_MODE_INTER_MV,
    OC_MODE_INTER_NOMV,OC_MODE_INTRA,OC_MODE_GOLDEN_NOMV,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  },
  {
    OC_MODE_INTER_MV_LAST,OC_MODE_INTER_MV_LAST2,OC_MODE_INTER_NOMV,
    OC_MODE_INTER_MV,OC_MODE_INTRA,OC_MODE_GOLDEN_NOMV,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  },
  {
    OC_MODE_INTER_MV_LAST,OC_MODE_INTER_MV,OC_MODE_INTER_MV_LAST2,
    OC_MODE_INTER_NOMV,OC_MODE_INTRA,OC_MODE_GOLDEN_NOMV,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  },
  {
    OC_MODE_INTER_MV_LAST,OC_MODE_INTER_MV,OC_MODE_INTER_NOMV,
    OC_MODE_INTER_MV_LAST2,OC_MODE_INTRA,OC_MODE_GOLDEN_NOMV,
    OC_MODE_GOLDEN_MV,OC_MODE_INTER_MV_FOUR
  },
  /*No MV dominates.*/
  {
    OC_MODE_INTER_NOMV,OC_MODE_INTER_MV_LAST,OC_MODE_INTER_MV_LAST2,
    OC_MODE_INTER_MV,OC_MODE_INTRA,OC_MODE_GOLDEN_NOMV,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  },
  {
    OC_MODE_INTER_NOMV,OC_MODE_GOLDEN_NOMV,OC_MODE_INTER_MV_LAST,
    OC_MODE_INTER_MV_LAST2,OC_MODE_INTER_MV,OC_MODE_INTRA,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  },
  /*Default ordering.*/
  {
    OC_MODE_INTER_NOMV,OC_MODE_INTRA,OC_MODE_INTER_MV,OC_MODE_INTER_MV_LAST,
    OC_MODE_INTER_MV_LAST2,OC_MODE_GOLDEN_NOMV,OC_MODE_GOLDEN_MV,
    OC_MODE_INTER_MV_FOUR
  }
};


static int oc_sb_run_unpack(oggpack_buffer *_opb){
  long bits;
  int ret;
  /*Coding scheme:
       Codeword            Run Length
     0                       1
     10x                     2-3
     110x                    4-5
     1110xx                  6-9
     11110xxx                10-17
     111110xxxx              18-33
     111111xxxxxxxxxxxx      34-4129*/
  theorapackB_read1(_opb,&bits);
  if(bits==0)return 1;
  theorapackB_read(_opb,2,&bits);
  if((bits&2)==0)return 2+(int)bits;
  else if((bits&1)==0){
    theorapackB_read1(_opb,&bits);
    return 4+(int)bits;
  }
  theorapackB_read(_opb,3,&bits);
  if((bits&4)==0)return 6+(int)bits;
  else if((bits&2)==0){
    ret=10+((bits&1)<<2);
    theorapackB_read(_opb,2,&bits);
    return ret+(int)bits;
  }
  else if((bits&1)==0){
    theorapackB_read(_opb,4,&bits);
    return 18+(int)bits;
  }
  theorapackB_read(_opb,12,&bits);
  return 34+(int)bits;
}

static int oc_block_run_unpack(oggpack_buffer *_opb){
  long bits;
  long bits2;
  /*Coding scheme:
     Codeword             Run Length
     0x                      1-2
     10x                     3-4
     110x                    5-6
     1110xx                  7-10
     11110xx                 11-14
     11111xxxx               15-30*/
  theorapackB_read(_opb,2,&bits);
  if((bits&2)==0)return 1+(int)bits;
  else if((bits&1)==0){
    theorapackB_read1(_opb,&bits);
    return 3+(int)bits;
  }
  theorapackB_read(_opb,2,&bits);
  if((bits&2)==0)return 5+(int)bits;
  else if((bits&1)==0){
    theorapackB_read(_opb,2,&bits);
    return 7+(int)bits;
  }
  theorapackB_read(_opb,3,&bits);
  if((bits&4)==0)return 11+bits;
  theorapackB_read(_opb,2,&bits2);
  return 15+((bits&3)<<2)+bits2;
}



static int oc_dec_init(oc_dec_ctx *_dec,const th_info *_info,
 const th_setup_info *_setup){
  int qti;
  int pli;
  int qi;
  int ret;
  ret=oc_state_init(&_dec->state,_info);
  if(ret<0)return ret;
  oc_huff_trees_copy(_dec->huff_tables,
   (const oc_huff_node *const *)_setup->huff_tables);
  for(qti=0;qti<2;qti++)for(pli=0;pli<3;pli++){
    _dec->state.dequant_tables[qti][pli]=
     _dec->state.dequant_table_data[qti][pli];
  }
  oc_dequant_tables_init(_dec->state.dequant_tables,_dec->pp_dc_scale,
   &_setup->qinfo);
  for(qi=0;qi<64;qi++){
    int qsum;
    qsum=0;
    for(qti=0;qti<2;qti++)for(pli=0;pli<3;pli++){
      qsum+=_dec->state.dequant_tables[qti][pli][qi][18]+
       _dec->state.dequant_tables[qti][pli][qi][19]+
       _dec->state.dequant_tables[qti][pli][qi][26]+
       _dec->state.dequant_tables[qti][pli][qi][27]<<(pli==0);
    }
    _dec->pp_sharp_mod[qi]=-(qsum>>11);
  }
  _dec->dct_tokens=(unsigned char **)oc_calloc_2d(64,
   _dec->state.nfrags,sizeof(_dec->dct_tokens[0][0]));
  _dec->extra_bits=(ogg_uint16_t **)oc_calloc_2d(64,
   _dec->state.nfrags,sizeof(_dec->extra_bits[0][0]));
  memcpy(_dec->state.loop_filter_limits,_setup->qinfo.loop_filter_limits,
   sizeof(_dec->state.loop_filter_limits));
  _dec->pp_level=OC_PP_LEVEL_DISABLED;
  _dec->dc_qis=NULL;
  _dec->variances=NULL;
  _dec->pp_frame_data=NULL;
  _dec->stripe_cb.ctx=NULL;
  _dec->stripe_cb.stripe_decoded=NULL;
  return 0;
}

static void oc_dec_clear(oc_dec_ctx *_dec){
  _ogg_free(_dec->pp_frame_data);
  _ogg_free(_dec->variances);
  _ogg_free(_dec->dc_qis);
  oc_free_2d(_dec->extra_bits);
  oc_free_2d(_dec->dct_tokens);
  oc_huff_trees_clear(_dec->huff_tables);
  oc_state_clear(&_dec->state);
}


static int oc_dec_frame_header_unpack(oc_dec_ctx *_dec){
  long val;
  /*Check to make sure this is a data packet.*/
  theorapackB_read1(&_dec->opb,&val);
  if(val!=0)return TH_EBADPACKET;
  /*Read in the frame type (I or P).*/
  theorapackB_read1(&_dec->opb,&val);
  _dec->state.frame_type=(int)val;
  /*Read in the current qi.*/
  theorapackB_read(&_dec->opb,6,&val);
  _dec->state.qis[0]=(int)val;
  theorapackB_read1(&_dec->opb,&val);
  if(!val)_dec->state.nqis=1;
  else{
    theorapackB_read(&_dec->opb,6,&val);
    _dec->state.qis[1]=(int)val;
    theorapackB_read1(&_dec->opb,&val);
    if(!val)_dec->state.nqis=2;
    else{
      theorapackB_read(&_dec->opb,6,&val);
      _dec->state.qis[2]=(int)val;
      _dec->state.nqis=3;
    }
  }
  if(_dec->state.frame_type==OC_INTRA_FRAME){
    /*Keyframes have 3 unused configuration bits, holdovers from VP3 days.
      Most of the other unused bits in the VP3 headers were eliminated.
      I don't know why these remain.*/
    /* I wanted to eliminate wasted bits, but not all config wiggle room --Monty */
    theorapackB_read(&_dec->opb,3,&val);
    if(val!=0)return TH_EIMPL;
  }
  return 0;
}

/*Mark all fragments as coded and in OC_MODE_INTRA.
  This also builds up the coded fragment list (in coded order), and clears the
   uncoded fragment list.
  It does not update the coded macro block list, as that is not used when
   decoding INTRA frames.*/
static void oc_dec_mark_all_intra(oc_dec_ctx *_dec){
  oc_sb *sb;
  oc_sb *sb_end;
  int    pli;
  int    ncoded_fragis;
  int    prev_ncoded_fragis;
  prev_ncoded_fragis=ncoded_fragis=0;
  sb=sb_end=_dec->state.sbs;
  for(pli=0;pli<3;pli++){
    const oc_fragment_plane *fplane;
    fplane=_dec->state.fplanes+pli;
    sb_end+=fplane->nsbs;
    for(;sb<sb_end;sb++){
      int quadi;
      for(quadi=0;quadi<4;quadi++)if(sb->quad_valid&1<<quadi){
        int bi;
        for(bi=0;bi<4;bi++){
          int fragi;
          fragi=sb->map[quadi][bi];
          if(fragi>=0){
            oc_fragment *frag;
            frag=_dec->state.frags+fragi;
            frag->coded=1;
            frag->mbmode=OC_MODE_INTRA;
            _dec->state.coded_fragis[ncoded_fragis++]=fragi;
          }
        }
      }
    }
    _dec->state.ncoded_fragis[pli]=ncoded_fragis-prev_ncoded_fragis;
    prev_ncoded_fragis=ncoded_fragis;
    _dec->state.nuncoded_fragis[pli]=0;
  }
}

/*Decodes the bit flags for whether or not each super block is partially coded
   or not.
  Return: The number of partially coded super blocks.*/
static int oc_dec_partial_sb_flags_unpack(oc_dec_ctx *_dec){
  oc_sb *sb;
  oc_sb *sb_end;
  long   val;
  int    flag;
  int    npartial;
  int    run_count;
  theorapackB_read1(&_dec->opb,&val);
  flag=(int)val;
  sb=_dec->state.sbs;
  sb_end=sb+_dec->state.nsbs;
  run_count=npartial=0;
  while(sb<sb_end){
    int full_run;
    run_count=oc_sb_run_unpack(&_dec->opb);
    full_run=run_count>=4129;
    do{
      sb->coded_partially=flag;
      sb->coded_fully=0;
      npartial+=flag;
      sb++;
    }
    while(--run_count>0&&sb<sb_end);
    if(full_run&&sb<sb_end){
      theorapackB_read1(&_dec->opb,&val);
      flag=(int)val;
    }
    else flag=!flag;
  }
  /*TODO: run_count should be 0 here.
    If it's not, we should issue a warning of some kind.*/
  return npartial;
}

/*Decodes the bit flags for whether or not each non-partially-coded super
   block is fully coded or not.
  This function should only be called if there is at least one
   non-partially-coded super block.
  Return: The number of partially coded super blocks.*/
static void oc_dec_coded_sb_flags_unpack(oc_dec_ctx *_dec){
  oc_sb *sb;
  oc_sb *sb_end;
  long   val;
  int    flag;
  int    run_count;
  sb=_dec->state.sbs;
  sb_end=sb+_dec->state.nsbs;
  /*Skip partially coded super blocks.*/
  for(;sb->coded_partially;sb++);
  theorapackB_read1(&_dec->opb,&val);
  flag=(int)val;
  while(sb<sb_end){
    int full_run;
    run_count=oc_sb_run_unpack(&_dec->opb);
    full_run=run_count>=4129;
    for(;sb<sb_end;sb++){
      if(sb->coded_partially)continue;
      if(run_count--<=0)break;
      sb->coded_fully=flag;
    }
    if(full_run&&sb<sb_end){
      theorapackB_read1(&_dec->opb,&val);
      flag=(int)val;
    }
    else flag=!flag;
  }
  /*TODO: run_count should be 0 here.
    If it's not, we should issue a warning of some kind.*/
}

static void oc_dec_coded_flags_unpack(oc_dec_ctx *_dec){
  oc_sb *sb;
  oc_sb *sb_end;
  long   val;
  int    npartial;
  int    pli;
  int    flag;
  int    run_count;
  int    ncoded_fragis;
  int    prev_ncoded_fragis;
  int    nuncoded_fragis;
  int    prev_nuncoded_fragis;
  npartial=oc_dec_partial_sb_flags_unpack(_dec);
  if(npartial<_dec->state.nsbs)oc_dec_coded_sb_flags_unpack(_dec);
  if(npartial>0){
    theorapackB_read1(&_dec->opb,&val);
    flag=!(int)val;
  }
  else flag=0;
  run_count=0;
  prev_ncoded_fragis=ncoded_fragis=prev_nuncoded_fragis=nuncoded_fragis=0;
  sb=sb_end=_dec->state.sbs;
  for(pli=0;pli<3;pli++){
    const oc_fragment_plane *fplane;
    fplane=_dec->state.fplanes+pli;
    sb_end+=fplane->nsbs;
    for(;sb<sb_end;sb++){
      int quadi;
      for(quadi=0;quadi<4;quadi++)if(sb->quad_valid&1<<quadi){
        int bi;
        for(bi=0;bi<4;bi++){
          int fragi;
          fragi=sb->map[quadi][bi];
          if(fragi>=0){
            oc_fragment *frag;
            frag=_dec->state.frags+fragi;
            if(sb->coded_fully)frag->coded=1;
            else if(!sb->coded_partially)frag->coded=0;
            else{
              if(run_count<=0){
                run_count=oc_block_run_unpack(&_dec->opb);
                flag=!flag;
              }
              run_count--;
              frag->coded=flag;
            }
            if(frag->coded)_dec->state.coded_fragis[ncoded_fragis++]=fragi;
            else *(_dec->state.uncoded_fragis-++nuncoded_fragis)=fragi;
          }
        }
      }
    }
    _dec->state.ncoded_fragis[pli]=ncoded_fragis-prev_ncoded_fragis;
    prev_ncoded_fragis=ncoded_fragis;
    _dec->state.nuncoded_fragis[pli]=nuncoded_fragis-prev_nuncoded_fragis;
    prev_nuncoded_fragis=nuncoded_fragis;
  }
  /*TODO: run_count should be 0 here.
    If it's not, we should issue a warning of some kind.*/
}



typedef int (*oc_mode_unpack_func)(oggpack_buffer *_opb);

static int oc_vlc_mode_unpack(oggpack_buffer *_opb){
  long val;
  int  i;
  for(i=0;i<7;i++){
    theorapackB_read1(_opb,&val);
    if(!val)break;
  }
  return i;
}

static int oc_clc_mode_unpack(oggpack_buffer *_opb){
  long val;
  theorapackB_read(_opb,3,&val);
  return (int)val;
}

/*Unpacks the list of macro block modes for INTER frames.*/
static void oc_dec_mb_modes_unpack(oc_dec_ctx *_dec){
  oc_mode_unpack_func  mode_unpack;
  oc_mb               *mb;
  oc_mb               *mb_end;
  const int           *alphabet;
  long                 val;
  int                  scheme0_alphabet[8];
  int                  mode_scheme;
  theorapackB_read(&_dec->opb,3,&val);
  mode_scheme=(int)val;
  if(mode_scheme==0){
    int mi;
    /*Just in case, initialize the modes to something.
      If the bitstream doesn't contain each index exactly once, it's likely
       corrupt and the rest of the packet is garbage anyway, but this way we
       won't crash, and we'll decode SOMETHING.*/
    /*LOOP VECTORIZES.*/
    for(mi=0;mi<OC_NMODES;mi++)scheme0_alphabet[mi]=OC_MODE_INTER_NOMV;
    for(mi=0;mi<OC_NMODES;mi++){
      theorapackB_read(&_dec->opb,3,&val);
      scheme0_alphabet[val]=OC_MODE_ALPHABETS[6][mi];
    }
    alphabet=scheme0_alphabet;
  }
  else alphabet=OC_MODE_ALPHABETS[mode_scheme-1];
  if(mode_scheme==7)mode_unpack=oc_clc_mode_unpack;
  else mode_unpack=oc_vlc_mode_unpack;
  mb=_dec->state.mbs;
  mb_end=mb+_dec->state.nmbs;
  for(;mb<mb_end;mb++){
    if(mb->mode!=OC_MODE_INVALID){
      int bi;
      for(bi=0;bi<4;bi++){
        int fragi;
        fragi=mb->map[0][bi];
        if(fragi>=0&&_dec->state.frags[fragi].coded)break;
      }
      if(bi<4)mb->mode=alphabet[(*mode_unpack)(&_dec->opb)];
      else mb->mode=OC_MODE_INTER_NOMV;
    }
  }
}



typedef int (*oc_mv_comp_unpack_func)(oggpack_buffer *_opb);

static int oc_vlc_mv_comp_unpack(oggpack_buffer *_opb){
  long bits;
  int  mvsigned[2];
  theorapackB_read(_opb,3,&bits);
  switch(bits){
    case  0:return 0;
    case  1:return 1;
    case  2:return -1;
    case  3:
    case  4:{
      mvsigned[0]=(int)(bits-1);
      theorapackB_read1(_opb,&bits);
    }break;
    /*case  5:
    case  6:
    case  7:*/
    default:{
      mvsigned[0]=1<<bits-3;
      theorapackB_read(_opb,bits-2,&bits);
      mvsigned[0]+=(int)(bits>>1);
      bits&=1;
    }break;
  }
  mvsigned[1]=-mvsigned[0];
  return mvsigned[bits];
}

static int oc_clc_mv_comp_unpack(oggpack_buffer *_opb){
  long bits;
  int  mvsigned[2];
  theorapackB_read(_opb,6,&bits);
  mvsigned[0]=bits>>1;
  mvsigned[1]=-mvsigned[0];
  return mvsigned[bits&1];
}

/*Unpacks the list of motion vectors for INTER frames, and propagtes the macro
   block modes and motion vectors to the individual fragments.*/
static void oc_dec_mv_unpack_and_frag_modes_fill(oc_dec_ctx *_dec){
  oc_set_chroma_mvs_func  set_chroma_mvs;
  oc_mv_comp_unpack_func  mv_comp_unpack;
  oc_mb                  *mb;
  oc_mb                  *mb_end;
  const int              *map_idxs;
  long                    val;
  int                     map_nidxs;
  oc_mv                   last_mv[2];
  oc_mv                   cbmvs[4];
  set_chroma_mvs=OC_SET_CHROMA_MVS_TABLE[_dec->state.info.pixel_fmt];
  theorapackB_read1(&_dec->opb,&val);
  mv_comp_unpack=val?oc_clc_mv_comp_unpack:oc_vlc_mv_comp_unpack;
  map_idxs=OC_MB_MAP_IDXS[_dec->state.info.pixel_fmt];
  map_nidxs=OC_MB_MAP_NIDXS[_dec->state.info.pixel_fmt];
  memset(last_mv,0,sizeof(last_mv));
  mb=_dec->state.mbs;
  mb_end=mb+_dec->state.nmbs;
  for(;mb<mb_end;mb++)if(mb->mode!=OC_MODE_INVALID){
    oc_fragment *frag;
    oc_mv        mbmv;
    int          coded[13];
    int          codedi;
    int          ncoded;
    int          mapi;
    int          mapii;
    int          fragi;
    int          mb_mode;
    /*Search for at least one coded fragment.*/
    ncoded=mapii=0;
    do{
      mapi=map_idxs[mapii];
      fragi=mb->map[mapi>>2][mapi&3];
      if(fragi>=0&&_dec->state.frags[fragi].coded)coded[ncoded++]=mapi;
    }
    while(++mapii<map_nidxs);
    if(ncoded<=0)continue;
    mb_mode=mb->mode;
    switch(mb_mode){
      case OC_MODE_INTER_MV_FOUR:{
        oc_mv       lbmvs[4];
        int         bi;
        /*Mark the tail of the list, so we don't accidentally go past it.*/
        coded[ncoded]=-1;
        for(bi=codedi=0;bi<4;bi++){
          if(coded[codedi]==bi){
            codedi++;
            frag=_dec->state.frags+mb->map[0][bi];
            frag->mbmode=mb_mode;
            frag->mv[0]=lbmvs[bi][0]=(signed char)(*mv_comp_unpack)(&_dec->opb);
            frag->mv[1]=lbmvs[bi][1]=(signed char)(*mv_comp_unpack)(&_dec->opb);
          }
          else lbmvs[bi][0]=lbmvs[bi][1]=0;
        }
        if(codedi>0){
          last_mv[1][0]=last_mv[0][0];
          last_mv[1][1]=last_mv[0][1];
          last_mv[0][0]=lbmvs[coded[codedi-1]][0];
          last_mv[0][1]=lbmvs[coded[codedi-1]][1];
        }
        if(codedi<ncoded){
          (*set_chroma_mvs)(cbmvs,(const oc_mv *)lbmvs);
          for(;codedi<ncoded;codedi++){
            mapi=coded[codedi];
            bi=mapi&3;
            frag=_dec->state.frags+mb->map[mapi>>2][bi];
            frag->mbmode=mb_mode;
            frag->mv[0]=cbmvs[bi][0];
            frag->mv[1]=cbmvs[bi][1];
          }
        }
      }break;
      case OC_MODE_INTER_MV:{
        last_mv[1][0]=last_mv[0][0];
        last_mv[1][1]=last_mv[0][1];
        mbmv[0]=last_mv[0][0]=(signed char)(*mv_comp_unpack)(&_dec->opb);
        mbmv[1]=last_mv[0][1]=(signed char)(*mv_comp_unpack)(&_dec->opb);
      }break;
      case OC_MODE_INTER_MV_LAST:{
        mbmv[0]=last_mv[0][0];
        mbmv[1]=last_mv[0][1];
      }break;
      case OC_MODE_INTER_MV_LAST2:{
        mbmv[0]=last_mv[1][0];
        mbmv[1]=last_mv[1][1];
        last_mv[1][0]=last_mv[0][0];
        last_mv[1][1]=last_mv[0][1];
        last_mv[0][0]=mbmv[0];
        last_mv[0][1]=mbmv[1];
      }break;
      case OC_MODE_GOLDEN_MV:{
        mbmv[0]=(signed char)(*mv_comp_unpack)(&_dec->opb);
        mbmv[1]=(signed char)(*mv_comp_unpack)(&_dec->opb);
      }break;
      default:mbmv[0]=mbmv[1]=0;break;
    }
    /*4MV mode fills in the fragments itself.
      For all other modes we can use this common code.*/
    if(mb_mode!=OC_MODE_INTER_MV_FOUR){
      for(codedi=0;codedi<ncoded;codedi++){
        mapi=coded[codedi];
        fragi=mb->map[mapi>>2][mapi&3];
        frag=_dec->state.frags+fragi;
        frag->mbmode=mb_mode;
        frag->mv[0]=mbmv[0];
        frag->mv[1]=mbmv[1];
      }
    }
  }
}

static void oc_dec_block_qis_unpack(oc_dec_ctx *_dec){
  oc_fragment *frag;
  int         *coded_fragi;
  int         *coded_fragi_end;
  int          ncoded_fragis;
  ncoded_fragis=_dec->state.ncoded_fragis[0]+
   _dec->state.ncoded_fragis[1]+_dec->state.ncoded_fragis[2];
  if(ncoded_fragis<=0)return;
  coded_fragi=_dec->state.coded_fragis;
  coded_fragi_end=coded_fragi+ncoded_fragis;
  if(_dec->state.nqis==1){
    /*If this frame has only a single qi value, then just set it in all coded
       fragments.*/
    while(coded_fragi<coded_fragi_end){
      _dec->state.frags[*coded_fragi++].qi=_dec->state.qis[0];
    }
  }
  else{
    long val;
    int  flag;
    int  nqi1;
    int  run_count;
    /*Otherwise, we decode a qi index for each fragment, using two passes of
      the same binary RLE scheme used for super-block coded bits.
     The first pass marks each fragment as having a qii of 0 or greater than
      0, and the second pass (if necessary), distinguishes between a qii of
      1 and 2.
     At first we just store the qii in the fragment.
     After all the qii's are decoded, we make a final pass to replace them
      with the corresponding qi's for this frame.*/
    theorapackB_read1(&_dec->opb,&val);
    flag=(int)val;
    run_count=nqi1=0;
    while(coded_fragi<coded_fragi_end){
      int full_run;
      run_count=oc_sb_run_unpack(&_dec->opb);
      full_run=run_count>=4129;
      do{
        _dec->state.frags[*coded_fragi++].qi=flag;
        nqi1+=flag;
      }
      while(--run_count>0&&coded_fragi<coded_fragi_end);
      if(full_run&&coded_fragi<coded_fragi_end){
        theorapackB_read1(&_dec->opb,&val);
        flag=(int)val;
      }
      else flag=!flag;
    }
    /*TODO: run_count should be 0 here.
      If it's not, we should issue a warning of some kind.*/
    /*If we have 3 different qi's for this frame, and there was at least one
       fragment with a non-zero qi, make the second pass.*/
    if(_dec->state.nqis==3&&nqi1>0){
      /*Skip qii==0 fragments.*/
      for(coded_fragi=_dec->state.coded_fragis;
       _dec->state.frags[*coded_fragi].qi==0;coded_fragi++);
      theorapackB_read1(&_dec->opb,&val);
      flag=(int)val;
      while(coded_fragi<coded_fragi_end){
        int full_run;
        run_count=oc_sb_run_unpack(&_dec->opb);
        full_run=run_count>=4129;
        for(;coded_fragi<coded_fragi_end;coded_fragi++){
          oc_fragment *frag;
          frag=_dec->state.frags+*coded_fragi;
          if(frag->qi==0)continue;
          if(run_count--<=0)break;
          frag->qi+=flag;
        }
        if(full_run&&coded_fragi<coded_fragi_end){
          theorapackB_read1(&_dec->opb,&val);
          flag=(int)val;
        }
        else flag=!flag;
      }
      /*TODO: run_count should be 0 here.
        If it's not, we should issue a warning of some kind.*/
    }
    /*Finally, translate qii's to qi's.*/
    for(coded_fragi=_dec->state.coded_fragis;coded_fragi<coded_fragi_end;
     coded_fragi++){
      frag=_dec->state.frags+*coded_fragi;
      frag->qi=_dec->state.qis[frag->qi];
    }
  }
}



/*Returns the decoded value of the given token.
  It CANNOT be called for any of the EOB tokens.
  _token:      The token value to skip.
  _extra_bits: The extra bits attached to this token.
  Return: The decoded coefficient value.*/
typedef int (*oc_token_dec1val_func)(int _token,int _extra_bits);

/*Handles zero run tokens.*/
static int oc_token_dec1val_zrl(void){
  return 0;
}

/*Handles 1, -1, 2 and -2 tokens.*/
static int oc_token_dec1val_const(int _token){
  static const int CONST_VALS[4]={1,-1,2,-2};
  return CONST_VALS[_token-OC_NDCT_ZRL_TOKEN_MAX];
}

/*Handles DCT value tokens category 2.*/
static int oc_token_dec1val_cat2(int _token,int _extra_bits){
  int valsigned[2];
  valsigned[0]=_token-OC_DCT_VAL_CAT2+3;
  valsigned[1]=-valsigned[0];
  return valsigned[_extra_bits];
}

/*Handles DCT value tokens categories 3 through 8.*/
static int oc_token_dec1val_cati(int _token,int _extra_bits){
  static const int VAL_CAT_OFFS[6]={
    OC_NDCT_VAL_CAT2_SIZE+3,
    OC_NDCT_VAL_CAT2_SIZE+5,
    OC_NDCT_VAL_CAT2_SIZE+9,
    OC_NDCT_VAL_CAT2_SIZE+17,
    OC_NDCT_VAL_CAT2_SIZE+33,
    OC_NDCT_VAL_CAT2_SIZE+65
  };
  static const int VAL_CAT_MASKS[6]={
    0x001,0x003,0x007,0x00F,0x01F,0x1FF
  };
  static const int VAL_CAT_SHIFTS[6]={1,2,3,4,5,9};
  int valsigned[2];
  int cati;
  cati=_token-OC_NDCT_VAL_CAT2_MAX;
  valsigned[0]=VAL_CAT_OFFS[cati]+(_extra_bits&VAL_CAT_MASKS[cati]);
  valsigned[1]=-valsigned[0];
  return valsigned[_extra_bits>>VAL_CAT_SHIFTS[cati]&1];
}

/*A jump table for compute the first coefficient value the given token value
   represents.*/
static const oc_token_dec1val_func OC_TOKEN_DEC1VAL_TABLE[TH_NDCT_TOKENS-
 OC_NDCT_EOB_TOKEN_MAX]={
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_const,
  (oc_token_dec1val_func)oc_token_dec1val_const,
  (oc_token_dec1val_func)oc_token_dec1val_const,
  (oc_token_dec1val_func)oc_token_dec1val_const,
  oc_token_dec1val_cat2,
  oc_token_dec1val_cat2,
  oc_token_dec1val_cat2,
  oc_token_dec1val_cat2,
  oc_token_dec1val_cati,
  oc_token_dec1val_cati,
  oc_token_dec1val_cati,
  oc_token_dec1val_cati,
  oc_token_dec1val_cati,
  oc_token_dec1val_cati,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl,
  (oc_token_dec1val_func)oc_token_dec1val_zrl
};

/*Returns the decoded value of the given token.
  It CANNOT be called for any of the EOB tokens.
  _token:      The token value to skip.
  _extra_bits: The extra bits attached to this token.
  Return: The decoded coefficient value.*/
static int oc_dct_token_dec1val(int _token,int _extra_bits){
  return (*OC_TOKEN_DEC1VAL_TABLE[_token-OC_NDCT_EOB_TOKEN_MAX])(_token,
   _extra_bits);
}

/*Unpacks the DC coefficient tokens.
  Unlike when unpacking the AC coefficient tokens, we actually need to decode
   the DC coefficient values now so that we can do DC prediction.
  _huff_idx:   The index of the Huffman table to use for each color plane.
  _ntoks_left: The number of tokens left to be decoded in each color plane for
                each coefficient.
               This is updated as EOB tokens and zero run tokens are decoded.
  Return: The length of any outstanding EOB run.*/
static int oc_dec_dc_coeff_unpack(oc_dec_ctx *_dec,int _huff_idxs[3],
 int _ntoks_left[3][64]){
  long  val;
  int  *coded_fragi;
  int  *coded_fragi_end;
  int   run_counts[64];
  int   cfi;
  int   eobi;
  int   eobs;
  int   ti;
  int   ebi;
  int   pli;
  int   rli;
  eobs=0;
  ti=ebi=0;
  coded_fragi_end=coded_fragi=_dec->state.coded_fragis;
  for(pli=0;pli<3;pli++){
    coded_fragi_end+=_dec->state.ncoded_fragis[pli];
    memset(run_counts,0,sizeof(run_counts));
    _dec->eob_runs[pli][0]=eobs;
    /*Continue any previous EOB run, if there was one.*/
    for(eobi=eobs;eobi-->0&&coded_fragi<coded_fragi_end;){
      _dec->state.frags[*coded_fragi++].dc=0;
    }
    cfi=0;
    while(eobs<_ntoks_left[pli][0]-cfi){
      int token;
      int neb;
      int eb;
      int skip;
      cfi+=eobs;
      run_counts[63]+=eobs;
      token=oc_huff_token_decode(&_dec->opb,
       _dec->huff_tables[_huff_idxs[pli]]);
      _dec->dct_tokens[0][ti++]=(unsigned char)token;
      neb=OC_DCT_TOKEN_EXTRA_BITS[token];
      if(neb){
        theorapackB_read(&_dec->opb,neb,&val);
        eb=(int)val;
        _dec->extra_bits[0][ebi++]=(ogg_uint16_t)eb;
      }
      else eb=0;
      skip=oc_dct_token_skip(token,eb);
      if(skip<0){
        eobs=eobi=-skip;
        while(eobi-->0&&coded_fragi<coded_fragi_end){
          _dec->state.frags[*coded_fragi++].dc=0;
        }
      }
      else{
        run_counts[skip-1]++;
        cfi++;
        eobs=0;
        _dec->state.frags[*coded_fragi++].dc=oc_dct_token_dec1val(token,eb);
      }
    }
    _dec->ti0[pli][0]=ti;
    _dec->ebi0[pli][0]=ebi;
    /*Set the EOB count to the portion of the last EOB run which extends past
       this coefficient.*/
    eobs=eobs+cfi-_ntoks_left[pli][0];
    /*Add the portion of the last EOB which was included in this coefficient to
       to the longest run length.*/
    run_counts[63]+=_ntoks_left[pli][0]-cfi;
    /*And convert the run_counts array to a moment table.*/
    for(rli=63;rli-->0;)run_counts[rli]+=run_counts[rli+1];
    /*Finally, subtract off the number of coefficients that have been
       accounted for by runs started in this coefficient.*/
    for(rli=64;rli-->0;)_ntoks_left[pli][rli]-=run_counts[rli];
  }
  return eobs;
}

/*Unpacks the AC coefficient tokens.
  This can completely discard coefficient values while unpacking, and so is
   somewhat simpler than unpacking the DC coefficient tokens.
  _huff_idx:   The index of the Huffman table to use for each color plane.
  _ntoks_left: The number of tokens left to be decoded in each color plane for
                each coefficient.
               This is updated as EOB tokens and zero run tokens are decoded.
  _eobs:       The length of any outstanding EOB run from previous
                coefficients.
  Return: The length of any outstanding EOB run.*/
static int oc_dec_ac_coeff_unpack(oc_dec_ctx *_dec,int _zzi,int _huff_idxs[3],
 int _ntoks_left[3][64],int _eobs){
  long val;
  int  run_counts[64];
  int  cfi;
  int  ti;
  int  ebi;
  int  pli;
  int  rli;
  ti=ebi=0;
  for(pli=0;pli<3;pli++){
    memset(run_counts,0,sizeof(run_counts));
    _dec->eob_runs[pli][_zzi]=_eobs;
    cfi=0;
    while(_eobs<_ntoks_left[pli][_zzi]-cfi){
      int token;
      int neb;
      int eb;
      int skip;
      cfi+=_eobs;
      run_counts[63]+=_eobs;
      token=oc_huff_token_decode(&_dec->opb,
       _dec->huff_tables[_huff_idxs[pli]]);
      _dec->dct_tokens[_zzi][ti++]=(unsigned char)token;
      neb=OC_DCT_TOKEN_EXTRA_BITS[token];
      if(neb){
        theorapackB_read(&_dec->opb,neb,&val);
        eb=(int)val;
        _dec->extra_bits[_zzi][ebi++]=(ogg_uint16_t)eb;
      }
      else eb=0;
      skip=oc_dct_token_skip(token,eb);
      if(skip<0)_eobs=-skip;
      else{
        run_counts[skip-1]++;
        cfi++;
        _eobs=0;
      }
    }
    _dec->ti0[pli][_zzi]=ti;
    _dec->ebi0[pli][_zzi]=ebi;
    /*Set the EOB count to the portion of the last EOB run which extends past
       this coefficient.*/
    _eobs=_eobs+cfi-_ntoks_left[pli][_zzi];
    /*Add the portion of the last EOB which was included in this coefficient to
       to the longest run length.*/
    run_counts[63]+=_ntoks_left[pli][_zzi]-cfi;
    /*And convert the run_counts array to a moment table.*/
    for(rli=63;rli-->0;)run_counts[rli]+=run_counts[rli+1];
    /*Finally, subtract off the number of coefficients that have been
       accounted for by runs started in this coefficient.*/
    for(rli=64-_zzi;rli-->0;)_ntoks_left[pli][_zzi+rli]-=run_counts[rli];
  }
  return _eobs;
}

/*Tokens describing the DCT coefficients that belong to each fragment are
   stored in the bitstream grouped by coefficient, not by fragment.

  This means that we either decode all the tokens in order, building up a
   separate coefficient list for each fragment as we go, and then go back and
   do the iDCT on each fragment, or we have to create separate lists of tokens
   for each coefficient, so that we can pull the next token required off the
   head of the appropriate list when decoding a specific fragment.

  The former was VP3's choice, and it meant 2*w*h extra storage for all the
   decoded coefficient values.

  We take the second option, which lets us store just one or three bytes per
   token (generally far fewer than the number of coefficients, due to EOB
   tokens and zero runs), and which requires us to only maintain a counter for
   each of the 64 coefficients, instead of a counter for every fragment to
   determine where the next token goes.

  Actually, we use 3 counters per coefficient, one for each color plane, so we
   can decode all color planes simultaneously.

  This lets color conversion, etc., be done as soon as a full MCU (one or
   two super block rows) is decoded, while the image data is still in cache.*/

static void oc_dec_residual_tokens_unpack(oc_dec_ctx *_dec){
  static const int OC_HUFF_LIST_MAX[5]={1,6,15,28,64};
  long val;
  int  ntoks_left[3][64];
  int  huff_idxs[3];
  int  pli;
  int  zzi;
  int  hgi;
  int  huffi_y;
  int  huffi_c;
  int  eobs;
  for(pli=0;pli<3;pli++)for(zzi=0;zzi<64;zzi++){
    ntoks_left[pli][zzi]=_dec->state.ncoded_fragis[pli];
  }
  theorapackB_read(&_dec->opb,4,&val);
  huffi_y=(int)val;
  theorapackB_read(&_dec->opb,4,&val);
  huffi_c=(int)val;
  huff_idxs[0]=huffi_y;
  huff_idxs[1]=huff_idxs[2]=huffi_c;
  _dec->eob_runs[0][0]=0;
  eobs=oc_dec_dc_coeff_unpack(_dec,huff_idxs,ntoks_left);
  theorapackB_read(&_dec->opb,4,&val);
  huffi_y=(int)val;
  theorapackB_read(&_dec->opb,4,&val);
  huffi_c=(int)val;
  zzi=1;
  for(hgi=1;hgi<5;hgi++){
    huff_idxs[0]=huffi_y+(hgi<<4);
    huff_idxs[1]=huff_idxs[2]=huffi_c+(hgi<<4);
    for(;zzi<OC_HUFF_LIST_MAX[hgi];zzi++){
      eobs=oc_dec_ac_coeff_unpack(_dec,zzi,huff_idxs,ntoks_left,eobs);
    }
  }
  /*TODO: eobs should be exactly zero, or 4096 or greater.
    The second case occurs when an EOB run of size zero is encountered, which
     gets treated as an infinite EOB run (where infinity is INT_MAX).
    If neither of these conditions holds, then a warning should be issued.*/
}



/*Expands a single token into the given coefficient list.
  This fills in the zeros for zero runs as well as coefficient values, and
   updates the index of the current coefficient.
  It CANNOT be called for any of the EOB tokens.
  _token:      The token value to expand.
  _extra_bits: The extra bits associated with the token.
  _dct_coeffs: The current list of coefficients, in zig-zag order.
  _zzi:        A pointer to the zig-zag index of the next coefficient to write
                to.
               This is updated before the function returns.*/
typedef void (*oc_token_expand_func)(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi);

/*Expands a zero run token.*/
static void oc_token_expand_zrl(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  int zzi;
  zzi=*_zzi;
  do _dct_coeffs[zzi++]=0;
  while(_extra_bits-->0);
  *_zzi=zzi;
}

/*Expands a constant, single-value token.*/
static void oc_token_expand_const(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  _dct_coeffs[(*_zzi)++]=(ogg_int16_t)oc_token_dec1val_const(_token);
}

/*Expands category 2 single-valued tokens.*/
static void oc_token_expand_cat2(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  _dct_coeffs[(*_zzi)++]=
   (ogg_int16_t)oc_token_dec1val_cat2(_token,_extra_bits);
}

/*Expands category 3 through 8 single-valued tokens.*/
static void oc_token_expand_cati(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  _dct_coeffs[(*_zzi)++]=
   (ogg_int16_t)oc_token_dec1val_cati(_token,_extra_bits);
}

/*Expands a category 1a zero run/value combo token.*/
static void oc_token_expand_run_cat1a(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  int zzi;
  int rl;
  zzi=*_zzi;
  /*LOOP VECTORIZES.*/
  for(rl=_token-OC_DCT_RUN_CAT1A+1;rl-->0;)_dct_coeffs[zzi++]=0;
  _dct_coeffs[zzi++]=(ogg_int16_t)(1-(_extra_bits<<1));
  *_zzi=zzi;
}

/*Expands all other zero run/value combo tokens.*/
static void oc_token_expand_run(int _token,int _extra_bits,
 ogg_int16_t _dct_coeffs[128],int *_zzi){
  static const int NZEROS_ADJUST[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    6,10,1,2
  };
  static const int NZEROS_MASK[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    3,7,0,1
  };
  static const int VALUE_SHIFT[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    0,0,0,1
  };
  static const int VALUE_MASK[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    0,0,1,1
  };
  static const int VALUE_ADJUST[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    1,1,2,2
  };
  static const int SIGN_SHIFT[OC_NDCT_RUN_MAX-OC_DCT_RUN_CAT1B]={
    2,3,1,2
  };
  int valsigned[2];
  int zzi;
  int rl;
  _token-=OC_DCT_RUN_CAT1B;
  rl=(_extra_bits&NZEROS_MASK[_token])+NZEROS_ADJUST[_token];
  zzi=*_zzi;
  /*LOOP VECTORIZES.*/
  while(rl-->0)_dct_coeffs[zzi++]=0;
  valsigned[0]=VALUE_ADJUST[_token]+
   (_extra_bits>>VALUE_SHIFT[_token]&VALUE_MASK[_token]);
  valsigned[1]=-valsigned[0];
  _dct_coeffs[zzi++]=(ogg_int16_t)valsigned[
   _extra_bits>>SIGN_SHIFT[_token]];
  *_zzi=zzi;
}

/*A jump table for expanding token values into coefficient values.
  This reduces all the conditional branches, etc., needed to parse these token
   values down to one indirect jump.*/
static const oc_token_expand_func OC_TOKEN_EXPAND_TABLE[TH_NDCT_TOKENS-
 OC_NDCT_EOB_TOKEN_MAX]={
  oc_token_expand_zrl,
  oc_token_expand_zrl,
  oc_token_expand_const,
  oc_token_expand_const,
  oc_token_expand_const,
  oc_token_expand_const,
  oc_token_expand_cat2,
  oc_token_expand_cat2,
  oc_token_expand_cat2,
  oc_token_expand_cat2,
  oc_token_expand_cati,
  oc_token_expand_cati,
  oc_token_expand_cati,
  oc_token_expand_cati,
  oc_token_expand_cati,
  oc_token_expand_cati,
  oc_token_expand_run_cat1a,
  oc_token_expand_run_cat1a,
  oc_token_expand_run_cat1a,
  oc_token_expand_run_cat1a,
  oc_token_expand_run_cat1a,
  oc_token_expand_run,
  oc_token_expand_run,
  oc_token_expand_run,
  oc_token_expand_run
};

/*Expands a single token into the given coefficient list.
  This fills in the zeros for zero runs as well as coefficient values, and
   updates the index of the current coefficient.
  It CANNOT be called for any of the EOB tokens.
  _token:      The token value to expand.
  _extra_bits: The extra bits associated with the token.
  _dct_coeffs: The current list of coefficients, in zig-zag order.
  _zzi:        A pointer to the zig-zag index of the next coefficient to write
                to.
               This is updated before the function returns.*/
static void oc_dct_token_expand(int _token,int _extra_bits,
 ogg_int16_t *_dct_coeffs,int *_zzi){
  (*OC_TOKEN_EXPAND_TABLE[_token-OC_NDCT_EOB_TOKEN_MAX])(_token,
   _extra_bits,_dct_coeffs,_zzi);
}



static int oc_dec_postprocess_init(oc_dec_ctx *_dec){
  /*pp_level 0: disabled; free any memory used and return*/
  if(_dec->pp_level<=OC_PP_LEVEL_DISABLED){
    if(_dec->dc_qis!=NULL){
      _ogg_free(_dec->dc_qis);
      _dec->dc_qis=NULL;
      _ogg_free(_dec->variances);
      _dec->variances=NULL;
      _ogg_free(_dec->pp_frame_data);
      _dec->pp_frame_data=NULL;
    }
    return 1;
  }
  if(_dec->dc_qis==NULL){
    /*If we haven't been tracking DC quantization indices, there's no point in
       starting now.*/
    if(_dec->state.frame_type!=OC_INTRA_FRAME)return 1;
    _dec->dc_qis=(unsigned char *)_ogg_malloc(
     _dec->state.nfrags*sizeof(_dec->dc_qis[0]));
    memset(_dec->dc_qis,_dec->state.qis[0],_dec->state.nfrags);
  }
  else{
    int           *coded_fragi;
    int           *coded_fragi_end;
    unsigned char  qi0;
    /*Update the DC quantization index of each coded block.*/
    qi0=(unsigned char)_dec->state.qis[0];
    coded_fragi_end=_dec->state.coded_fragis+_dec->state.ncoded_fragis[0]+
     _dec->state.ncoded_fragis[1]+_dec->state.ncoded_fragis[2];
    for(coded_fragi=_dec->state.coded_fragis;coded_fragi<coded_fragi_end;
     coded_fragi++){
      _dec->dc_qis[*coded_fragi]=qi0;
    }
  }
  /*pp_level 1: Stop after updating DC quantization indices.*/
  if(_dec->pp_level<=OC_PP_LEVEL_TRACKDCQI){
    if(_dec->variances!=NULL){
      _ogg_free(_dec->variances);
      _dec->variances=NULL;
      _ogg_free(_dec->pp_frame_data);
      _dec->pp_frame_data=NULL;
    }
    return 1;
  }
  if(_dec->variances==NULL||
   _dec->pp_frame_has_chroma!=(_dec->pp_level>=OC_PP_LEVEL_DEBLOCKC)){
    size_t frame_sz;
    frame_sz=_dec->state.info.frame_width*_dec->state.info.frame_height;
    if(_dec->pp_level<OC_PP_LEVEL_DEBLOCKC){
      _dec->variances=(int *)_ogg_realloc(_dec->variances,
       _dec->state.fplanes[0].nfrags*sizeof(_dec->variances[0]));
      _dec->pp_frame_data=(unsigned char *)_ogg_realloc(
       _dec->pp_frame_data,frame_sz*sizeof(_dec->pp_frame_data[0]));
      _dec->pp_frame_buf[0].width=_dec->state.info.frame_width;
      _dec->pp_frame_buf[0].height=_dec->state.info.frame_height;
      _dec->pp_frame_buf[0].stride=-_dec->pp_frame_buf[0].width;
      _dec->pp_frame_buf[0].data=_dec->pp_frame_data+
       (1-_dec->pp_frame_buf[0].height)*_dec->pp_frame_buf[0].stride;
    }
    else{
      size_t y_sz;
      size_t c_sz;
      int    c_w;
      int    c_h;
      _dec->variances=(int *)_ogg_realloc(_dec->variances,
       _dec->state.nfrags*sizeof(_dec->variances[0]));
      y_sz=frame_sz;
      c_w=_dec->state.info.frame_width>>!(_dec->state.info.pixel_fmt&1);
      c_h=_dec->state.info.frame_height>>!(_dec->state.info.pixel_fmt&2);
      c_sz=c_w*c_h;
      frame_sz+=c_sz<<1;
      _dec->pp_frame_data=(unsigned char *)_ogg_realloc(
       _dec->pp_frame_data,frame_sz*sizeof(_dec->pp_frame_data[0]));
      _dec->pp_frame_buf[0].width=_dec->state.info.frame_width;
      _dec->pp_frame_buf[0].height=_dec->state.info.frame_height;
      _dec->pp_frame_buf[0].stride=_dec->pp_frame_buf[0].width;
      _dec->pp_frame_buf[0].data=_dec->pp_frame_data;
      _dec->pp_frame_buf[1].width=c_w;
      _dec->pp_frame_buf[1].height=c_h;
      _dec->pp_frame_buf[1].stride=_dec->pp_frame_buf[1].width;
      _dec->pp_frame_buf[1].data=_dec->pp_frame_buf[0].data+y_sz;
      _dec->pp_frame_buf[2].width=c_w;
      _dec->pp_frame_buf[2].height=c_h;
      _dec->pp_frame_buf[2].stride=_dec->pp_frame_buf[2].width;
      _dec->pp_frame_buf[2].data=_dec->pp_frame_buf[1].data+c_sz;
      oc_ycbcr_buffer_flip(_dec->pp_frame_buf,_dec->pp_frame_buf);
    }
    _dec->pp_frame_has_chroma=(_dec->pp_level>=OC_PP_LEVEL_DEBLOCKC);
  }
  /*If we're not processing chroma, copy the reference frame's chroma planes.*/
  if(_dec->pp_level<OC_PP_LEVEL_DEBLOCKC){
    memcpy(_dec->pp_frame_buf+1,
     _dec->state.ref_frame_bufs[_dec->state.ref_frame_idx[OC_FRAME_SELF]]+1,
     sizeof(_dec->pp_frame_buf[1])*2);
  }
  return 0;
}



typedef struct{
  int  ti[3][64];
  int  ebi[3][64];
  int  eob_runs[3][64];
  int  bounding_values[256];
  int *coded_fragis[3];
  int *uncoded_fragis[3];
  int  fragy0[3];
  int  fragy_end[3];
  int  ncoded_fragis[3];
  int  nuncoded_fragis[3];
  int  pred_last[3][3];
  int  mcu_nvfrags;
  int  loop_filter;
  int  pp_level;
}oc_dec_pipeline_state;



/*Initialize the main decoding pipeline.*/
static void oc_dec_pipeline_init(oc_dec_ctx *_dec,
 oc_dec_pipeline_state *_pipe){
  int *coded_fragi_end;
  int *uncoded_fragi_end;
  int  pli;
  /*If chroma is sub-sampled in the vertical direction, we have to decode two
     super block rows of Y' for each super block row of Cb and Cr.*/
  _pipe->mcu_nvfrags=4<<!(_dec->state.info.pixel_fmt&2);
  /*Initialize the token and extra bits indices for each plane and
     coefficient.*/
  memset(_pipe->ti[0],0,sizeof(_pipe->ti[0]));
  memset(_pipe->ebi[0],0,sizeof(_pipe->ebi[0]));
  for(pli=1;pli<3;pli++){
    memcpy(_pipe->ti[pli],_dec->ti0[pli-1],sizeof(_pipe->ti[0]));
    memcpy(_pipe->ebi[pli],_dec->ebi0[pli-1],sizeof(_pipe->ebi[0]));
  }
  /*Also copy over the initial the EOB run counts.*/
  memcpy(_pipe->eob_runs,_dec->eob_runs,sizeof(_pipe->eob_runs));
  /*Set up per-plane pointers to the coded and uncoded fragments lists.*/
  coded_fragi_end=_dec->state.coded_fragis;
  uncoded_fragi_end=_dec->state.uncoded_fragis;
  for(pli=0;pli<3;pli++){
    _pipe->coded_fragis[pli]=coded_fragi_end;
    _pipe->uncoded_fragis[pli]=uncoded_fragi_end;
    coded_fragi_end+=_dec->state.ncoded_fragis[pli];
    uncoded_fragi_end-=_dec->state.nuncoded_fragis[pli];
  }
  /*Set the previous DC predictor to 0 for all color planes and frame types.*/
  memset(_pipe->pred_last,0,sizeof(_pipe->pred_last));
  /*Initialize the bounding value array for the loop filter.*/
  _pipe->loop_filter=!oc_state_loop_filter_init(&_dec->state,
   _pipe->bounding_values);
  /*Initialize any buffers needed for post-processing.
    We also save the current post-processing level, to guard against the user
     changing it from a callback.*/
  if(!oc_dec_postprocess_init(_dec))_pipe->pp_level=_dec->pp_level;
  /*If we don't have enough information to post-process, disable it, regardless
     of the user-requested level.*/
  else{
    _pipe->pp_level=OC_PP_LEVEL_DISABLED;
    memcpy(_dec->pp_frame_buf,
     _dec->state.ref_frame_bufs[_dec->state.ref_frame_idx[OC_FRAME_SELF]],
     sizeof(_dec->pp_frame_buf[0])*3);
  }
}

/*Undo the DC prediction in a single plane of an MCU (one or two super block
   rows).
  As a side effect, the number of coded and uncoded fragments in this plane of
   the MCU is also computed.*/
static void oc_dec_dc_unpredict_mcu_plane(oc_dec_ctx *_dec,
 oc_dec_pipeline_state *_pipe,int _pli){
  /*Undo the DC prediction.*/
  oc_fragment_plane *fplane;
  oc_fragment       *frag;
  int               *pred_last;
  int                ncoded_fragis;
  int                fragx;
  int                fragy;
  int                fragy0;
  int                fragy_end;
  /*Compute the first and last fragment row of the current MCU for this
     plane.*/
  fplane=_dec->state.fplanes+_pli;
  fragy0=_pipe->fragy0[_pli];
  fragy_end=_pipe->fragy_end[_pli];
  frag=_dec->state.frags+fplane->froffset+(fragy0*fplane->nhfrags);
  ncoded_fragis=0;
  pred_last=_pipe->pred_last[_pli];
  for(fragy=fragy0;fragy<fragy_end;fragy++){
    for(fragx=0;fragx<fplane->nhfrags;fragx++,frag++){
      if(!frag->coded)continue;
      pred_last[OC_FRAME_FOR_MODE[frag->mbmode]]=frag->dc+=
       oc_frag_pred_dc(frag,fplane,fragx,fragy,pred_last);
      ncoded_fragis++;
    }
  }
  _pipe->ncoded_fragis[_pli]=ncoded_fragis;
  /*Also save the number of uncoded fragments so we know how many to copy.*/
  _pipe->nuncoded_fragis[_pli]=
   (fragy_end-fragy0)*fplane->nhfrags-ncoded_fragis;
}

/*Reconstructs all coded fragments in a single MCU (one or two super block
   rows).
  This requires that each coded fragment have a proper macro block mode and
   motion vector (if not in INTRA mode), and have it's DC value decoded, with
   the DC prediction process reversed, and the number of coded and uncoded
   fragments in this plane of the MCU be counted.
  The token lists for each color plane and coefficient should also be filled
   in, along with initial token offsets, extra bits offsets, and EOB run
   counts.*/
static void oc_dec_frags_recon_mcu_plane(oc_dec_ctx *_dec,
 oc_dec_pipeline_state *_pipe,int _pli){
  /*Decode the AC coefficients.*/
  int *ti;
  int *ebi;
  int *eob_runs;
  int *coded_fragi;
  int *coded_fragi_end;
  ti=_pipe->ti[_pli];
  ebi=_pipe->ebi[_pli];
  eob_runs=_pipe->eob_runs[_pli];
  coded_fragi_end=coded_fragi=_pipe->coded_fragis[_pli];
  coded_fragi_end+=_pipe->ncoded_fragis[_pli];
  for(;coded_fragi<coded_fragi_end;coded_fragi++){
    oc_fragment    *frag;
    oc_quant_table *iquants;
    /*This array is made one bigger than necessary so that an invalid zero
       run cannot cause a buffer overflow.
      The inverse zig-zag mapping sends all out of range indices to the last
       entry of this array, where they are ignored.*/
    ogg_int16_t    dct_coeffs[128];
    int            fragi;
    int            zzi;
    int            last_zzi;
    fragi=*coded_fragi;
    frag=_dec->state.frags+fragi;
    for(zzi=0;zzi<64;){
      int token;
      int eb;
      last_zzi=zzi;
      if(eob_runs[zzi]){
        eob_runs[zzi]--;
        break;
      }
      else{
        int ebflag;
        token=_dec->dct_tokens[zzi][ti[zzi]++];
        ebflag=OC_DCT_TOKEN_EXTRA_BITS[token]!=0;
        eb=_dec->extra_bits[zzi][ebi[zzi]]&-ebflag;
        ebi[zzi]+=ebflag;
        if(token<OC_NDCT_EOB_TOKEN_MAX){
          eob_runs[zzi]=-oc_dct_token_skip(token,eb);
        }
        else oc_dct_token_expand(token,eb,dct_coeffs,&zzi);
      }
    }
    /*TODO: zzi should be exactly 64 here.
      If it's not, we should report some kind of warning.*/
    zzi=OC_MINI(zzi,64);
    dct_coeffs[0]=(ogg_int16_t)frag->dc;
    iquants=_dec->state.dequant_tables[frag->mbmode!=OC_MODE_INTRA][_pli];
    /*last_zzi is always initialized.
      If your compiler thinks otherwise, it is dumb.*/
    oc_state_frag_recon(&_dec->state,frag,_pli,dct_coeffs,last_zzi,zzi,
     iquants[_dec->state.qis[0]][0],iquants[frag->qi]);
  }
  _pipe->coded_fragis[_pli]=coded_fragi;
  /*Right now the reconstructed MCU has only the coded blocks in it.*/
  /*TODO: We make the decision here to always copy the uncoded blocks into it
     from the reference frame.
    We could also copy the coded blocks back over the reference frame, if we
     wait for an additional MCU to be decoded, which might be faster if only a
     small number of blocks are coded.
    However, this introduces more latency, creating a larger cache footprint.
    It's unknown which decision is better, but this one results in simpler
     code, and the hard case (high bitrate, high resolution) is handled
     correctly.*/
  /*Copy the uncoded blocks from the previous reference frame.*/
  _pipe->uncoded_fragis[_pli]-=_pipe->nuncoded_fragis[_pli];
  oc_state_frag_copy(&_dec->state,_pipe->uncoded_fragis[_pli],
   _pipe->nuncoded_fragis[_pli],OC_FRAME_SELF,OC_FRAME_PREV,_pli);
}

/*Filter a horizontal block edge.*/
static void oc_filter_hedge(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,int _qstep,int _flimit,
 int *_variance0,int *_variance1){
  unsigned char       *rdst;
  const unsigned char *rsrc;
  unsigned char       *cdst;
  const unsigned char *csrc;
  int                  r[10];
  int                  sum0;
  int                  sum1;
  int                  bx;
  int                  by;
  rdst=_dst;
  rsrc=_src;
  for(bx=0;bx<8;bx++){
    cdst=rdst;
    csrc=rsrc;
    for(by=0;by<10;by++){
      r[by]=*csrc;
      csrc+=_src_ystride;
    }
    sum0=sum1=0;
    for(by=0;by<4;by++){
      sum0+=abs(r[by+1]-r[by]);
      sum1+=abs(r[by+5]-r[by+6]);
    }
    *_variance0+=OC_MINI(255,sum0);
    *_variance1+=OC_MINI(255,sum1);
    if(sum0<_flimit&&sum1<_flimit&&r[5]-r[4]<_qstep&&r[4]-r[5]<_qstep){
      *cdst=(unsigned char)(r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4>>3);
      cdst+=_dst_ystride;
      *cdst=(unsigned char)(r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4>>3);
      cdst+=_dst_ystride;
      for(by=0;by<4;by++){
        *cdst=(unsigned char)(r[by]+r[by+1]+r[by+2]+r[by+3]*2+
         r[by+4]+r[by+5]+r[by+6]+4>>3);
        cdst+=_dst_ystride;
      }
      *cdst=(unsigned char)(r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4>>3);
      cdst+=_dst_ystride;
      *cdst=(unsigned char)(r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4>>3);
    }
    else{
      for(by=1;by<=8;by++){
        *cdst=(unsigned char)r[by];
        cdst+=_dst_ystride;
      }
    }
    rdst++;
    rsrc++;
  }
}

/*Filter a vertical block edge.*/
static void oc_filter_vedge(unsigned char *_dst,int _dst_ystride,
 int _qstep,int _flimit,int *_variances){
  unsigned char       *rdst;
  const unsigned char *rsrc;
  unsigned char       *cdst;
  int                  r[10];
  int                  sum0;
  int                  sum1;
  int                  bx;
  int                  by;
  cdst=_dst;
  for(by=0;by<8;by++){
    rsrc=cdst-1;
    rdst=cdst;
    for(bx=0;bx<10;bx++)r[bx]=*rsrc++;
    sum0=sum1=0;
    for(bx=0;bx<4;bx++){
      sum0+=abs(r[bx+1]-r[bx]);
      sum1+=abs(r[bx+5]-r[bx+6]);
    }
    _variances[0]+=OC_MINI(255,sum0);
    _variances[1]+=OC_MINI(255,sum1);
    if(sum0<_flimit&&sum1<_flimit&&r[5]-r[4]<_qstep&&r[4]-r[5]<_qstep){
      *rdst++=(unsigned char)(r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4>>3);
      *rdst++=(unsigned char)(r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4>>3);
      for(bx=0;bx<4;bx++){
        *rdst++=(unsigned char)(r[bx]+r[bx+1]+r[bx+2]+r[bx+3]*2+
         r[bx+4]+r[bx+5]+r[bx+6]+4>>3);
      }
      *rdst++=(unsigned char)(r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4>>3);
      *rdst=(unsigned char)(r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4>>3);
    }
    else for(bx=1;bx<=8;bx++)*rdst++=(unsigned char)r[bx];
    cdst+=_dst_ystride;
  }
}

static void oc_dec_deblock_frag_rows(oc_dec_ctx *_dec,
 th_img_plane *_dst,th_img_plane *_src,int _pli,int _fragy0,
 int _fragy_end){
  oc_fragment_plane   *fplane;
  int                 *variance;
  unsigned char       *dc_qi;
  unsigned char       *dst;
  const unsigned char *src;
  int                  notstart;
  int                  notdone;
  int                  froffset;
  int                  flimit;
  int                  qstep;
  int                  y_end;
  int                  y;
  int                  x;
  _dst+=_pli;
  _src+=_pli;
  fplane=_dec->state.fplanes+_pli;
  froffset=fplane->froffset+_fragy0*fplane->nhfrags;
  variance=_dec->variances+froffset;
  dc_qi=_dec->dc_qis+froffset;
  notstart=_fragy0>0;
  notdone=_fragy_end<fplane->nvfrags;
  /*We want to clear an extra row of variances, except at the end.*/
  memset(variance+(fplane->nhfrags&-notstart),0,
   (_fragy_end+notdone-_fragy0-notstart)*fplane->nhfrags*sizeof(variance[0]));
  /*Except for the first time, we want to point to the middle of the row.*/
  y=(_fragy0<<3)+(notstart<<2);
  dst=_dst->data+y*_dst->stride;
  src=_src->data+y*_src->stride;
  for(;y<4;y++){
    memcpy(dst,src,_dst->width*sizeof(dst[0]));
    dst+=_dst->stride;
    src+=_src->stride;
  }
  /*We also want to skip the last row in the frame for this loop.*/
  y_end=_fragy_end-!notdone<<3;
  for(;y<y_end;y+=8){
    qstep=_dec->pp_dc_scale[*dc_qi];
    flimit=(qstep*3)>>2;
    oc_filter_hedge(dst,_dst->stride,src-_src->stride,_src->stride,
     qstep,flimit,variance,variance+fplane->nhfrags);
    variance++;
    dc_qi++;
    for(x=8;x<_dst->width;x+=8){
      qstep=_dec->pp_dc_scale[*dc_qi];
      flimit=(qstep*3)>>2;
      oc_filter_hedge(dst+x,_dst->stride,src+x-_src->stride,_src->stride,
       qstep,flimit,variance,variance+fplane->nhfrags);
      oc_filter_vedge(dst+x-(_dst->stride<<2)-4,_dst->stride,
       qstep,flimit,variance-1);
      variance++;
      dc_qi++;
    }
    dst+=_dst->stride<<3;
    src+=_src->stride<<3;
  }
  /*And finally, handle the last row in the frame, if it's in the range.*/
  if(!notdone){
    for(;y<_dst->height;y++){
      memcpy(dst,src,_dst->width*sizeof(dst[0]));
      dst+=_dst->stride;
      src+=_src->stride;
    }
    /*Filter the last row of vertical block edges.*/
    dc_qi++;
    for(x=8;x<_dst->width;x+=8){
      qstep=_dec->pp_dc_scale[*dc_qi++];
      flimit=(qstep*3)>>2;
      oc_filter_vedge(dst+x-(_dst->stride<<3)-4,_dst->stride,
       qstep,flimit,variance++);
    }
  }
}

static void oc_dering_block(unsigned char *_idata,int _ystride,int _b,
 int _dc_scale,int _sharp_mod,int _strong){
  static const int     OCDB_MOD_MAX[2]={24,32};
  static const int     OCDB_MOD_SHIFT[2]={1,0};
  const unsigned char *psrc;
  const unsigned char *src;
  const unsigned char *nsrc;
  unsigned char       *dst;
  int                  vmod[72];
  int                  hmod[72];
  int                  mod_hi;
  int                  by;
  int                  bx;
  mod_hi=OC_MINI(3*_dc_scale,OCDB_MOD_MAX[_strong]);
  dst=_idata;
  src=dst;
  psrc=src-(_ystride&-!(_b&4));
  for(by=0;by<9;by++){
    for(bx=0;bx<8;bx++){
      int mod;
      mod=32+_dc_scale-(abs(src[bx]-psrc[bx])<<OCDB_MOD_SHIFT[_strong]);
      vmod[(by<<3)+bx]=mod<-64?_sharp_mod:OC_CLAMPI(0,mod,mod_hi);
    }
    psrc=src;
    src+=_ystride&-(!(_b&8)|by<7);
  }
  nsrc=dst;
  psrc=dst-!(_b&1);
  for(bx=0;bx<9;bx++){
    src=nsrc;
    for(by=0;by<8;by++){
      int mod;
      mod=32+_dc_scale-(abs(*src-*psrc)<<OCDB_MOD_SHIFT[_strong]);
      hmod[(bx<<3)+by]=mod<-64?_sharp_mod:OC_CLAMPI(0,mod,mod_hi);
      psrc+=_ystride;
      src+=_ystride;
    }
    psrc=nsrc;
    nsrc+=!(_b&2)|bx<7;
  }
  src=dst;
  psrc=src-(_ystride&-!(_b&4));
  nsrc=src+_ystride;
  for(by=0;by<8;by++){
    int a;
    int b;
    int w;
    a=128;
    b=64;
    w=hmod[by];
    a-=w;
    b+=w**(src-!(_b&1));
    w=vmod[(by<<3)];
    a-=w;
    b+=w*psrc[0];
    w=vmod[(by+1<<3)];
    a-=w;
    b+=w*nsrc[0];
    w=hmod[(1<<3)+by];
    a-=w;
    b+=w*src[1];
    dst[0]=OC_CLAMP255(a*src[0]+b>>7);
    for(bx=1;bx<7;bx++){
      a=128;
      b=64;
      w=hmod[(bx<<3)+by];
      a-=w;
      b+=w*src[bx-1];
      w=vmod[(by<<3)+bx];
      a-=w;
      b+=w*psrc[bx];
      w=vmod[(by+1<<3)+bx];
      a-=w;
      b+=w*nsrc[bx];
      w=hmod[(bx+1<<3)+by];
      a-=w;
      b+=w*src[bx+1];
      dst[bx]=OC_CLAMP255(a*src[bx]+b>>7);
    }
    a=128;
    b=64;
    w=hmod[(7<<3)+by];
    a-=w;
    b+=w*src[6];
    w=vmod[(by<<3)+7];
    a-=w;
    b+=w*psrc[7];
    w=vmod[(by+1<<3)+7];
    a-=w;
    b+=w*nsrc[7];
    w=hmod[(8<<3)+by];
    a-=w;
    b+=w*src[7+!(_b&2)];
    dst[7]=OC_CLAMP255(a*src[7]+b>>7);
    dst+=_ystride;
    psrc=src;
    src=nsrc;
    nsrc+=_ystride&-(!(_b&8)|by<6);
  }
}

#define OC_DERING_THRESH1 (384)
#define OC_DERING_THRESH2 (4*OC_DERING_THRESH1)
#define OC_DERING_THRESH3 (5*OC_DERING_THRESH1)
#define OC_DERING_THRESH4 (10*OC_DERING_THRESH1)

static void oc_dec_dering_frag_rows(oc_dec_ctx *_dec,th_img_plane *_img,
 int _pli,int _fragy0,int _fragy_end){
  th_img_plane      *iplane;
  oc_fragment_plane *fplane;
  oc_fragment       *frag;
  int               *variance;
  unsigned char     *idata;
  int                sthresh;
  int                strong;
  int                froffset;
  int                y_end;
  int                y;
  int                x;
  iplane=_img+_pli;
  fplane=_dec->state.fplanes+_pli;
  froffset=fplane->froffset+_fragy0*fplane->nhfrags;
  variance=_dec->variances+froffset;
  frag=_dec->state.frags+froffset;
  strong=_dec->pp_level>=(_pli?OC_PP_LEVEL_SDERINGC:OC_PP_LEVEL_SDERINGY);
  sthresh=_pli?OC_DERING_THRESH4:OC_DERING_THRESH3;
  y=_fragy0<<3;
  idata=iplane->data+y*iplane->stride;
  y_end=_fragy_end<<3;
  for(;y<y_end;y+=8){
    for(x=0;x<iplane->width;x+=8){
      int b;
      int qi;
      int var;
      qi=frag->qi;
      var=*variance;
      b=(x<=0)|(x+8>=iplane->width)<<1|(y<=0)<<2|(y+8>=iplane->height)<<3;
      if(strong&&var>sthresh){
        oc_dering_block(idata+x,iplane->stride,b,
         _dec->pp_dc_scale[qi],_dec->pp_sharp_mod[qi],1);
        if(_pli||!(b&1)&&*(variance-1)>OC_DERING_THRESH4||
         !(b&2)&&variance[1]>OC_DERING_THRESH4||
         !(b&4)&&*(variance-fplane->nvfrags)>OC_DERING_THRESH4||
         !(b&8)&&variance[fplane->nvfrags]>OC_DERING_THRESH4){
          oc_dering_block(idata+x,iplane->stride,b,
           _dec->pp_dc_scale[qi],_dec->pp_sharp_mod[qi],1);
          oc_dering_block(idata+x,iplane->stride,b,
           _dec->pp_dc_scale[qi],_dec->pp_sharp_mod[qi],1);
        }
      }
      else if(var>OC_DERING_THRESH2){
        oc_dering_block(idata+x,iplane->stride,b,
         _dec->pp_dc_scale[qi],_dec->pp_sharp_mod[qi],1);
      }
      else if(var>OC_DERING_THRESH1){
        oc_dering_block(idata+x,iplane->stride,b,
         _dec->pp_dc_scale[qi],_dec->pp_sharp_mod[qi],0);
      }
      frag++;
      variance++;
    }
    idata+=iplane->stride<<3;
  }
}



th_dec_ctx *th_decode_alloc(const th_info *_info,
 const th_setup_info *_setup){
  oc_dec_ctx *dec;
  if(_info==NULL||_setup==NULL)return NULL;
  dec=_ogg_malloc(sizeof(*dec));
  if(oc_dec_init(dec,_info,_setup)<0){
    _ogg_free(dec);
    return NULL;
  }
  dec->state.curframe_num=0;
  return dec;
}

void th_decode_free(th_dec_ctx *_dec){
  if(_dec!=NULL){
    oc_dec_clear(_dec);
    _ogg_free(_dec);
  }
}

int th_decode_ctl(th_dec_ctx *_dec,int _req,void *_buf,
 size_t _buf_sz){
  switch(_req){
    case TH_DECCTL_GET_PPLEVEL_MAX:{
      if(_dec==NULL||_buf==NULL)return TH_EFAULT;
      if(_buf_sz!=sizeof(int))return TH_EINVAL;
      (*(int *)_buf)=OC_PP_LEVEL_MAX;
      return 0;
    }break;
    case TH_DECCTL_SET_PPLEVEL:{
      int pp_level;
      if(_dec==NULL||_buf==NULL)return TH_EFAULT;
      if(_buf_sz!=sizeof(int))return TH_EINVAL;
      pp_level=*(int *)_buf;
      if(pp_level<0||pp_level>OC_PP_LEVEL_MAX)return TH_EINVAL;
      _dec->pp_level=pp_level;
      return 0;
    }break;
    case TH_DECCTL_SET_GRANPOS:{
      ogg_int64_t granpos;
      if(_dec==NULL||_buf==NULL)return TH_EFAULT;
      if(_buf_sz!=sizeof(ogg_int64_t))return TH_EINVAL;
      granpos=*(ogg_int64_t *)_buf;
      if(granpos<0)return TH_EINVAL;
      _dec->state.granpos=granpos;
      _dec->state.keyframe_num=
       granpos>>_dec->state.info.keyframe_granule_shift;
      _dec->state.curframe_num=_dec->state.keyframe_num+
       (granpos&(1<<_dec->state.info.keyframe_granule_shift)-1);
      return 0;
    }break;
    case TH_DECCTL_SET_STRIPE_CB:{
      th_stripe_callback *cb;
      if(_dec==NULL||_buf==NULL)return TH_EFAULT;
      if(_buf_sz!=sizeof(th_stripe_callback))return TH_EINVAL;
      cb=(th_stripe_callback *)_buf;
      _dec->stripe_cb.ctx=cb->ctx;
      _dec->stripe_cb.stripe_decoded=cb->stripe_decoded;
      return 0;
    }break;
    default:return TH_EIMPL;
  }
}

int th_decode_packetin(th_dec_ctx *_dec,const ogg_packet *_op,
 ogg_int64_t *_granpos){
  int ret;
  if(_dec==NULL||_op==NULL)return TH_EFAULT;
  /*A completely empty packet indicates a dropped frame and is treated exactly
     like an inter frame with no coded blocks.
    Only proceed if we have a non-empty packet.*/
  if(_op->bytes!=0){
    oc_dec_pipeline_state pipe;
    th_ycbcr_buffer       stripe_buf;
    int                   stripe_fragy;
    int                   refi;
    int                   pli;
    int                   notstart;
    int                   notdone;
    theorapackB_readinit(&_dec->opb,_op->packet,_op->bytes);
    ret=oc_dec_frame_header_unpack(_dec);
    if(ret<0)return ret;
    /*Select a free buffer to use for the reconstructed version of this
       frame.*/
    if(_dec->state.frame_type!=OC_INTRA_FRAME&&
     (_dec->state.ref_frame_idx[OC_FRAME_GOLD]<0||
     _dec->state.ref_frame_idx[OC_FRAME_PREV]<0)){
      th_info *info;
      size_t       yplane_sz;
      size_t       cplane_sz;
      int          yhstride;
      int          yvstride;
      int          chstride;
      int          cvstride;
      /*We're decoding an INTER frame, but have no initialized reference
         buffers (i.e., decoding did not start on a key frame).
        We initialize them to a solid gray here.*/
      _dec->state.ref_frame_idx[OC_FRAME_GOLD]=0;
      _dec->state.ref_frame_idx[OC_FRAME_PREV]=0;
      _dec->state.ref_frame_idx[OC_FRAME_SELF]=refi=1;
      info=&_dec->state.info;
      yhstride=info->frame_width+2*OC_UMV_PADDING;
      yvstride=info->frame_height+2*OC_UMV_PADDING;
      chstride=yhstride>>!(info->pixel_fmt&1);
      cvstride=yvstride>>!(info->pixel_fmt&2);
      yplane_sz=(size_t)yhstride*yvstride;
      cplane_sz=(size_t)chstride*cvstride;
      memset(_dec->state.ref_frame_data,0x80,yplane_sz+2*cplane_sz);
    }
    else{
      for(refi=0;refi==_dec->state.ref_frame_idx[OC_FRAME_GOLD]||
       refi==_dec->state.ref_frame_idx[OC_FRAME_PREV];refi++);
      _dec->state.ref_frame_idx[OC_FRAME_SELF]=refi;
    }
    if(_dec->state.frame_type==OC_INTRA_FRAME){
      oc_dec_mark_all_intra(_dec);
      _dec->state.keyframe_num=_dec->state.curframe_num;
    }else{
      oc_dec_coded_flags_unpack(_dec);
      oc_dec_mb_modes_unpack(_dec);
      oc_dec_mv_unpack_and_frag_modes_fill(_dec);
    }
    oc_dec_block_qis_unpack(_dec);
    oc_dec_residual_tokens_unpack(_dec);
    /*Update granule position.
      This must be done before the striped decode callbacks so that the
       application knows what to do with the frame data.*/
    _dec->state.granpos=
     (_dec->state.keyframe_num<<_dec->state.info.keyframe_granule_shift)+
     (_dec->state.curframe_num-_dec->state.keyframe_num);
    _dec->state.curframe_num++;
    if(_granpos!=NULL)*_granpos=_dec->state.granpos;
    /*All of the rest of the operations -- DC prediction reversal,
       reconstructing coded fragments, copying uncoded fragments, loop
       filtering, extending borders, and out-of-loop post-processing -- should
       be pipelined.
      I.e., DC prediction reversal, reconstruction, and uncoded fragment
       copying are done for one or two super block rows, then loop filtering is
       run as far as it can, then bordering copying, then post-processing.
      For 4:2:0 video a Minimum Codable Unit or MCU contains two luma super
       block rows, and one chroma.
      Otherwise, an MCU consists of one super block row from each plane.
      Inside each MCU, we perform all of the steps on one color plane before
       moving on to the next.
      After reconstruction, the additional filtering stages introduce a delay
       since they need some pixels from the next fragment row.
      Thus the actual number of decoded rows available is slightly smaller for
       the first MCU, and slightly larger for the last.

      This entire process allows us to operate on the data while it is still in
       cache, resulting in big performance improvements.
      An application callback allows further application processing (blitting
       to video memory, color conversion, etc.) to also use the data while it's
       in cache.*/
    oc_dec_pipeline_init(_dec,&pipe);
    oc_ycbcr_buffer_flip(stripe_buf,_dec->pp_frame_buf);
    notstart=0;
    notdone=1;
    for(stripe_fragy=notstart=0;notdone;stripe_fragy+=pipe.mcu_nvfrags){
      int avail_fragy0;
      int avail_fragy_end;
      avail_fragy0=avail_fragy_end=_dec->state.fplanes[0].nvfrags;
      notdone=stripe_fragy+pipe.mcu_nvfrags<avail_fragy_end;
      for(pli=0;pli<3;pli++){
        oc_fragment_plane *fplane;
        int                frag_shift;
        int                pp_offset;
        int                sdelay;
        int                edelay;
        fplane=_dec->state.fplanes+pli;
        /*Compute the first and last fragment row of the current MCU for this
           plane.*/
        frag_shift=pli!=0&&!(_dec->state.info.pixel_fmt&2);
        pipe.fragy0[pli]=stripe_fragy>>frag_shift;
        pipe.fragy_end[pli]=OC_MINI(fplane->nvfrags,
         pipe.fragy0[pli]+(pipe.mcu_nvfrags>>frag_shift));
        oc_dec_dc_unpredict_mcu_plane(_dec,&pipe,pli);
        oc_dec_frags_recon_mcu_plane(_dec,&pipe,pli);
        sdelay=edelay=0;
        if(pipe.loop_filter){
          sdelay+=notstart;
          edelay+=notdone;
          oc_state_loop_filter_frag_rows(&_dec->state,pipe.bounding_values,
           refi,pli,pipe.fragy0[pli]-sdelay,pipe.fragy_end[pli]-edelay);
        }
        /*To fill the borders, we have an additional two pixel delay, since a
           fragment in the next row could filter its top edge, using two pixels
           from a fragment in this row.
          But there's no reason to delay a full fragment between the two.*/
        oc_state_borders_fill_rows(&_dec->state,refi,pli,
         (pipe.fragy0[pli]-sdelay<<3)-(sdelay<<1),
         (pipe.fragy_end[pli]-edelay<<3)-(edelay<<1));
        /*Out-of-loop post-processing.*/
        pp_offset=3*(pli!=0);
        if(pipe.pp_level>=OC_PP_LEVEL_DEBLOCKY+pp_offset){
          /*Perform de-blocking in one plane.*/
          sdelay+=notstart;
          edelay+=notdone;
          oc_dec_deblock_frag_rows(_dec,_dec->pp_frame_buf,
           _dec->state.ref_frame_bufs[refi],pli,
           pipe.fragy0[pli]-sdelay,pipe.fragy_end[pli]-edelay);
          if(pipe.pp_level>=OC_PP_LEVEL_DERINGY+pp_offset){
            /*Perform de-ringing in one plane.*/
            sdelay+=notstart;
            edelay+=notdone;
            oc_dec_dering_frag_rows(_dec,_dec->pp_frame_buf,pli,
             pipe.fragy0[pli]-sdelay,pipe.fragy_end[pli]-edelay);
          }
        }
        /*If no post-processing is done, we still need to delay a row for the
           loop filter, thanks to the strange filtering order VP3 chose.*/
        else if(pipe.loop_filter){
          sdelay+=notstart;
          edelay+=notdone;
        }
        /*Compute the intersection of the available rows in all planes.
          If chroma is sub-sampled, the effect of each of its delays is
           doubled, but luma might have more post-processing filters enabled
           than chroma, so we don't know up front which one is the limiting
           factor.*/
        avail_fragy0=OC_MINI(avail_fragy0,pipe.fragy0[pli]-sdelay<<frag_shift);
        avail_fragy_end=OC_MINI(avail_fragy_end,
         pipe.fragy_end[pli]-edelay<<frag_shift);
      }
      if(_dec->stripe_cb.stripe_decoded!=NULL){
        /*Make the callback, ensuring we flip the sense of the "start" and
           "end" of the available region upside down.*/
        (*_dec->stripe_cb.stripe_decoded)(_dec->stripe_cb.ctx,stripe_buf,
         _dec->state.fplanes[0].nvfrags-avail_fragy_end,
         _dec->state.fplanes[0].nvfrags-avail_fragy0);
      }
      notstart=1;
    }
    /*Finish filling in the reference frame borders.*/
    for(pli=0;pli<3;pli++)oc_state_borders_fill_caps(&_dec->state,refi,pli);
    /*Update the reference frame indices.*/
    if(_dec->state.frame_type==OC_INTRA_FRAME){
      /*The new frame becomes both the previous and gold reference frames.*/
      _dec->state.ref_frame_idx[OC_FRAME_GOLD]=
       _dec->state.ref_frame_idx[OC_FRAME_PREV]=
       _dec->state.ref_frame_idx[OC_FRAME_SELF];
    }
    else{
      /*Otherwise, just replace the previous reference frame.*/
      _dec->state.ref_frame_idx[OC_FRAME_PREV]=
       _dec->state.ref_frame_idx[OC_FRAME_SELF];
    }
#if defined(OC_DUMP_IMAGES)
    /*Don't dump images for dropped frames.*/
    oc_state_dump_frame(&_dec->state,OC_FRAME_SELF,"dec");
#endif
    return 0;
  }
  else{
    /*Just update the granule position and return.*/
    _dec->state.granpos=
     (_dec->state.keyframe_num<<_dec->state.info.keyframe_granule_shift)+
     (_dec->state.curframe_num-_dec->state.keyframe_num);
    _dec->state.curframe_num++;
    if(_granpos!=NULL)*_granpos=_dec->state.granpos;
    return TH_DUPFRAME;
  }
}

int th_decode_ycbcr_out(th_dec_ctx *_dec,th_ycbcr_buffer _ycbcr){
  oc_ycbcr_buffer_flip(_ycbcr,_dec->pp_frame_buf);
  return 0;
}
