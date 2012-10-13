/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFLogStream_h
#define CSFLogStream_h

#include "CSFLog.h"

#ifdef DEBUG
#include <string>
#include <sstream>
#include <iostream>

#define CSFLogCriticalS(tag, message)	{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_CRITICAL, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }
#define CSFLogErrorS(tag, message)		{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_ERROR, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }
#define CSFLogWarnS(tag, message)		{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_WARNING, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }
#define CSFLogNoticeS(tag, message)		{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_NOTICE, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }
#define CSFLogInfoS(tag, message)		{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_INFO, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }
#define CSFLogDebugS(tag, message)		{ std::ostringstream _oss; _oss << message << std::endl; CSFLog( CSF_LOG_DEBUG, __FILE__ , __LINE__ , tag, _oss.str().c_str()); }

#else // DEBUG

#define CSFLogCriticalS(tag, message)   {}
#define CSFLogErrorS(tag, message)      {}
#define CSFLogWarnS(tag, message)       {}
#define CSFLogNoticeS(tag, message)     {}
#define CSFLogInfoS(tag, message)       {}
#define CSFLogDebugS(tag, message)      {}

#endif // DEBUG

#endif
