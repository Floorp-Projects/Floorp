/*
	RunnableMixin.h
	
	Provides a mixin nsIRunnable implementation.
	
	by Patrick C. Beard.
 */

#pragma once

#include "nsIThreadManager.h"
#include "SupportsMixin.h"

class RunnableMixin : public nsIRunnable, private SupportsMixin {
public:
	RunnableMixin();

	DECL_SUPPORTS_MIXIN

	NS_IMETHOD Run() = 0;

private:
	// support for SupportsMixin.
	static const InterfaceInfo sInterfaces[];
	static const UInt32 kInterfaceCount;
};
