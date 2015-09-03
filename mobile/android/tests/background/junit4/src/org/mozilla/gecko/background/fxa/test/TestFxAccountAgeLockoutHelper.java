/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa.test;

import java.util.Calendar;

import org.junit.Assert;
import org.junit.Test;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.fxa.FxAccountConstants;

public class TestFxAccountAgeLockoutHelper {
  @Test
  public void testPassesAgeCheck() {
    Calendar today = Calendar.getInstance();
    int birthMonthIndex = today.get(Calendar.MONTH);
    int birthDate = today.get(Calendar.DATE);
    int birthYear = today.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertTrue("Minimum age as of today",
        FxAccountAgeLockoutHelper.passesAgeCheck(birthDate, birthMonthIndex, birthYear));

    Calendar yesterday = Calendar.getInstance();
    yesterday.add(Calendar.DATE, -1);
    birthMonthIndex = yesterday.get(Calendar.MONTH);
    birthDate = yesterday.get(Calendar.DATE);
    birthYear = yesterday.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertTrue("Minimum age more than by a day",
        FxAccountAgeLockoutHelper.passesAgeCheck(birthDate, birthMonthIndex, birthYear));

    Calendar tomorrow = Calendar.getInstance();
    tomorrow.add(Calendar.DATE, 1);
    birthMonthIndex = tomorrow.get(Calendar.MONTH);
    birthDate = tomorrow.get(Calendar.DATE);
    birthYear = tomorrow.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertFalse("Minimum age fails by a day",
        FxAccountAgeLockoutHelper.passesAgeCheck(birthDate, birthMonthIndex, birthYear));

    Calendar monthBefore = Calendar.getInstance();
    monthBefore.add(Calendar.MONTH, -1);
    birthMonthIndex = monthBefore.get(Calendar.MONTH);
    birthDate = monthBefore.get(Calendar.DATE);
    birthYear = monthBefore.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertTrue("Minimum age more than by a month",
        FxAccountAgeLockoutHelper.passesAgeCheck(birthDate, birthMonthIndex, birthYear));

    Calendar monthAfter = Calendar.getInstance();
    monthAfter.add(Calendar.MONTH, 1);
    birthMonthIndex = monthAfter.get(Calendar.MONTH);
    birthDate = monthAfter.get(Calendar.DATE);
    birthYear = monthAfter.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertFalse("Minimum age fails by a month",
        FxAccountAgeLockoutHelper.passesAgeCheck(birthDate, birthMonthIndex, birthYear));
  }

  @Test
  public void testIsMagicYear() {
    final Calendar today = Calendar.getInstance();
    int magicYear = today.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT;
    Assert.assertTrue("Passes magic year check: year is magic year",
        FxAccountAgeLockoutHelper.isMagicYear(magicYear));

    int beforeMagicYear = today.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT - 1;
    Assert.assertFalse("Fails magic year check: year before magic year",
        FxAccountAgeLockoutHelper.isMagicYear(beforeMagicYear));

    int afterMagicYear = today.get(Calendar.YEAR) - FxAccountConstants.MINIMUM_AGE_TO_CREATE_AN_ACCOUNT + 1;
    Assert.assertFalse("Fails magic year: year after magic year",
        FxAccountAgeLockoutHelper.isMagicYear(afterMagicYear));
  }
}
