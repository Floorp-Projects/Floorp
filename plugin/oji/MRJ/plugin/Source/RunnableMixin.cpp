/*
	RunnableMixin.cpp

	Provides a mixin nsIRunnable implementation.
	
	by Patrick C. Beard.
 */

#include "RunnableMixin.h"

const InterfaceInfo RunnableMixin::sInterfaces[] = {
	{ NS_IRUNNABLE_IID, INTERFACE_OFFSET(RunnableMixin, nsIRunnable) },
};
const UInt32 RunnableMixin::kInterfaceCount = sizeof(sInterfaces) / sizeof(InterfaceInfo);

RunnableMixin::RunnableMixin()
	:	SupportsMixin(this, sInterfaces, kInterfaceCount)
{
}
