
#include "nsIMemoryReporter.h"
#include "nsCOMArray.h"
#include "mozilla/Mutex.h"
#include "nsString.h"

using mozilla::Mutex;

class nsMemoryReporter : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  nsMemoryReporter(nsACString& process,
                   nsACString& path, 
                   PRInt32 kind,
                   PRInt32 units,
                   PRInt64 amount,
                   nsACString& desc);

  ~nsMemoryReporter();

protected:
  nsCString mProcess;
  nsCString mPath;
  PRInt32   mKind;
  PRInt32   mUnits;
  PRInt64   mAmount;
  nsCString mDesc;
};


class nsMemoryReporterManager : public nsIMemoryReporterManager
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYREPORTERMANAGER

    nsMemoryReporterManager();
    virtual ~nsMemoryReporterManager();

private:
    nsCOMArray<nsIMemoryReporter>      mReporters;
    nsCOMArray<nsIMemoryMultiReporter> mMultiReporters;
    Mutex                              mMutex;
};

#define NS_MEMORY_REPORTER_MANAGER_CID \
{ 0xfb97e4f5, 0x32dd, 0x497a, \
{ 0xba, 0xa2, 0x7d, 0x1e, 0x55, 0x7, 0x99, 0x10 } }
