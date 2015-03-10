#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "gtest/gtest.h"

static void
nsXPIDLStringTest_Value(char16_t** aResult)
{
    *aResult = ToNewUnicode(NS_LITERAL_STRING("Hello, World"));
}

TEST(XPIDLString, Main)
{
    nsXPIDLString s1;
    nsXPIDLStringTest_Value(getter_Copies(s1));
    EXPECT_TRUE(s1.EqualsLiteral("Hello, World"));
}

