/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is places test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * This is a mock Link object which can be used in tests.
 */

#ifndef mock_Link_h__
#define mock_Link_h__

#include "mozilla/dom/Link.h"

class mock_Link : public mozilla::dom::Link
{
public:
  NS_DECL_ISUPPORTS

  mock_Link(void (*aHandlerFunction)(nsLinkState),
            bool aRunNextTest = true)
  : mozilla::dom::Link(nsnull)
  , mHandler(aHandlerFunction)
  , mRunNextTest(aRunNextTest)
  {
    // Create a cyclic ownership, so that the link will be released only
    // after its status has been updated.  This will ensure that, when it should
    // run the next test, it will happen at the end of the test function, if
    // the link status has already been set before.  Indeed the link status is
    // updated on a separate connection, thus may happen at any time.
    mDeathGrip = this;
  }

  virtual void SetLinkState(nsLinkState aState)
  {
    // Notify our callback function.
    mHandler(aState);

    // Break the cycle so the object can be destroyed.
    mDeathGrip = 0;
  }

  ~mock_Link() {
    // Run the next test if we are supposed to.
    if (mRunNextTest) {
      run_next_test();
    }
  }

private:
  void (*mHandler)(nsLinkState);
  bool mRunNextTest;
  nsRefPtr<Link> mDeathGrip;
};

NS_IMPL_ISUPPORTS1(
  mock_Link,
  mozilla::dom::Link
)

////////////////////////////////////////////////////////////////////////////////
//// Needed Link Methods

namespace mozilla {
namespace dom {

Link::Link(Element* aElement)
: mLinkState(mozilla::dom::Link::defaultState)
, mRegistered(false)
, mElement(aElement)
{
}

Link::~Link()
{
}

nsLinkState
Link::GetLinkState() const
{
  NS_NOTREACHED("Unexpected call to Link::GetLinkState");
  return eLinkState_Unknown; // suppress compiler warning
}

void
Link::SetLinkState(nsLinkState aState)
{
  NS_NOTREACHED("Unexpected call to Link::SetLinkState");
}

void
Link::ResetLinkState(bool aNotify)
{
  NS_NOTREACHED("Unexpected call to Link::ResetLinkState");
}

already_AddRefed<nsIURI>
Link::GetURI() const 
{
  NS_NOTREACHED("Unexpected call to Link::GetURI");
  return nsnull; // suppress compiler warning
}

} // namespace dom
} // namespace mozilla

#endif // mock_Link_h__
