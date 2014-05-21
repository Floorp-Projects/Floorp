/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Types.h"
#define STAGEFRIGHT_EXPORT __attribute__ ((visibility ("default")))

#include "media/stagefright/foundation/ALooper.h"
#include "media/stagefright/foundation/AMessage.h"
#include "media/stagefright/foundation/ABuffer.h"
#include "media/stagefright/foundation/AString.h"
#include "media/ICrypto.h"
#include "gui/Surface.h"


namespace android {
	
// stub for Mutex
MOZ_EXPORT
Mutex::Mutex()
{
}

MOZ_EXPORT
Mutex::~Mutex()
{
}

// stub for Condition
MOZ_EXPORT
Condition::Condition()
{
}

MOZ_EXPORT
Condition::~Condition()
{
}

// stub for AString
MOZ_EXPORT
AString::AString()
{
}

MOZ_EXPORT
AString::~AString()
{
}

MOZ_EXPORT
struct ALooper::LooperThread:public Thread
{
};

// stub for ALooper
MOZ_EXPORT 
ALooper::ALooper() : mRunningLocally(false) 
{
}

MOZ_EXPORT 
ALooper::~ALooper() 
{
}

MOZ_EXPORT status_t 
ALooper::start(bool runOnCallingThread, 
			   bool canCallJava, 
			   int32_t priority)
{
	return 0;
}

MOZ_EXPORT status_t 
ALooper::stop()
{
	return 0;
}

// stub for AMessage
MOZ_EXPORT 
AMessage::AMessage(uint32_t what, 
				   int target)
	: mWhat(what),
      mTarget(target),
      mNumItems(0)
{
}

MOZ_EXPORT void
AMessage::clear()
{
}

MOZ_EXPORT 
AMessage::~AMessage()
{
}

MOZ_EXPORT void 
AMessage::setString(
        const char *name, const char *s, int len)
{
}

MOZ_EXPORT void 
AMessage::setInt32(const char *name, int32_t value)
{
}

// stub for ABuffer
MOZ_EXPORT void 
ABuffer::setRange(size_t offset, size_t size)
{
}

}

