#include <nsCOMPtr.h>
#include <nsString.h>
#include <nsIURI.h>
#include <nsIChannel.h>
#include <nsIHTTPChannel.h>
#include <nsIInputStream.h>
#include <nsNetUtil.h>

/*
 * This is a very simple program that tests a "blocking" HTTP get.  The call to
 * OpenInputStream does not block; instead it returns a blocking nsIInputStream,
 * from which we read the HTTP response content.
 */

#define RETURN_IF_FAILED(rv, what) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(what ": failed - %08x\n", rv); \
        return -1; \
    } \
    PR_END_MACRO

int
main(int argc, char **argv)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIInputStream> inputStream;
    PRTime t1, t2;

    if (argc < 2) {
        printf("Usage: TestSyncHTTP <url>\n");
        return -1;
    }

    rv = NS_NewURI(getter_AddRefs(uri), argv[1]);
    RETURN_IF_FAILED(rv, "NS_NewURI");

    rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, nsnull);
    RETURN_IF_FAILED(rv, "NS_OpenURI");

    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel)
        httpChannel->SetOpenInputStreamHasEventQueue(PR_FALSE);

    t1 = PR_Now();

    rv = channel->OpenInputStream(getter_AddRefs(inputStream));
    RETURN_IF_FAILED(rv, "nsIChannel::OpenInputStream");

    //
    // read the response content...
    //
    char buf[256];
    PRUint32 bytesRead = 1, totalRead = 0;
    while (bytesRead > 0) {
        rv = inputStream->Read(buf, sizeof buf, &bytesRead);
        RETURN_IF_FAILED(rv, "nsIInputStream::Read");
        totalRead += bytesRead;
    }
    t2 = PR_Now();

    printf("total read: %u bytes\n", totalRead);
    printf("total read time: %0.3f\n", ((double) (t2 - t1))/1000000.0);
    return 0;
}
