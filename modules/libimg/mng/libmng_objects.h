/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_objects.h          copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : Internal object structures (definition)                    * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the internal object structures               * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* *             0.5.2 - 05/23/2000 - G.Juyn                                * */
/* *             - changed inclusion to DISPLAY_PROCS                       * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - added global color-chunks for animations                 * */
/* *             - added global PLTE,tRNS,bKGD chunks for animation         * */
/* *             - added SAVE & SEEK animation objects                      * */
/* *             0.5.2 - 05/29/2000 - G.Juyn                                * */
/* *             - added framenr/layernr/playtime to object header          * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - added ani-objects for delta-image processing             * */
/* *             - added compression/filter/interlace fields to             * */
/* *               object-buffer for delta-image processing                 * */
/* *                                                                        * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - changed definition of aTRNSentries                       * */
/* *             0.5.3 - 06/22/2000 - G.Juyn                                * */
/* *             - added definition for PPLT animation-processing           * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/10/2000 - G.Juyn                                * */
/* *             - fixed DEFI behavior                                      * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added support for delta-JNG                              * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - added valid-flag to stored objects for read() / display()* */
/* *             0.9.3 - 10/19/2000 - G.Juyn                                * */
/* *             - added storage for pixel-/alpha-sampledepth for delta's   * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_objects_h_
#define _libmng_objects_h_

/* ************************************************************************** */

#ifdef MNG_INCLUDE_DISPLAY_PROCS

/* ************************************************************************** */

typedef mng_retcode (*mng_cleanupobject) (mng_datap    pData,
                                          mng_objectp  pHeader);

typedef mng_retcode (*mng_processobject) (mng_datap    pData,
                                          mng_objectp  pHeader);

/* ************************************************************************** */

typedef struct {
           mng_cleanupobject fCleanup;
           mng_processobject fProcess;
           mng_objectp       pNext;              /* for double-linked list */
           mng_objectp       pPrev;
           mng_uint32        iFramenr;
           mng_uint32        iLayernr;
           mng_uint32        iPlaytime;
        } mng_object_header;
typedef mng_object_header * mng_object_headerp;

/* ************************************************************************** */

typedef struct {                                 /* MNG specification "object-buffer" */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint32        iRefcount;          /* reference counter */
           mng_bool          bFrozen;            /* frozen flag */
           mng_bool          bConcrete;          /* concrete flag */
           mng_bool          bViewable;          /* viewable flag */
           mng_uint32        iWidth;             /* image specifics */
           mng_uint32        iHeight;
           mng_uint8         iBitdepth;
           mng_uint8         iColortype;
           mng_uint8         iCompression;
           mng_uint8         iFilter;
           mng_uint8         iInterlace;
           
           mng_uint8         iAlphabitdepth;     /* used only for JNG images */
           mng_uint8         iJHDRcompression;
           mng_uint8         iJHDRinterlace;

           mng_uint8         iPixelsampledepth;  /* used with delta-images */
           mng_uint8         iAlphasampledepth;

           mng_bool          bHasPLTE;           /* PLTE chunk present */
           mng_bool          bHasTRNS;           /* tRNS chunk present */
           mng_bool          bHasGAMA;           /* gAMA chunk present */
           mng_bool          bHasCHRM;           /* cHRM chunk present */
           mng_bool          bHasSRGB;           /* sRGB chunk present */
           mng_bool          bHasICCP;           /* iCCP chunk present */
           mng_bool          bHasBKGD;           /* bKGD chunk present */

           mng_uint32        iPLTEcount;         /* PLTE fields */
           mng_rgbpaltab     aPLTEentries;

           mng_uint16        iTRNSgray;          /* tRNS fields */
           mng_uint16        iTRNSred;
           mng_uint16        iTRNSgreen;
           mng_uint16        iTRNSblue;
           mng_uint32        iTRNScount;
           mng_uint8arr      aTRNSentries;

           mng_uint32        iGamma;             /* gAMA fields */

           mng_uint32        iWhitepointx;       /* cHRM fields */
           mng_uint32        iWhitepointy;
           mng_uint32        iPrimaryredx;
           mng_uint32        iPrimaryredy;
           mng_uint32        iPrimarygreenx;
           mng_uint32        iPrimarygreeny;
           mng_uint32        iPrimarybluex;
           mng_uint32        iPrimarybluey;

           mng_uint8         iRenderingintent;   /* sRGB fields */

           mng_uint32        iProfilesize;       /* iCCP fields */
           mng_ptr           pProfile;

           mng_uint8         iBKGDindex;         /* bKGD fields */
           mng_uint16        iBKGDgray;
           mng_uint16        iBKGDred;
           mng_uint16        iBKGDgreen;
           mng_uint16        iBKGDblue;

           mng_uint32        iSamplesize;        /* size of a sample */
           mng_uint32        iRowsize;           /* size of a row of samples */
           mng_uint32        iImgdatasize;       /* size of the sample data buffer */
           mng_uint8p        pImgdata;           /* actual sample data buffer */

         } mng_imagedata;
