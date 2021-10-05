package org.mozilla.geckoview.test;

import org.junit.rules.ExternalResource;
import org.junit.rules.TemporaryFolder;
import org.mozilla.geckoview.test.rule.TestHarnessException;

import java.io.File;
import java.io.IOException;

/** Lazily provides a temporary profile folder for tests. */
public class TemporaryProfileRule extends ExternalResource {
    TemporaryFolder mTemporaryFolder;
    File mProfileFolder;

    @Override
    protected void after() {
        if (mTemporaryFolder != null) {
            mTemporaryFolder.delete();
            mProfileFolder = null;
        }
    }

    public File get() {
        if (mProfileFolder == null) {
            mTemporaryFolder = new TemporaryFolder();
            try {
                mTemporaryFolder.create();
                mProfileFolder = mTemporaryFolder.newFolder("test-profile");
            } catch (IOException ex) {
                throw new TestHarnessException(ex);
            }
        }

        return mProfileFolder;
    }
}
