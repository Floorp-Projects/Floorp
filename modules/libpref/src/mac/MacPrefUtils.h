/*
	MacPrefUtils.h
	
	File utilities used by macpref.cp. These come from a variety of sources, and
	should eventually be supplanted by John McMullen's nsFile utilities.
	
	Based on ufilemgr.h by atotic.
	
	by Patrick C. Beard.
 */

#pragma once

#ifndef __MAC_PREF_UTILS__
#define __MAC_PREF_UTILS__

#ifndef __MACTYPES__
#include <MacTypes.h>
#endif

struct FSSpec;

char * EncodedPathNameFromFSSpec(const FSSpec& inSpec, Boolean wantLeafName);
OSErr FSSpecFromLocalUnixPath(const char * url, FSSpec * outSpec, Boolean resolveAlias = true);
Boolean FileExists(const FSSpec& inSpec);

#endif /* __MAC_PREF_UTILS__ */
