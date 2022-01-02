#include "FuzzingInterface.h"
#include "FuzzingStreamListener.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(FuzzingStreamListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
FuzzingStreamListener::OnStartRequest(nsIRequest* aRequest) {
  FUZZING_LOG(("FuzzingStreamListener::OnStartRequest"));
  return NS_OK;
}

NS_IMETHODIMP
FuzzingStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  FUZZING_LOG(("FuzzingStreamListener::OnDataAvailable"));
  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  nsCString data;

  while (aCount) {
    nsresult rv = NS_ReadInputStreamToString(aInputStream, data, toRead);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aCount -= toRead;
    toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  }
  return NS_OK;
}

NS_IMETHODIMP
FuzzingStreamListener::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  FUZZING_LOG(("FuzzingStreamListener::OnStopRequest"));
  mChannelDone = true;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
