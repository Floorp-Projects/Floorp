/* this file contains the actual definitions of */
/* the IIDs and CLSIDs */

/* link this file in with the server and any clients */


/* File created by MIDL compiler version 3.03.0110 */
/* at Sun Sep 20 22:13:10 1998
 */
/* Compiler settings for MozillaControl.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: none
*/
//@@MIDL_FILE_HEADING(  )
#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

const IID LIBID_MOZILLACONTROLLib = {0x1339B53E,0x3453,0x11D2,{0x93,0xB9,0x00,0x00,0x00,0x00,0x00,0x00}};


const IID IID_IWebBrowser = {0xEAB22AC1,0x30C1,0x11CF,{0xA7,0xEB,0x00,0x00,0xC0,0x5B,0xAE,0x0B}};


const IID DIID_DWebBrowserEvents = {0xEAB22AC2,0x30C1,0x11CF,{0xA7,0xEB,0x00,0x00,0xC0,0x5B,0xAE,0x0B}};


const CLSID CLSID_MozillaBrowser = {0x1339B54C,0x3453,0x11D2,{0x93,0xB9,0x00,0x00,0x00,0x00,0x00,0x00}};


#ifdef __cplusplus
}
#endif

