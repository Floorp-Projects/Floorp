#ifndef __typeinfo__
#define __typeinfo__

#include "prtypes.h"
#include <wchar.h>

typedef wchar_t wchar;
typedef wchar* nsSTR;
typedef PRUint16 nsDISPID;
typedef PRUint32 nsHREFTYPE;

enum nsTIERROR {
	NS_TYPE_MISMATCH = 100,
	NS_NOSUCH_FUN = 101,
	NS_NOSUCH_VAR = 102,
	NS_NOSUCH_NAME = 103,
	NS_DISPATCH_MISMATCH = 104,
	NS_NOSUCH_INDEX = 105,
	NS_NOSUCH_ID = 106,
	NS_ARGUMENT_MISMATCH = 107,
};

enum nsSYSKIND {
	SYS_WIN32,
	SYS_MAC,
	SYS_UNIX
};

struct nsTLIBATTR {
    nsIID guid;
    PRUint16 lcid;
    nsSYSKIND syskind;
    PRInt32 wMajorVerNum;
    PRInt32 wMinorVerNum;
    PRInt32 wLibFlags;
};

enum nsTYPEKIND {
	TKIND_INTERFACE,
	TKIND_COCLASS
};

typedef PRUint16 nsVARTYPE;

enum nsVARENUM {
    VT_EMPTY     = 0,            // Not specified.
    VT_NULL      = 1,            // Null.
    VT_I2        = 2,            // 2-byte signed int.
    VT_I4        = 3,            // 4-byte signed int.
    VT_R4        = 4,            // 4-byte real.
    VT_R8        = 5,            // 8-byte real.
    VT_ERROR     = 6,           // status code (scode).
    VT_BOOL      = 7,          // Boolean;
    VT_SUPPORT   = 8,          // nsISupports *.
    VT_C1       = 9,          // Unsigned char.
    VT_C2       = 10,          // Unicode char.
    VT_STR       = 11,          // C string.
    VT_WSTR       = 12,          // Unicode string.
    VT_VOID       = 13,          // void *.
    // By reference, a pointer to the data is passed.
	// Added as extra bit.
    VT_BYREF      = (int) 0x4000
};

struct nsVARIANT {
    nsVARTYPE kind;
    union {
        unsigned char        cVal;
        wchar                wVal;
		char                 *strVal;
		nsSTR                wstrVal;
        PRInt16              iVal;
        PRInt32              lVal;
        float                fltVal;
        double               dblVal;
        PRBool               boolVal;
        PRUint16             scode;
        nsISupports          * suppVal;
        void                 * voidVal;
        unsigned char        * pcVal;
        wchar                * pwVal;
        nsSTR                * pstrVal;
        PRInt16              * piVal;
        PRInt32              * plVal;
        float                * pfltVal;
        double               * pdblVal;
        PRBool               * pboolVal;
        PRUint16             * pscode;
        nsISupports          ** psuppVal;
        void                 ** pvoidVal;
    } u;
};

struct nsDISPPARAMS {
	nsVARIANT *argv;               // Array of arguments.
    PRUint32 argc;                // Number of arguments.
};

struct nsEXCEP {
    PRUint16 scode;        // An error code describing the error.
    nsSTR description;        // Textual description of the error.
};

struct nsTYPEATTR {
    nsIID guid;                    // The GUID of the type information. 
    PRUint16 lcid;                    // Locale of member names and doc 
                               // strings
    PRInt32 cbSizeInstance;// The size of an instance of 
                                // this type.
    nsTYPEKIND typekind;            // The kind of type this information
                                // describes.
    PRUint16 cFuncs;        // Number of functions.
    PRUint16 cVars;        // Number of variables/data members.
    PRInt16 cImplTypes;    // Number of implemented interfaces.
    PRInt16 wTypeFlags;
    PRInt16 wMajorVerNum;    // Major version number.
    PRInt16 wMinorVerNum;    // Minor version number.
};

enum nsINVOKEKIND {
    INVOKE_FUNC=0,
    INVOKE_PROPERTYGET=1,
    INVOKE_PROPERTYPUT=2
};

enum nsDISPATCHKIND {
    DISPATCH_FUNC=0,
    DISPATCH_PROPERTYGET=1,
    DISPATCH_PROPERTYPUT=2
};

enum nsFUNCKIND {    
	FUNC_VIRTUAL,
	FUNC_STATIC
};

struct nsFUNCDESC {
    nsDISPID dispid;                        // Function dispatch ID.
	nsVARTYPE *paramDesc;		/* tags for parameters */
	nsFUNCKIND funckind;  // Specifies whether the function is virtual or static
    nsINVOKEKIND invkind;        // Invocation kind. Indicates if this is a                             // property function, and if so, what kind.
    PRUint32 cParams;            // Count of total number of parameters.
    nsVARTYPE retDesc;    // Contains the return type of the function.
};

enum nsVARKIND {
    VAR_INSTANCE,
    VAR_STATIC
};

struct nsVARDESC {
    nsDISPID dispid;
    nsVARTYPE varType;
    nsVARKIND varkind;
};

class nsITypeInfo;
class nsITypeLib;

#endif
