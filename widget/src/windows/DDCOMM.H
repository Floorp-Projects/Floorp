#ifndef F_DDCOMM
#define F_DDCOMM

#define STRICT
#include <windows.h>
#include <ole2.h>
#include <ole2ver.h>

//extern "C" GUID CLSID_CfDataObj;
//#define RCF_FORMAT "RCF Format"

//ReleaseInterface calls 'Release' and NULLs the pointer
#define RELEASE_INTERFACE(p)\
            {\
            if (NULL!=p)\
                {\
                p->Release();\
                p=NULL;\
                }\
				}

#define SET_FORMATETC(fe, cf, td, asp, li, med)   \
	 {\
	 (fe).cfFormat=cf;\
	 (fe).ptd=td;\
	 (fe).dwAspect=asp;\
	 (fe).lindex=li;\
	 (fe).tymed=med;\
	 }


#endif

