/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.setup.InvalidSyncKeyException;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestSyncKeyVerification {

  private int[] mutateIndices;
  private final String validBasicKey = "abcdefghijkmnpqrstuvwxyz23"; // 26 char, valid characters.
  char[] invalidChars = new char[] { '1', 'l', 'o', '0' };

  @Before
  public void setUp() {
    // Generate indicies to mutate.
    mutateIndices = generateMutationArray();
  }

  @Test
  public void testValidKey() {
    try {
      ActivityUtils.validateSyncKey(validBasicKey);
    } catch (InvalidSyncKeyException e) {
      fail("Threw unexpected InvalidSyncKeyException.");
    }
  }

  @Test
  public void testHyphenationSuccess() {
    StringBuilder sb = new StringBuilder();
    int prev = 0;
    for (int i : mutateIndices) {
      sb.append(validBasicKey.substring(prev, i));
      sb.append("-");
      prev = i;
    }
    sb.append(validBasicKey.substring(prev));
    String hString = sb.toString();
    try {
      ActivityUtils.validateSyncKey(hString);
    } catch (InvalidSyncKeyException e) {
      fail("Failed validation with hypenation.");
    }
  }

  @Test
  public void testCapitalizationSuccess() {

    char[] mutatedKey = validBasicKey.toCharArray();
    for (int i : mutateIndices) {
      mutatedKey[i] = Character.toUpperCase(validBasicKey.charAt(i));
    }
    String mKey = new String(mutatedKey);
    try {
      ActivityUtils.validateSyncKey(mKey);
    } catch (InvalidSyncKeyException e) {
      fail("Failed validation with uppercasing.");
    }
  }

  @Test (expected = InvalidSyncKeyException.class)
  public void testInvalidCharFailure() throws InvalidSyncKeyException {
    char[] mutatedKey = validBasicKey.toCharArray();
    for (int i : mutateIndices) {
      mutatedKey[i] = invalidChars[i % invalidChars.length];
    }
    ActivityUtils.validateSyncKey(mutatedKey.toString());
  }

  private int[] generateMutationArray() {
    // Hardcoded; change if desired?
    return new int[] { 2, 4, 5, 9, 16 };
  }
}
