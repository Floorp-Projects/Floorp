package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.GeckoHelper;

import android.view.View;

/**
 * Tests that verify the behavior of back button in edit mode.
 */
public class testBackButtonInEditMode extends UITest {
    public void testBackButtonInEditMode() {
        GeckoHelper.blockForReady();

        // Verify back button behavior for edit mode.
        mToolbar.enterEditingMode()
                .assertIsUrlEditTextSelected();
        testBackPressInEditMode();
        testExitUsingBackButton();

        // Verify back button behavior in edit mode after input.
        mToolbar.enterEditingMode()
                .enterUrl("dummy")
                .assertIsUrlEditTextSelected();
        testBackPressInEditMode();
        testExitUsingBackButton();

        // Verify the swipe behavior in edit mode.
        mToolbar.enterEditingMode()
                .assertIsUrlEditTextSelected();
        mAboutHome.swipeToPanelOnLeft();
        mToolbar.assertIsUrlEditTextNotSelected()
                .assertIsEditing();
        testExitUsingBackButton();
    }

    public void testBackPressInEditMode() {
        // Press back button and verify URLEditText is not selected.
        getSolo().goBack();
        mToolbar.assertIsUrlEditTextNotSelected()
                .assertIsEditing();
    }

    public void testExitUsingBackButton() {
        getSolo().goBack();
        mToolbar.assertIsNotEditing();
    }
}
