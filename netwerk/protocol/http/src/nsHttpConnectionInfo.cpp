#include "nsHttpConnectionInfo.h"
#include "nsPrintfCString.h"

void
nsHttpConnectionInfo::SetOriginServer(const nsACString &host, PRInt32 port)
{
    mHost = host;
    mPort = port == -1 ? DefaultPort() : port;

    //
    // build hash key:
    //
    // the hash key uniquely identifies the connection type.  two connections
    // are "equal" if they end up talking the same protocol to the same server.
    //

    const char *keyHost;
    PRInt32 keyPort;

    if (mUsingHttpProxy && !mUsingSSL) {
        keyHost = ProxyHost();
        keyPort = ProxyPort();
    }
    else {
        keyHost = Host();
        keyPort = Port();
    }

    mHashKey.Assign(NS_LITERAL_CSTRING("..") +
                    nsDependentCString(keyHost) +
                    nsPrintfCString(":%d", keyPort));

    if (mUsingHttpProxy)
        mHashKey.SetCharAt('P', 0);
    if (mUsingSSL)
        mHashKey.SetCharAt('S', 1);

    // NOTE: for transparent proxies (e.g., SOCKS) we need to encode the proxy
    // type in the hash key (this ensures that we will continue to speak the
    // right protocol even if our proxy preferences change).
    if (!mUsingHttpProxy && ProxyHost())
        mHashKey.Append(NS_LITERAL_CSTRING(" (") +
                        nsDependentCString(ProxyType()) +
                        NS_LITERAL_CSTRING(")"));
}