typedef mng_imagedata * mng_imagedatap;

/* ************************************************************************** */

typedef struct {                                 /* MNG specification "object" */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iId;                /* object-id */
           mng_bool          bFrozen;            /* frozen flag */
           mng_bool          bVisible;           /* potential visibility flag */
           mng_bool          bViewable;          /* viewable flag */
           mng_bool          bValid;             /* marks invalid when only reading */
           mng_int32         iPosx;              /* location fields */
           mng_int32         iPosy;
           mng_bool          bClipped;           /* clipping fields */
           mng_int32         iClipl;
           mng_int32         iClipr;
           mng_int32         iClipt;
           mng_int32         iClipb;
           mng_uint16        iMAGN_MethodX;      /* magnification (MAGN) */
           mng_uint16        iMAGN_MethodY;
           mng_uint16        iMAGN_MX;
           mng_uint16        iMAGN_MY;
           mng_uint16        iMAGN_ML;
           mng_uint16        iMAGN_MR;
           mng_uint16        iMAGN_MT;
           mng_uint16        iMAGN_MB;
           mng_imagedatap    pImgbuf;            /* the image-data buffer */
        } mng_image;
typedef mng_image * mng_imagep;

/* ************************************************************************** */

                                                 /* "on-the-fly" image (= object 0) */       
typedef mng_image mng_ani_image;                 /* let's (ab)use the general "object" */
typedef mng_ani_image * mng_ani_imagep;          /* that's actualy crucial, so don't change it! */

/* ************************************************************************** */

