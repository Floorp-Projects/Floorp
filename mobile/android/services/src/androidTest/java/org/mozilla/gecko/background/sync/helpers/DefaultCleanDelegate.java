/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;

public class DefaultCleanDelegate extends DefaultDelegate implements RepositorySessionCleanDelegate {

  @Override
  public void onCleaned(Repository repo) {
    performNotify("Default begin delegate hit.", null);
  }

  @Override
  public void onCleanFailed(Repository repo, Exception ex) {
    performNotify("Clean failed.", ex);
  }

}
