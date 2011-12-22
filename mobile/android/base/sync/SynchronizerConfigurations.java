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
 *   Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map.Entry;
import java.util.Set;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import android.os.Bundle;

public class SynchronizerConfigurations {
  private static final int CONFIGURATION_VERSION = 1;
  private HashMap<String, SynchronizerConfiguration> engines;

  protected HashMap<String, SynchronizerConfiguration> enginesMapFromBundleV1(Bundle engineBundle) throws IOException, ParseException, NonObjectJSONException {
    HashMap<String, SynchronizerConfiguration> engines = new HashMap<String, SynchronizerConfiguration>();
    Set<String> keySet = engineBundle.keySet();
    for (String engine : keySet) {
      String[] values = engineBundle.getStringArray(engine);
      String syncID                        = values[0];
      RepositorySessionBundle remoteBundle = new RepositorySessionBundle(values[1]);
      RepositorySessionBundle localBundle  = new RepositorySessionBundle(values[2]);
      engines.put(engine, new SynchronizerConfiguration(syncID, remoteBundle, localBundle));
    }
    return engines;
  }

  public SynchronizerConfigurations(Bundle bundle) throws IOException, ParseException, NonObjectJSONException, UnknownSynchronizerConfigurationVersionException {
    if (bundle == null) {
      bundle = new Bundle();
    }

    Bundle engineBundle = bundle.getBundle("engines");
    if (engineBundle == null) {
      this.initializeDefaultEnginesMap();
      return;
    }
    int version = engineBundle.getInt("version");
    if (version == 0) {
      // No data in the bundle.
      this.initializeDefaultEnginesMap();
      return;
    }
    if (version == 1) {
      engines = enginesMapFromBundleV1(engineBundle);
      return;
    }
    throw new UnknownSynchronizerConfigurationVersionException(version);
  }

  private void initializeDefaultEnginesMap() {
    engines = new HashMap<String, SynchronizerConfiguration>();
  }

  public void fillBundle(Bundle bundle) {
    Bundle contents = new Bundle();
    for (Entry<String, SynchronizerConfiguration> entry : engines.entrySet()) {
      contents.putStringArray(entry.getKey(), entry.getValue().toStringValues());
    }
    contents.putInt("version", CONFIGURATION_VERSION);
    bundle.putBundle("engines", contents);
  }

  public SynchronizerConfiguration forEngine(String engineName) {
    return engines.get(engineName);
  }
}
