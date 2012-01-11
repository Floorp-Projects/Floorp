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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll <jvoll@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

public abstract class AndroidBrowserRepository extends Repository {

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate, Context context) {
    new CreateSessionThread(delegate, context).start();
  }

  @Override
  public void clean(boolean success, RepositorySessionCleanDelegate delegate, Context context) {
    // Only clean deleted records if success
    if (success) {
      new CleanThread(delegate, context).start();
    }
  }

  class CleanThread extends Thread {
    private RepositorySessionCleanDelegate delegate;
    private Context context;

    public CleanThread(RepositorySessionCleanDelegate delegate, Context context) {
      if (context == null) {
        throw new IllegalArgumentException("context is null");
      }
      this.delegate = delegate;
      this.context = context;
    }

    public void run() {
      try {
        getDataAccessor(context).purgeDeleted();
      } catch (NullCursorException e) {
        delegate.onCleanFailed(AndroidBrowserRepository.this, e);
        return;
      } catch (Exception e) {
        delegate.onCleanFailed(AndroidBrowserRepository.this, e);
        return;
      }
      delegate.onCleaned(AndroidBrowserRepository.this);
    }
  }

  protected abstract AndroidBrowserRepositoryDataAccessor getDataAccessor(Context context);
  protected abstract void sessionCreator(RepositorySessionCreationDelegate delegate, Context context);

  class CreateSessionThread extends Thread {
    private RepositorySessionCreationDelegate delegate;
    private Context context;

    public CreateSessionThread(RepositorySessionCreationDelegate delegate, Context context) {
      if (context == null) {
        throw new IllegalArgumentException("context is null.");
      }
      this.delegate = delegate;
      this.context = context;
    }

    public void run() {
      sessionCreator(delegate, context);
    }
  }

}
