/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunks.h           copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : Chunk structures (definition)                              * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of known chunk structures                       * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/04/2000 - G.Juyn                                * */
/* *             - put in some extra comments                               * */
/* *             0.5.1 - 05/06/2000 - G.Juyn                                * */
/* *             - fixed layout for sBIT, PPLT                              * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed write callback definition                        * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - fixed layout for PPLT again (missed deltatype ?!?)       * */
/* *                                                                        * */
/* *             0.5.2 - 05/31/2000 - G.Juyn                                * */
/* *             - removed useless definition (contributed by Tim Rowley)   * */
/* *             0.5.2 - 06/03/2000 - G.Juyn                                * */
/* *             - fixed makeup for Linux gcc compile                       * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/10/2000 - G.Juyn                                * */
/* *             - fixed DEFI behavior                                      * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added JDAA chunk                                         * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_chunks_h_
#define _libmng_chunks_h_

/* ************************************************************************** */

#ifdef MNG_SWAP_ENDIAN
#define PNG_SIG 0x474e5089L
#define JNG_SIG 0x474e4a8bL
#define MNG_SIG 0x474e4d8aL
#define POST_SIG 0x0a1a0a0dL
#else
#define PNG_SIG 0x89504e47L
#define JNG_SIG 0x8b4a4e47L
#define MNG_SIG 0x8a4d4e47L
#define POST_SIG 0x0d0a1a0aL
#endif

/* ************************************************************************** */

typedef mng_retcode (*mng_createchunk)  (mng_datap   pData,
                                         mng_chunkp  pHeader,
                                         mng_chunkp* ppChunk);

typedef mng_retcode (*mng_cleanupchunk) (mng_datap   pData,
                                         mng_chunkp  pHeader);

typedef mng_retcode (*mng_readchunk)    (mng_datap   pData,
                                         mng_chunkp  pHeader,
                                         mng_uint32  iRawlen,
                                         mng_uint8p  pRawdata,
                                         mng_chunkp* pChunk);

typedef mng_retcode (*mng_writechunk)   (mng_datap   pData,
                                         mng_chunkp  pChunk);

/* ************************************************************************** */

typedef struct {                       /* generic header */
           mng_chunkid       iChunkname;
           mng_createchunk   fCreate;
           mng_cleanupchunk  fCleanup;
           mng_readchunk     fRead;
           mng_writechunk    fWrite;
           mng_chunkp        pNext;    /* for double-linked list */
           mng_chunkp        pPrev;
        } mng_chunk_header;
typedef mng_chunk_header * mng_chunk_headerp;

/* ************************************************************************** */

typedef struct {                       /* IHDR */
           mng_chunk_header  sHeader;
           mng_uint32        iWidth;
           mng_uint32        iHeight;
           mng_uint8         iBitdepth;
           mng_uint8         iColortype;
           mng_uint8         iCompression;
           mng_uint8         iFilter;
           mng_uint8         iInterlace;
        } mng_ihdr;
typedef mng_ihdr * mng_ihdrp;

/* ************************************************************************** */

typedef struct {                       /* PLTE */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iEntrycount;
           mng_rgbpaltab     aEntries;
        } mng_plte;
typedef mng_plte * mng_pltep;

/* ************************************************************************** */

typedef struct {                       /* IDAT */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iDatasize;
           mng_ptr           pData;
        } mng_idat;
typedef mng_idat * mng_idatp;

/* ************************************************************************** */

typedef struct {                       /* IEND */
           mng_chunk_header  sHeader;
        } mng_iend;
typedef mng_iend * mng_iendp;

/* ************************************************************************** */

typedef struct {                       /* tRNS */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_bool          bGlobal;
           mng_uint8         iType;    /* colortype (0,2,3) */
           mng_uint32        iCount;
           mng_uint8arr      aEntries;
           mng_uint16        iGray;
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
           mng_uint32        iRawlen;
           mng_uint8arr      aRawdata;
        } mng_trns;
typedef mng_trns * mng_trnsp;

/* ************************************************************************** */

typedef struct {                       /* gAMA */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iGamma;
        } mng_gama;
typedef mng_gama * mng_gamap;

/* ************************************************************************** */

