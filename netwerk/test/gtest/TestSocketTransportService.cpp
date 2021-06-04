#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"
#include "../../base/nsSocketTransportService2.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

TEST(TestSocketTransportService, PortRemappingPreferenceReading)
{
  nsCOMPtr<nsISocketTransportService> service =
      do_GetService("@mozilla.org/network/socket-transport-service;1");
  ASSERT_TRUE(service);

  auto* sts = gSocketTransportService;
  ASSERT_TRUE(sts);

  sts->Dispatch(
      NS_NewRunnableFunction(
          "test",
          [&]() {
            auto CheckPortRemap = [&](uint16_t input, uint16_t output) -> bool {
              sts->ApplyPortRemap(&input);
              return input == output;
            };

            // Ill-formed prefs
            ASSERT_FALSE(sts->UpdatePortRemapPreference(";"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference(" ;"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("; "_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("foo"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference(" foo"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference(" foo "_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("10=20;"_ns));

            ASSERT_FALSE(sts->UpdatePortRemapPreference("1"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1="_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1,="_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-="_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-,="_ns));

            ASSERT_FALSE(sts->UpdatePortRemapPreference("1=2,"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1=2-3"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-2,=3"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-2,3"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-2,3-4"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-2,3-4,"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-2,3-4="_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("10000000=10"_ns));

            ASSERT_FALSE(sts->UpdatePortRemapPreference("1=2;3"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=2;3"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=2;3="_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-foo=2;3=15"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=foo;3=15"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=2;foo=15"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=2;3=foo"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1-4=2x3=15"_ns));
            ASSERT_FALSE(sts->UpdatePortRemapPreference("1+4=2;3=15"_ns));

            // Well-formed prefs
            ASSERT_TRUE(sts->UpdatePortRemapPreference("1=2"_ns));
            ASSERT_TRUE(CheckPortRemap(1, 2));
            ASSERT_TRUE(CheckPortRemap(2, 2));
            ASSERT_TRUE(CheckPortRemap(3, 3));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("10=20"_ns));
            ASSERT_TRUE(CheckPortRemap(1, 1));
            ASSERT_TRUE(CheckPortRemap(2, 2));
            ASSERT_TRUE(CheckPortRemap(3, 3));
            ASSERT_TRUE(CheckPortRemap(10, 20));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("100-200=1000"_ns));
            ASSERT_TRUE(CheckPortRemap(10, 10));
            ASSERT_TRUE(CheckPortRemap(99, 99));
            ASSERT_TRUE(CheckPortRemap(100, 1000));
            ASSERT_TRUE(CheckPortRemap(101, 1000));
            ASSERT_TRUE(CheckPortRemap(200, 1000));
            ASSERT_TRUE(CheckPortRemap(201, 201));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("100-200,500=1000"_ns));
            ASSERT_TRUE(CheckPortRemap(10, 10));
            ASSERT_TRUE(CheckPortRemap(99, 99));
            ASSERT_TRUE(CheckPortRemap(100, 1000));
            ASSERT_TRUE(CheckPortRemap(101, 1000));
            ASSERT_TRUE(CheckPortRemap(200, 1000));
            ASSERT_TRUE(CheckPortRemap(201, 201));
            ASSERT_TRUE(CheckPortRemap(499, 499));
            ASSERT_TRUE(CheckPortRemap(500, 1000));
            ASSERT_TRUE(CheckPortRemap(501, 501));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("1-3=10;5-8,12=20"_ns));
            ASSERT_TRUE(CheckPortRemap(1, 10));
            ASSERT_TRUE(CheckPortRemap(2, 10));
            ASSERT_TRUE(CheckPortRemap(3, 10));
            ASSERT_TRUE(CheckPortRemap(4, 4));
            ASSERT_TRUE(CheckPortRemap(5, 20));
            ASSERT_TRUE(CheckPortRemap(8, 20));
            ASSERT_TRUE(CheckPortRemap(11, 11));
            ASSERT_TRUE(CheckPortRemap(12, 20));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("80=8080;443=8080"_ns));
            ASSERT_TRUE(CheckPortRemap(80, 8080));
            ASSERT_TRUE(CheckPortRemap(443, 8080));

            // Later rules rewrite earlier rules
            ASSERT_TRUE(sts->UpdatePortRemapPreference("10=100;10=200"_ns));
            ASSERT_TRUE(CheckPortRemap(10, 200));

            ASSERT_TRUE(
                sts->UpdatePortRemapPreference("10-20=100;10-20=200"_ns));
            ASSERT_TRUE(CheckPortRemap(10, 200));
            ASSERT_TRUE(CheckPortRemap(20, 200));

            ASSERT_TRUE(sts->UpdatePortRemapPreference("10-20=100;15=200"_ns));
            ASSERT_TRUE(CheckPortRemap(10, 100));
            ASSERT_TRUE(CheckPortRemap(15, 200));
            ASSERT_TRUE(CheckPortRemap(20, 100));

            ASSERT_TRUE(sts->UpdatePortRemapPreference(
                " 100 - 200 = 1000 ; 150 = 2000 "_ns));
            ASSERT_TRUE(CheckPortRemap(100, 1000));
            ASSERT_TRUE(CheckPortRemap(150, 2000));
            ASSERT_TRUE(CheckPortRemap(200, 1000));

            // Turn off any mapping
            ASSERT_TRUE(sts->UpdatePortRemapPreference(""_ns));
            for (uint32_t port = 0; port < 65536; ++port) {
              ASSERT_TRUE(CheckPortRemap((uint16_t)port, (uint16_t)port));
            }
          }),
      NS_DISPATCH_SYNC);
}

}  // namespace net
}  // namespace mozilla
