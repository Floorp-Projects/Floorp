#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"

static void
nsXPIDLStringTest_Value(PRUnichar** aResult)
{
    *aResult = ToNewUnicode(NS_LITERAL_STRING("Hello, World"));
}

int
main(int argc, char* argv[])
{
    nsXPIDLString s1;
    nsXPIDLStringTest_Value(getter_Copies(s1));
    return 0;
}

