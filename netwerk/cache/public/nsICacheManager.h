#ifndef _nsICacheManger_H_
#define _nsICacheManger_H_

#include "nsISupports.h"
#include "nsICacheModule.h"
#include "nsICachePref.h"

// nsICacheManager {05A4BC00-3E1A-11d3-87EE-000629D01344}
#define NS_ICACHEMANAGER_IID \
{0x5a4bc00, 0x3e1a, 0x11d3, \
 {0x87, 0xee, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44}}

// {B8B5D4E0-3F92-11d3-87EF-000629D01344}
#define NS_CACHEMANAGER_CID \
 {0xb8b5d4e0, 0x3f92, 0x11d3, \
  {0x87, 0xef, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44}}

class nsICacheManager : public nsISupports
{
  public:
	
    //Reserved modules
    enum modules
    {
       MEM =0,
       DISK=1
    };

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICACHEMANAGER_IID) ;

    NS_IMETHOD Contains(const char* i_url) const = 0 ;

    NS_IMETHOD Entries(PRInt16 * n_Entries) const  = 0 ;

    NS_IMETHOD GetObj(const char* i_url, void ** o_Object) const = 0 ;    

    NS_IMETHOD GetModule(PRInt16 i_index, nsICacheModule** o_module) const = 0 ;
    NS_IMETHOD GetDiskModule(nsICacheModule** o_module) const = 0 ;
    NS_IMETHOD GetMemModule(nsICacheModule** o_module) const = 0 ;

    NS_IMETHOD AddModule (PRInt16 * o_Index, nsICacheModule * pModule) = 0 ;

    NS_IMETHOD GetPrefs(nsICachePref** o_Pref) const = 0 ;

    NS_IMETHOD InfoAsHTML(char** o_Buffer) const = 0 ;

    NS_IMETHOD IsOffline(PRBool * bOffline) const = 0 ;

    NS_IMETHOD Offline(PRBool bSet) = 0 ;

    NS_IMETHOD Remove(const char* i_url) = 0 ;

    NS_IMETHOD WorstCaseTime(PRUint32 * o_Time) const = 0 ;

} ;

#endif