typedef struct {                       /* cHRM */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iWhitepointx;
           mng_uint32        iWhitepointy;
           mng_uint32        iRedx;
           mng_uint32        iRedy;
           mng_uint32        iGreenx;
           mng_uint32        iGreeny;
           mng_uint32        iBluex;
           mng_uint32        iBluey;
        } mng_chrm;
typedef mng_chrm * mng_chrmp;

/* ************************************************************************** */

typedef struct {                       /* sRGB */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint8         iRenderingintent;
        } mng_srgb;
typedef mng_srgb * mng_srgbp;

/* ************************************************************************** */

typedef struct {                       /* iCCP */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iNamesize;
           mng_pchar         zName;
           mng_uint8         iCompression;
           mng_uint32        iProfilesize;
           mng_ptr           pProfile;
        } mng_iccp;
typedef mng_iccp * mng_iccpp;

/* ************************************************************************** */

typedef struct {                       /* tEXt */
           mng_chunk_header  sHeader;
           mng_uint32        iKeywordsize;
           mng_pchar         zKeyword;
           mng_uint32        iTextsize;
           mng_pchar         zText;
        } mng_text;
typedef mng_text * mng_textp;

/* ************************************************************************** */

typedef struct {                       /* zTXt */
           mng_chunk_header  sHeader;
           mng_uint32        iKeywordsize;
           mng_pchar         zKeyword;
           mng_uint8         iCompression;
           mng_uint32        iTextsize;
           mng_pchar         zText;
        } mng_ztxt;
typedef mng_ztxt * mng_ztxtp;

/* ************************************************************************** */

typedef struct {                       /* iTXt */
           mng_chunk_header  sHeader;
           mng_uint32        iKeywordsize;
           mng_pchar         zKeyword;
           mng_uint8         iCompressionflag;
           mng_uint8         iCompressionmethod;
           mng_uint32        iLanguagesize;
           mng_pchar         zLanguage;
           mng_uint32        iTranslationsize;
           mng_pchar         zTranslation;
           mng_uint32        iTextsize;
           mng_pchar         zText;
        } mng_itxt;
typedef mng_itxt * mng_itxtp;

/* ************************************************************************** */

typedef struct {                       /* bKGD */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint8         iType;    /* 3=indexed, 0=gray, 2=rgb */
           mng_uint8         iIndex;
           mng_uint16        iGray;
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
        } mng_bkgd;
typedef mng_bkgd * mng_bkgdp;

/* ************************************************************************** */

typedef struct {                       /* pHYs */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iSizex;
           mng_uint32        iSizey;
           mng_uint8         iUnit;
        } mng_phys;
typedef mng_phys * mng_physp;

/* ************************************************************************** */

typedef struct {                       /* sBIT */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint8         iType;    /* colortype (0,2,3,4,6,10,12,14,16) */
           mng_uint8arr4     aBits;
        } mng_sbit;
typedef mng_sbit * mng_sbitp;

/* ************************************************************************** */

typedef struct {                       /* sPLT */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint32        iNamesize;
           mng_pchar         zName;
           mng_uint8         iSampledepth;
           mng_uint32        iEntrycount;
           mng_ptr           pEntries;
        } mng_splt;
typedef mng_splt * mng_spltp;

/* ************************************************************************** */

typedef struct {                       /* hIST */
           mng_chunk_header  sHeader;
           mng_uint32        iEntrycount;
           mng_uint16arr     aEntries;
        } mng_hist;
typedef mng_hist * mng_histp;

/* ************************************************************************** */

typedef struct {                       /* tIME */
           mng_chunk_header  sHeader;
           mng_uint16        iYear;
           mng_uint8         iMonth;
           mng_uint8         iDay;
           mng_uint8         iHour;
           mng_uint8         iMinute;
           mng_uint8         iSecond;
        } mng_time;
typedef mng_time * mng_timep;

/* ************************************************************************** */

typedef struct {                       /* MHDR */
           mng_chunk_header  sHeader;
           mng_uint32        iWidth;
           mng_uint32        iHeight;
           mng_uint32        iTicks;
           mng_uint32        iLayercount;
           mng_uint32        iFramecount;
           mng_uint32        iPlaytime;
           mng_uint32        iSimplicity;
        } mng_mhdr;
typedef mng_mhdr * mng_mhdrp;

