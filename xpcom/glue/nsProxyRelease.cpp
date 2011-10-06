/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

class nsProxyReleaseEvent : public nsRunnable
{
public:
    nsProxyReleaseEvent(nsISupports *doomed)
        : mDoomed(doomed) {
    }

    NS_IMETHOD Run()
    {
        mDoomed->Release();
        return NS_OK;
    }

private:
    nsISupports *mDoomed;
};

nsresult
NS_ProxyRelease(nsIEventTarget *target, nsISupports *doomed,
                bool alwaysProxy)
{
    nsresult rv;

    if (!target) {
        NS_RELEASE(doomed);
        return NS_OK;
    }

    if (!alwaysProxy) {
        bool onCurrentThread = false;
        rv = target->IsOnCurrentThread(&onCurrentThread);
        if (NS_SUCCEEDED(rv) && onCurrentThread) {
            NS_RELEASE(doomed);
            return NS_OK;
        }
    }

    nsRefPtr<nsIRunnable> ev = new nsProxyReleaseEvent(doomed);
    if (!ev) {
        // we do not release doomed here since it may cause a delete on the
        // wrong thread.  better to leak than crash. 
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = target->Dispatch(ev, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to post proxy release event");
        // again, it is better to leak the doomed object than risk crashing as
        // a result of deleting it on the wrong thread.
    }
    return rv;
}
