#ifndef nsIProfile_h__
#define nsIProfile_h__

#include "xp_core.h"
#include "jsapi.h"
#include "nsISupports.h"
#include "nsFileSpec.h"

/** new ids **/
#define NS_IPROFILE_IID                                \
  { /* {02b0625a-e7f3-11d2-9f5a-006008a6efe9} */       \
    0x02b0625a,                                        \
    0xe7f3,                                            \
    0x11d2,                                            \
    { 0x9f, 0x5a, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9 } \
  }

#define NS_PROFILE_CID                                 \
  { /* {02b0625b-e7f3-11d2-9f5a-006008a6efe9} */       \
    0x02b0625b,                                        \
    0xe7f3,                                            \
    0x11d2,                                            \
    { 0x9f, 0x5a, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9 } \
  }

#define NS_USING_PROFILES 1

/*
 * Return values
 */

class nsIProfile: public nsISupports {
public:

  static const nsIID& GetIID(void) { static nsIID iid = NS_IPROFILE_IID; return iid; }

	// Initialize/shutdown
	NS_IMETHOD Startup(char *filename) = 0;
	NS_IMETHOD Shutdown() = 0;

	// Getters
	NS_IMETHOD GetProfileDir(const char *profileName, nsFileSpec* profileDir) = 0;
	NS_IMETHOD GetProfileCount(int *numProfiles) = 0;
	NS_IMETHOD GetSingleProfile(char **profileName) = 0;
	NS_IMETHOD GetCurrentProfile(char **profileName) = 0;
	NS_IMETHOD GetFirstProfile(char **profileName) = 0;
	NS_IMETHOD GetCurrentProfileDir(nsFileSpec* profileDir) = 0;

	// Setters
	NS_IMETHOD SetProfileDir(const char *profileName, const nsFileSpec& profileDir) = 0;
};

#endif /* nsIProfile_h__ */
