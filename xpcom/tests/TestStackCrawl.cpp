#include <stdio.h>

#include "nsISupportsUtils.h"
#include "nsTraceRefCnt.h"

int main(int argc, char* argv[])
{
    char buf[16384];
    nsTraceRefcnt::WalkTheStack(buf, sizeof(buf));
    printf("%s\n", buf);

    return 0;
}

