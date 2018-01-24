/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSShutDown_h
#define nsNSSShutDown_h

// This is the vestigial remains of the old NSS shutdown tracking
// infrastructure. It will be removed entirely in bug 1421084 when we've
// demonstrated that shutting down NSS only when all non-main-threads have been
// joined is feasible (and beneficial).

class nsNSSShutDownObject
{
public:
  nsNSSShutDownObject() {}
  virtual ~nsNSSShutDownObject() {}
};

#endif // nsNSSShutDown_h
