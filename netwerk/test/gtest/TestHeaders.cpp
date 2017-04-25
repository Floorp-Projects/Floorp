#include "gtest/gtest.h"

#include "nsHttpHeaderArray.h"


TEST(TestHeaders, DuplicateHSTS) {
    // When the Strict-Transport-Security header is sent multiple times, its
    // effective value is the value of the first item. It is not merged as other
    // headers are.
    mozilla::net::nsHttpHeaderArray headers;
    nsresult rv = headers.SetHeaderFromNet(
        mozilla::net::nsHttp::Strict_Transport_Security, NS_LITERAL_CSTRING("max-age=360"), true
    );
    ASSERT_EQ(rv, NS_OK);

    nsAutoCString h;
    rv = headers.GetHeader(mozilla::net::nsHttp::Strict_Transport_Security, h);
    ASSERT_EQ(rv, NS_OK);
    ASSERT_EQ(h.get(), "max-age=360");

    rv = headers.SetHeaderFromNet(
        mozilla::net::nsHttp::Strict_Transport_Security, NS_LITERAL_CSTRING("max-age=720"), true
    );
    ASSERT_EQ(rv, NS_OK);

    rv = headers.GetHeader(mozilla::net::nsHttp::Strict_Transport_Security, h);
    ASSERT_EQ(rv, NS_OK);
    ASSERT_EQ(h.get(), "max-age=360");
}