/* ************************************************************************** */

typedef struct {                       /* MEND */
           mng_chunk_header  sHeader;
        } mng_mend;
typedef mng_mend * mng_mendp;

/* ************************************************************************** */

typedef struct {                       /* LOOP */
           mng_chunk_header  sHeader;
           mng_uint8         iLevel;
           mng_uint32        iRepeat;
           mng_uint8         iTermination;
           mng_uint32        iItermin;
           mng_uint32        iItermax;
           mng_uint32        iCount;
           mng_uint32p       pSignals;
        } mng_loop;
typedef mng_loop * mng_loopp;

/* ************************************************************************** */

typedef struct {                       /* ENDL */
           mng_chunk_header  sHeader;
           mng_uint8         iLevel;
        } mng_endl;
typedef mng_endl * mng_endlp;

/* ************************************************************************** */

typedef struct {                       /* DEFI */
           mng_chunk_header  sHeader;
           mng_uint16        iObjectid;
           mng_bool          bHasdonotshow;
           mng_uint8         iDonotshow;
           mng_bool          bHasconcrete;
           mng_uint8         iConcrete;
           mng_bool          bHasloca;
           mng_int32         iXlocation;
           mng_int32         iYlocation;
           mng_bool          bHasclip;
           mng_int32         iLeftcb;
           mng_int32         iRightcb;
           mng_int32         iTopcb;
           mng_int32         iBottomcb;
        } mng_defi;
typedef mng_defi * mng_defip;

/* ************************************************************************** */

typedef struct {                       /* BASI */
           mng_chunk_header  sHeader;
           mng_uint32        iWidth;
           mng_uint32        iHeight;
           mng_uint8         iBitdepth;
           mng_uint8         iColortype;
           mng_uint8         iCompression;
           mng_uint8         iFilter;
           mng_uint8         iInterlace;
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
           mng_uint16        iAlpha;
           mng_uint8         iViewable;
        } mng_basi;
typedef mng_basi * mng_basip;

/* ************************************************************************** */

typedef struct {                       /* CLON */
           mng_chunk_header  sHeader;
           mng_uint16        iSourceid;
           mng_uint16        iCloneid;
           mng_uint8         iClonetype;
           mng_uint8         iDonotshow;
           mng_uint8         iConcrete;
           mng_bool          bHasloca;
           mng_uint8         iLocationtype;
           mng_int32         iLocationx;
           mng_int32         iLocationy;
        } mng_clon;
typedef mng_clon * mng_clonp;

/* ************************************************************************** */

typedef struct {                       /* PAST source */
           mng_uint16        iSourceid;
           mng_uint8         iComposition;
           mng_uint8         iOrientation;
           mng_uint8         iOffsettype;
           mng_int32         iOffsetx;
           mng_int32         iOffsety;
           mng_uint8         iBoundarytype;
           mng_int32         iBoundaryl;
           mng_int32         iBoundaryr;
           mng_int32         iBoundaryt;
           mng_int32         iBoundaryb;
        } mng_past_source;
typedef mng_past_source * mng_past_sourcep;

typedef struct {                       /* PAST */
           mng_chunk_header  sHeader;
           mng_uint16        iDestid;
           mng_uint8         iTargettype;
           mng_int32         iTargetx;
           mng_int32         iTargety;
           mng_uint32        iCount;
           mng_past_sourcep  pSources;
        } mng_past;
typedef mng_past * mng_pastp;

/* ************************************************************************** */

typedef struct {                       /* DISC */
           mng_chunk_header  sHeader;
           mng_uint32        iCount;
           mng_uint16p       pObjectids;
        } mng_disc;
typedef mng_disc * mng_discp;

/* ************************************************************************** */

typedef struct {                       /* BACK */
           mng_chunk_header  sHeader;
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
           mng_uint8         iMandatory;
           mng_uint16        iImageid;
           mng_uint8         iTile;
        } mng_back;
typedef mng_back * mng_backp;

/* ************************************************************************** */

