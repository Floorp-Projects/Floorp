#include <stdio.h>

#include "nsISupportsUtils.h"
#include "nsTraceRefCntImpl.h"

int main(int argc, char* argv[])
{
    nsTraceRefcntImpl::WalkTheStack(stdout);
    return 0;
}

