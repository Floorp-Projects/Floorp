/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*****************************************************************************
*                                                                            *
* Module Name     : OS2UPDT.H                                                *
*                                                                            *
*                                                                            *
* Description     : Include file for SU_WIN.C                                *
*                                                                            *
*                                                                            *
*****************************************************************************/

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Various program defines                                           º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
/* String processing definitions */
//XP_OS2#define CR                       (CHAR) '\r'
//XP_OS2#define LF                       (CHAR) '\n'
// #define EOFFILE               (CHAR) '\032'                         //P1D
#define EOFFILE                  (CHAR) '\x1A'                         //P1A
#define BLANK                    (CHAR) ' '
//XP_OS2#define TAB                      (CHAR) '\t'
#define EQUAL                    (CHAR) '='
#define COLON                    (CHAR) ':'
#define SEMICOLON                (CHAR) ';'
#define ZEND                     (CHAR) '\0'

//XP_OS2#define CRLF                     "\r\n"
// #define EOFLINE               "\r\n\032"                            //P1D
#define EOFLINE                  "\r\n\x1A"                            //P1A
#define BLANK_TAB                " \t"
//#define BLANK_TAB_EOFLINE      " \t\r\n\032"                         //P1D
//#define SEMIC_EOFLINE          ";\r\n\032"                           //P1D
#define BLANK_TAB_EOFLINE        " \t\r\n\x1A" /* P1A                      */
#define SEMIC_EOFLINE            ";\r\n\x1A"   /* P1A                      */


//XP_OS2#define MAXPATHLEN     260
#define DASD_FLAG      0
#define INHERIT        0x08
#define WRITE_THRU     0
#define FAIL_FLAG      0
#define SHARE_FLAG     0x10
#define ACCESS_FLAG    0x02
/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Various program defines                                           º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */

#define CONFIGSYS                "C:\\CONFIG.SYS"

#define FOUR_K                   4*1024
#define SEG_SIZE                 (ULONG)65536
#define NORM                     0x00
#define OS2HPFS                  "\\OS2\\HPFS.IFS"
#define WOS2HPFS                 "\\OS2\\BOOT\\HPFS.IFS"
#define OS2FAT                   "DISKCACHE"
#define ODI2NDILINE              "ODI2NDI.OS2"
#define INT15                    "INT15.SYS"
#define CPQPART                  "CPQPART.SYS"
#define CPQREDIR                 "CPQREDIR.FLT"
#define OS2CDROM                 "OS2CDROM.DMD"
#define DOSSYS                   "DOS.SYS"
#define MOUSESYS                 "MOUSE.SYS"
#define TESTCFG                  "TESTCFG.SYS"
#define INT15                    "INT15.SYS"
#define CPQPART                  "CPQPART.SYS"
#define NOSWAP                   "NOSWAP"


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Configuration routine defines                                     º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
                                         /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                         /*³ Type of the configuration line³*/
                                         /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
                                         /* Example:                        */
#define CSYS_COMMAND            200    /* BUFFERS=60                       */
#define CSYS_EV                 201    /* SET PROMPT=$i[$p]                */
#define CSYS_PATH_EV            202    /* SET HELP=C:\OS2\HELP;E:\EPM      */
#define CSYS_PATH               203    /* SET PATH=C:\OS2;C:\MUGLIB;       */
#define CSYS_DPATH              204    /* SET DPATH=C:\OS2;C:\MUGLIB\DLL;  */
#define CSYS_LIBPATH            205    /* LIBPATH=C:\OS2\DLL;              */
#define CSYS_DEVICE             206    /* DEVICE=C:\OS2\EGA.SYS            */
#define CSYS_RUN                207    /* RUN=C:\CMLIB\ACSTRSYS.EXE        */
#define CSYS_IFS                208    /* IFS=C:\OS2\HPFS.IFS /CACHE:64    */
#define CSYS_PROTSHELL          209    /* PROTSHELL=C:\SECURESH.EXE        */
#define CSYS_BOOKSHELF          210    /* SET BOOKSHELF=C:\OS2;C:\BOOK;    */
#define CSYS_AUTOSTART          211    /* SET AUTOSTART=PROGRAMS,          */
#define CSYS_20                 212    /* 20=NETWRKSTA.200                 */
#define CSYS_HELP               213    /* 20=NETWRKSTA.200                 */
#define CSYS_DISKCACHE          214
#define CSYS_CALL               215    /* CALL=C:\XXX...                   */
#define CSYS_REM                216    /* REM ANYTHING                     */
#define CSYS_SET                217    /* SET ANYTHING no '=' used         */
#define CSYS_BASEDEV            218
#define CSYS_MEMMAN             219

                                         /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                         /*³ CsysUpdate insert positions   ³*/
                                         /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
                                         /* These constants represent update*/
                                         /* locations within CONFIG.SYS.    */
                                         /* An update location is specified */
                                         /* as either one of these constants*/
                                         /* or an actual pointer within the */
                                         /* buffer.  Note that the constants*/
                                         /* can't be misconstrued as buffer */
                                         /* pointers since the selector is 0*/
                                         /*                                 */
                                         /* Update as the:                  */
#define CSYS_FIRST      (PSZ) 0x0000FFFF       /* First line in CONFIG.SYS */
#define CSYS_LAST       (PSZ) 0x0000FFFE       /* Last line in CONFIG.SYS  */
#define CSYS_FIRST_TYPE (PSZ) 0x0000FFFD   /* First or last DEVICE, RUN, or*/
#define CSYS_LAST_TYPE  (PSZ) 0x0000FFFC       /* IFS (depends on type)    */


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º   Forward Function Declarations                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
#ifdef __cplusplus
extern "C" { 
#endif
VOID StripFrontWhite(char *);
char *NextLine(char *);
VOID  CopyLine(char *, char *);
ULONG InsertString(char *, char *, char *);
                                            /* Steve's Functions            */
CHAR *ScootString(CHAR *, SHORT);
CHAR *CopyNBytes(CHAR *, CHAR *, USHORT);
USHORT StringLength(CHAR *);
ULONG ReadFileToBuffer(char *, char **,ULONG *);
ULONG WriteBufferToFile(char *, char **);
ULONG search_file_drive(char *, char *);
PSZ    FAR CsysQuery           (PSZ, USHORT, PSZ, PSZ);
PSZ    FAR CsysUpdate          (PSZ, USHORT, PSZ, PSZ, PSZ);
PSZ    FAR CsysDelete          (PSZ, USHORT, PSZ, PSZ);
ULONG WriteLockFileDDToConfig (PSZ pszListFile);
ULONG WriteLockFileRenameEntry(PSZ final, PSZ current, PSZ listfile);
                                         /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                         /*³ Routines used locally         ³*/
                                         /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
VOID   FAR BuildAssignment     (USHORT, PSZ, PSZ);
VOID   FAR FormatLine          (PSZ, PSZ);
#ifdef __cplusplus
}
#endif