typedef struct {                       /* FRAM */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint8         iMode;
           mng_uint32        iNamesize;
           mng_pchar         zName;
           mng_uint8         iChangedelay;
           mng_uint8         iChangetimeout;
           mng_uint8         iChangeclipping;
           mng_uint8         iChangesyncid;
           mng_uint32        iDelay;
           mng_uint32        iTimeout;
           mng_uint8         iBoundarytype;
           mng_int32         iBoundaryl;
           mng_int32         iBoundaryr;
           mng_int32         iBoundaryt;
           mng_int32         iBoundaryb;
           mng_uint32        iCount;
           mng_uint32p       pSyncids;
        } mng_fram;
typedef mng_fram * mng_framp;

/* ************************************************************************** */

typedef struct {                       /* MOVE */
           mng_chunk_header  sHeader;
           mng_uint16        iFirstid;
           mng_uint16        iLastid;
           mng_uint8         iMovetype;
           mng_int32         iMovex;
           mng_int32         iMovey;
        } mng_move;
typedef mng_move * mng_movep;

/* ************************************************************************** */

typedef struct {                       /* CLIP */
           mng_chunk_header  sHeader;
           mng_uint16        iFirstid;
           mng_uint16        iLastid;
           mng_uint8         iCliptype;
           mng_int32         iClipl;
           mng_int32         iClipr;
           mng_int32         iClipt;
           mng_int32         iClipb;
        } mng_clip;
typedef mng_clip * mng_clipp;

/* ************************************************************************** */

typedef struct {                       /* SHOW */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint16        iFirstid;
           mng_uint16        iLastid;
           mng_uint8         iMode;
        } mng_show;
typedef mng_show * mng_showp;

/* ************************************************************************** */

typedef struct {                       /* TERM */
           mng_chunk_header  sHeader;
           mng_uint8         iTermaction;
           mng_uint8         iIteraction;
           mng_uint32        iDelay;
           mng_uint32        iItermax;
        } mng_term;
typedef mng_term * mng_termp;

/* ************************************************************************** */

typedef struct {                       /* SAVE entry */
           mng_uint8         iEntrytype;
           mng_uint32arr2    iOffset;            /* 0=MSI, 1=LSI */
           mng_uint32arr2    iStarttime;         /* 0=MSI, 1=LSI */
           mng_uint32        iLayernr;
           mng_uint32        iFramenr;
           mng_uint32        iNamesize;
           mng_pchar         zName;
        } mng_save_entry;
typedef mng_save_entry * mng_save_entryp;

typedef struct {                       /* SAVE */
           mng_chunk_header  sHeader;
           mng_bool          bEmpty;
           mng_uint8         iOffsettype;
           mng_uint32        iCount;
           mng_save_entryp   pEntries;
        } mng_save;
typedef mng_save * mng_savep;

/* ************************************************************************** */

typedef struct {                       /* SEEK */
           mng_chunk_header  sHeader;
           mng_uint32        iNamesize;
           mng_pchar         zName;
        } mng_seek;
typedef mng_seek * mng_seekp;

/* ************************************************************************** */

typedef struct {                       /* eXPI */
           mng_chunk_header  sHeader;
           mng_uint16        iSnapshotid;
           mng_uint32        iNamesize;
           mng_pchar         zName;
        } mng_expi;
typedef mng_expi * mng_expip;

/* ************************************************************************** */

typedef struct {                       /* fPRI */
           mng_chunk_header  sHeader;
           mng_uint8         iDeltatype;
           mng_uint8         iPriority;
        } mng_fpri;
typedef mng_fpri * mng_fprip;

/* ************************************************************************** */

typedef struct {                       /* nEED */
           mng_chunk_header  sHeader;
           mng_uint32        iKeywordssize;
           mng_pchar         zKeywords;
        } mng_need;
typedef mng_need * mng_needp;

/* ************************************************************************** */

typedef mng_phys mng_phyg;             /* pHYg */
typedef mng_phyg * mng_phygp;

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG

typedef struct {                       /* JHDR */
           mng_chunk_header  sHeader;
           mng_uint32        iWidth;
           mng_uint32        iHeight;
           mng_uint8         iColortype;
           mng_uint8         iImagesampledepth;
           mng_uint8         iImagecompression;
           mng_uint8         iImageinterlace;
           mng_uint8         iAlphasampledepth;
           mng_uint8         iAlphacompression;
           mng_uint8         iAlphafilter;
           mng_uint8         iAlphainterlace;
        } mng_jhdr;
