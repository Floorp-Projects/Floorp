/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_cms.h              copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.2                                                      * */
/* *                                                                        * */
/* * purpose   : color management routines (definition)                     * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of color management routines                    * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added creatememprofile                                   * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_cms_h_
#define _libmng_cms_h_

/* ************************************************************************** */

#ifdef MNG_INCLUDE_LCMS
void        mnglcms_initlibrary       (void);
mng_cmsprof mnglcms_createfileprofile (mng_pchar    zFilename);
mng_cmsprof mnglcms_creatememprofile  (mng_uint32   iProfilesize,
                                       mng_ptr      pProfile );
void        mnglcms_freeprofile       (mng_cmsprof  hProf    );
void        mnglcms_freetransform     (mng_cmstrans hTrans   );
#endif

/* ************************************************************************** */

#ifdef MNG_FULL_CMS
mng_retcode init_full_cms          (mng_datap pData);
mng_retcode init_full_cms_object   (mng_datap pData);
mng_retcode correct_full_cms       (mng_datap pData);
#endif

#if defined(MNG_FULL_CMS) || defined(MNG_GAMMA_ONLY)
mng_retcode init_gamma_only        (mng_datap pData);
mng_retcode init_gamma_only_object (mng_datap pData);
mng_retcode correct_gamma_only     (mng_datap pData);
#endif

#ifdef MNG_APP_CMS
mng_retcode init_app_cms           (mng_datap pData);
mng_retcode init_app_cms_object    (mng_datap pData);
mng_retcode correct_app_cms        (mng_datap pData);
#endif

/* ************************************************************************** */

#endif /* _libmng_cms_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
