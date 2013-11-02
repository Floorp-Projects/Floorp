/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.background.healthreport.ProfileInformationCache;

public class MockProfileInformationCache extends ProfileInformationCache {
  public MockProfileInformationCache(String profilePath) {
    super(profilePath);
  }

  public boolean isInitialized() {
    return this.initialized;
  }
  public boolean needsWrite() {
    return this.needsWrite;
  }
  public File getFile() {
    return this.file;
  }

  public void writeJSON(JSONObject toWrite) throws IOException {
    writeToFile(toWrite);
  }

  public JSONObject readJSON() throws FileNotFoundException, JSONException {
    return readFromFile();
  }

  public void setInitialized(final boolean initialized) {
    this.initialized = initialized;
  }
}
