/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsUtils_h
#define nsAlertsUtils_h

#include "nsIPrincipal.h"
#include "nsString.h"

class nsAlertsUtils final
{
private:
  nsAlertsUtils() = delete;

public:
  /**
   * Indicates whether an alert from |aPrincipal| should include the source
   * string and action buttons. Returns false if |aPrincipal| is |nullptr|, or
   * a system, expanded, or null principal.
   */
  static bool
  IsActionablePrincipal(nsIPrincipal* aPrincipal);

  /**
   * Sets |aHostPort| to the host and port from |aPrincipal|'s URI, or an
   * empty string if |aPrincipal| is not actionable.
   */
  static void
  GetSourceHostPort(nsIPrincipal* aPrincipal, nsAString& aHostPort);
};
#endif /* nsAlertsUtils_h */
