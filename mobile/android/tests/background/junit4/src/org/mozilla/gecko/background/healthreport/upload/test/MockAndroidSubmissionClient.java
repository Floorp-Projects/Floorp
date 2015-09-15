/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload.test;

import android.content.Context;
import android.content.SharedPreferences;

import org.mozilla.gecko.background.healthreport.Environment;
import org.mozilla.gecko.background.healthreport.Environment.UIType;
import org.mozilla.gecko.background.healthreport.EnvironmentBuilder.ConfigurationProvider;
import org.mozilla.gecko.background.healthreport.HealthReportStorage;
import org.mozilla.gecko.background.healthreport.ProfileInformationCache;
import org.mozilla.gecko.background.healthreport.test.HealthReportStorageStub;
import org.mozilla.gecko.background.healthreport.upload.AndroidSubmissionClient;

public class MockAndroidSubmissionClient extends AndroidSubmissionClient {
  public MockAndroidSubmissionClient(final Context context, final SharedPreferences prefs,
      final String profilePath) {
    super(context, prefs, profilePath, new MockConfigurationProvider());
  }

  public static class MockConfigurationProvider implements ConfigurationProvider {
    @Override
    public boolean hasHardwareKeyboard() {
      return false;
    }

    @Override
    public UIType getUIType() {
      return UIType.DEFAULT;
    }

    @Override
    public int getUIModeType() {
      return 0;
    }

    @Override
    public int getScreenLayoutSize() {
      return 0;
    }

    @Override
    public int getScreenXInMM() {
      return 100;
    }

    @Override
    public int getScreenYInMM() {
      return 140;
    }

  }

  public class MockSubmissionsTracker extends SubmissionsTracker {
    public MockSubmissionsTracker(final HealthReportStorage storage, final long localTime,
        final boolean hasUploadBeenRequested) {
      super(storage, localTime, hasUploadBeenRequested);
    }

    // Override so we don't touch storage to register the current env.
    // The returned id value does not matter much.
    @Override
    public int registerCurrentEnvironment() {
      return 0;
    }

    // Override so we don't touch storage to get cache. Only getCurrentEnviroment uses the cache,
    // which we override, so we're free to return null.
    @Override
    public ProfileInformationCache getProfileInformationCache() {
      return null;
    }

    @Override
    public TrackingGenerator getGenerator() {
      return new MockTrackingGenerator();
    }

    public class MockTrackingGenerator extends TrackingGenerator {
      // Override so it doesn't fail in the constructor when touching the storage stub (below).
      @Override
      protected Environment getCurrentEnvironment() {
        return new Environment() {
          @Override
          public int register() {
            return 0;
          }
        };
      }
    }
  }

  /**
   * Mocked HealthReportStorage class for use within the MockAndroidSubmissionClient and its inner
   * classes, to prevent access to real storage.
   */
  public static class MockHealthReportStorage extends HealthReportStorageStub {
    // Ensures a unique Field ID for SubmissionsFieldName.getID().
    @Override
    public Field getField(String mName, int mVersion, String fieldName) {
      return new Field(mName, mVersion, fieldName, 0) {
        @Override
        public int getID() throws IllegalStateException {
          return fieldName.hashCode();
        }
      };
    }

    // Called in the SubmissionsTracker constructor.
    @Override
    public int getDay(final long millis) {
      return 0;
    }
  }
}
