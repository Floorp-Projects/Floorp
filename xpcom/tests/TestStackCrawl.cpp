#include <stdio.h>

#include "nsISupportsUtils.h"
#include "nsTraceRefCnt.h"

int main(int argc, char* argv[])
{
    nsTraceRefcnt::WalkTheStack(stdout);
    return 0;
}

