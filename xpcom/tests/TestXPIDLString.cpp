#include "nsString.h"
#include "nsXPIDLString.h"

static void
nsXPIDLStringTest_Value(PRUnichar** aResult)
{
    nsAutoString s("Hello, World");
    *aResult = nsXPIDLString::Copy(s.GetUnicode());
}

static void
nsXPIDLStringTest_Reference(const PRUnichar** aResult)
{
    static const PRUnichar s[] = { 'H', 'e', 'l', 'l', 'o', ',', ' ',
                                   'w', 'o', 'r', 'l', 'd', '\0' };

    *aResult = s;
}

int
main(int argc, char* argv[])
{
    nsXPIDLString s1;
    nsXPIDLStringTest_Value(getter_Copies(s1));
    nsXPIDLStringTest_Reference(getter_Shares(s1));
    return 0;
}

