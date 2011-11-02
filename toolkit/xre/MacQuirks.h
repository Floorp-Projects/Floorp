/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef MacQuirks_h__
#define MacQuirks_h__

#include <sys/types.h>
#include <sys/sysctl.h>
#include "CoreFoundation/CoreFoundation.h"
#include "CoreServices/CoreServices.h"
#include "Carbon/Carbon.h"

static void
TriggerQuirks()
{
  int mib[2];

  mib[0] = CTL_KERN;
  mib[1] = KERN_OSRELEASE;
  // we won't support versions greater than 10.7.99
  char release[sizeof("10.7.99")];
  size_t len = sizeof(release);
  // sysctl will return ENOMEM if the release string is longer than sizeof(release)
  int ret = sysctl(mib, 2, release, &len, NULL, 0);
  // we only want to trigger this on OS X 10.6, on versions 10.6.8 or newer
  // Darwin version 10 corresponds to OS X version 10.6, version 11 is 10.7
  // http://en.wikipedia.org/wiki/Darwin_(operating_system)#Release_history
  if (ret == 0 && NS_CompareVersions(release, "10.8.0") >= 0 && NS_CompareVersions(release, "11") < 0) {
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
      CFRetain(mainBundle);

      CFStringRef bundleID = CFBundleGetIdentifier(mainBundle);
      if (bundleID) {
        CFRetain(bundleID);

        CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFBundleGetInfoDictionary(mainBundle);
        CFDictionarySetValue(dict, CFSTR("CFBundleIdentifier"), CFSTR("org.mozilla.firefox"));

        // Trigger a load of the quirks table for org.mozilla.firefox.
        // We use different function on 32/64bit because of how the APIs
        // behave to force a call to GetBugsForOurBundleIDFromCoreservicesd.
#ifdef __i386__
        ProcessSerialNumber psn;
        ::GetCurrentProcess(&psn);
#else
        SInt32 major;
        ::Gestalt(gestaltSystemVersionMajor, &major);
#endif

        // restore the original id
        dict = (CFMutableDictionaryRef)CFBundleGetInfoDictionary(mainBundle);
        CFDictionarySetValue(dict, CFSTR("CFBundleIdentifier"), bundleID);

        CFRelease(bundleID);
      }
      CFRelease(mainBundle);
    }
  }
}

#endif //MacQuirks_h__