typedef struct {                                 /* global PLTE object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint32        iEntrycount;
           mng_rgbpaltab     aEntries;
        } mng_ani_plte;
typedef mng_ani_plte * mng_ani_pltep;

/* ************************************************************************** */

typedef struct {                                 /* global tRNS object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint32        iRawlen;
           mng_uint8arr      aRawdata;
        } mng_ani_trns;
typedef mng_ani_trns * mng_ani_trnsp;

/* ************************************************************************** */

typedef struct {                                 /* global gAMA object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_bool          bEmpty;
           mng_uint32        iGamma;
        } mng_ani_gama;
typedef mng_ani_gama * mng_ani_gamap;

/* ************************************************************************** */

typedef struct {                                 /* global gCRM object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_bool          bEmpty;
           mng_uint32        iWhitepointx;
           mng_uint32        iWhitepointy;
           mng_uint32        iRedx;
           mng_uint32        iRedy;
           mng_uint32        iGreenx;
           mng_uint32        iGreeny;
           mng_uint32        iBluex;
           mng_uint32        iBluey;
        } mng_ani_chrm;
typedef mng_ani_chrm * mng_ani_chrmp;

/* ************************************************************************** */

typedef struct {                                 /* global sRGB object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_bool          bEmpty;
           mng_uint8         iRenderingintent;
        } mng_ani_srgb;
typedef mng_ani_srgb * mng_ani_srgbp;

/* ************************************************************************** */

typedef struct {                                 /* global iCCP object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_bool          bEmpty;
           mng_uint32        iProfilesize;
           mng_ptr           pProfile;
        } mng_ani_iccp;
typedef mng_ani_iccp * mng_ani_iccpp;

/* ************************************************************************** */

typedef struct {                                 /* global bKGD object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
        } mng_ani_bkgd;
typedef mng_ani_bkgd * mng_ani_bkgdp;

/* ************************************************************************** */

typedef struct {                                 /* LOOP object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iLevel;
           mng_uint32        iRepeatcount;
           mng_uint8         iTermcond;
           mng_uint32        iItermin;
           mng_uint32        iItermax;
           mng_uint32        iCount;
           mng_uint32p       pSignals;

           mng_uint32        iRunningcount;      /* running counter */
        } mng_ani_loop;
typedef mng_ani_loop * mng_ani_loopp;

/* ************************************************************************** */

typedef struct {                                 /* ENDL object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iLevel;

           mng_ani_loopp     pLOOP;              /* matching LOOP */
        } mng_ani_endl;
typedef mng_ani_endl * mng_ani_endlp;

/* ************************************************************************** */

typedef struct {                                 /* DEFI object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iId;                
           mng_bool          bHasdonotshow;
           mng_uint8         iDonotshow;
           mng_bool          bHasconcrete;
           mng_uint8         iConcrete;
           mng_bool          bHasloca;           
           mng_int32         iLocax;
           mng_int32         iLocay;
           mng_bool          bHasclip;
           mng_int32         iClipl;
           mng_int32         iClipr;
           mng_int32         iClipt;
           mng_int32         iClipb;
        } mng_ani_defi;
typedef mng_ani_defi * mng_ani_defip;

/* ************************************************************************** */

typedef struct {                                 /* BASI object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iRed;               
           mng_uint16        iGreen;             
           mng_uint16        iBlue;              
           mng_bool          bHasalpha;             
           mng_uint16        iAlpha;
           mng_uint8         iViewable;
        } mng_ani_basi;
typedef mng_ani_basi * mng_ani_basip;

/* ************************************************************************** */

typedef struct {                                 /* CLON object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iCloneid;
           mng_uint16        iSourceid;
           mng_uint8         iClonetype;
           mng_bool          bHasdonotshow;
           mng_uint8         iDonotshow;
           mng_uint8         iConcrete;
           mng_bool          bHasloca;
           mng_uint8         iLocatype;
           mng_int32         iLocax;
           mng_int32         iLocay;
        } mng_ani_clon;
typedef mng_ani_clon * mng_ani_clonp;

/* ************************************************************************** */

typedef struct {                                 /* BACK object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iRed;
           mng_uint16        iGreen;
           mng_uint16        iBlue;
           mng_uint8         iMandatory;
           mng_uint16        iImageid;
           mng_uint8         iTile;
        } mng_ani_back;
typedef mng_ani_back * mng_ani_backp;

/* ************************************************************************** */

typedef struct {                                 /* FRAM object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iFramemode;
           mng_uint8         iChangedelay;
           mng_uint32        iDelay;
           mng_uint8         iChangetimeout;
           mng_uint32        iTimeout;
           mng_uint8         iChangeclipping;
           mng_uint8         iCliptype;
           mng_int32         iClipl;
           mng_int32         iClipr;
           mng_int32         iClipt;
           mng_int32         iClipb;
        } mng_ani_fram;
typedef mng_ani_fram * mng_ani_framp;

/* ************************************************************************** */

typedef struct {                                 /* MOVE object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iFirstid;           
           mng_uint16        iLastid;            
           mng_uint8         iType;              
           mng_int32         iLocax;             
           mng_int32         iLocay;
        } mng_ani_move;
typedef mng_ani_move * mng_ani_movep;

/* ************************************************************************** */

typedef struct {                                 /* CLIP object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iFirstid;           
           mng_uint16        iLastid;            
           mng_uint8         iType;              
           mng_int32         iClipl;             
           mng_int32         iClipr;             
           mng_int32         iClipt;             
           mng_int32         iClipb;
        } mng_ani_clip;
typedef mng_ani_clip * mng_ani_clipp;

/* ************************************************************************** */

typedef struct {                                 /* SHOW object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iFirstid;           
           mng_uint16        iLastid;            
           mng_uint8         iMode;
        } mng_ani_show;
typedef mng_ani_show * mng_ani_showp;

/* ************************************************************************** */

typedef struct {                                 /* TERM object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iTermaction;        
           mng_uint8         iIteraction;        
           mng_uint32        iDelay;             
           mng_uint32        iItermax;
        } mng_ani_term;
typedef mng_ani_term * mng_ani_termp;

/* ************************************************************************** */

typedef struct {                                 /* SAVE object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
        } mng_ani_save;
typedef mng_ani_save * mng_ani_savep;

/* ************************************************************************** */

typedef struct {                                 /* SEEK object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
        } mng_ani_seek;
typedef mng_ani_seek * mng_ani_seekp;

/* ************************************************************************** */

typedef struct {                                 /* DHDR object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint16        iObjectid;
           mng_uint8         iImagetype;
           mng_uint8         iDeltatype;
           mng_uint32        iBlockwidth;
           mng_uint32        iBlockheight;
           mng_uint32        iBlockx;
           mng_uint32        iBlocky;
        } mng_ani_dhdr;
typedef mng_ani_dhdr * mng_ani_dhdrp;

/* ************************************************************************** */

typedef struct {                                 /* PROM object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iBitdepth;
           mng_uint8         iColortype;
           mng_uint8         iFilltype;
        } mng_ani_prom;
typedef mng_ani_prom * mng_ani_promp;

/* ************************************************************************** */

typedef struct {                                 /* IPNG object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
        } mng_ani_ipng;
typedef mng_ani_ipng * mng_ani_ipngp;

/* ************************************************************************** */

typedef struct {                                 /* IJNG object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
        } mng_ani_ijng;
typedef mng_ani_ijng * mng_ani_ijngp;

/* ************************************************************************** */

typedef struct {                                 /* PPLT object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
           mng_uint8         iType;
           mng_uint32        iCount;
           mng_rgbpaltab     aIndexentries;
           mng_uint8arr      aAlphaentries;
           mng_uint8arr      aUsedentries;
        } mng_ani_pplt;
typedef mng_ani_pplt * mng_ani_ppltp;

/* ************************************************************************** */

typedef struct {                                 /* MAGN object */
           mng_object_header sHeader;            /* default header (DO NOT REMOVE) */
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
        } mng_ani_magn;
typedef mng_ani_magn * mng_ani_magnp;

/* ************************************************************************** */

#endif /* MNG_INCLUDE_DISPLAY_PROCS */

/* ************************************************************************** */

#endif /* _libmng_objects_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
