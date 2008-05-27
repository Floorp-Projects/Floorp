
#include "nsIMemoryReporter.h"
#include "nsCOMArray.h"

class nsMemoryReporterManager : public nsIMemoryReporterManager
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYREPORTERMANAGER

private:
    nsCOMArray<nsIMemoryReporter> mReporters;
};

#define NS_MEMORY_REPORTER_MANAGER_CID \
{ 0xfb97e4f5, 0x32dd, 0x497a, \
{ 0xba, 0xa2, 0x7d, 0x1e, 0x55, 0x7, 0x99, 0x10 } }