typedef mng_jhdr * mng_jhdrp;

/* ************************************************************************** */

typedef mng_idat mng_jdaa;             /* JDAA */
typedef mng_jdaa * mng_jdaap;

/* ************************************************************************** */

typedef mng_idat mng_jdat;             /* JDAT */
typedef mng_jdat * mng_jdatp;

/* ************************************************************************** */

typedef struct {                       /* JSEP */
           mng_chunk_header  sHeader;
        } mng_jsep;
typedef mng_jsep * mng_jsepp;

#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

typedef struct {                       /* DHDR */
           mng_chunk_header  sHeader;
           mng_uint16        iObjectid;
           mng_uint8         iImagetype;
           mng_uint8         iDeltatype;
           mng_uint32        iBlockwidth;
           mng_uint32        iBlockheight;
           mng_uint32        iBlockx;
           mng_uint32        iBlocky;
        } mng_dhdr;
typedef mng_dhdr * mng_dhdrp;

/* ************************************************************************** */

typedef struct {                       /* PROM */
           mng_chunk_header  sHeader;
           mng_uint8         iColortype;
           mng_uint8         iSampledepth;
           mng_uint8         iFilltype;
        } mng_prom;
typedef mng_prom * mng_promp;

/* ************************************************************************** */

typedef struct {                       /* IPNG */
           mng_chunk_header  sHeader;
        } mng_ipng;
typedef mng_ipng *mng_ipngp;

/* ************************************************************************** */

typedef struct {                       /* PPLT entry */
           mng_uint8         iRed;
           mng_uint8         iGreen;
           mng_uint8         iBlue;
           mng_uint8         iAlpha;
           mng_bool          bUsed;
        } mng_pplt_entry;
typedef mng_pplt_entry * mng_pplt_entryp;

typedef struct {                       /* PPLT */
           mng_chunk_header  sHeader;
           mng_uint8         iDeltatype;
           mng_uint32        iCount;
           mng_pplt_entry    aEntries [256];
        } mng_pplt;
typedef mng_pplt * mng_ppltp;

/* ************************************************************************** */

typedef struct {                       /* IJNG */
           mng_chunk_header  sHeader;
        } mng_ijng;
typedef mng_ijng *mng_ijngp;

/* ************************************************************************** */

typedef struct {                       /* DROP */
           mng_chunk_header  sHeader;
           mng_uint32        iCount;
           mng_chunkidp      pChunknames;
        } mng_drop;
typedef mng_drop * mng_dropp;

/* ************************************************************************** */

typedef struct {                       /* DBYK */
           mng_chunk_header  sHeader;
           mng_chunkid       iChunkname;
           mng_uint8         iPolarity;
           mng_uint32        iKeywordssize;
           mng_pchar         zKeywords;
        } mng_dbyk;
typedef mng_dbyk * mng_dbykp;

/* ************************************************************************** */

typedef struct {                       /* ORDR entry */
           mng_chunkid       iChunkname;
           mng_uint8         iOrdertype;
        } mng_ordr_entry;
typedef mng_ordr_entry * mng_ordr_entryp;

typedef struct mng_ordr_struct {       /* ORDR */
           mng_chunk_header  sHeader;
           mng_uint32        iCount;
           mng_ordr_entryp   pEntries;
        } mng_ordr;
typedef mng_ordr * mng_ordrp;

/* ************************************************************************** */

typedef struct {                       /* MAGN */
           mng_chunk_header  sHeader;
           mng_uint16        iFirstid;
           mng_uint16        iLastid;
           mng_uint16        iMethodX;
           mng_uint16        iMX;
           mng_uint16        iMY;
           mng_uint16        iML;
           mng_uint16        iMR;
           mng_uint16        iMT;
           mng_uint16        iMB;
           mng_uint16        iMethodY;
        } mng_magn;
typedef mng_magn * mng_magnp;

/* ************************************************************************** */

typedef struct {                       /* unknown chunk */
           mng_chunk_header  sHeader;
           mng_uint32        iDatasize;
           mng_ptr           pData;
        } mng_unknown_chunk;
typedef mng_unknown_chunk * mng_unknown_chunkp;

/* ************************************************************************** */

#endif /* _libmng_chunks_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
