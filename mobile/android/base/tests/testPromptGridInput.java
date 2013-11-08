package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

import android.graphics.drawable.Drawable;
import android.widget.EditText;
import android.widget.CheckedTextView;
import android.widget.TextView;
import android.text.InputType;
import android.util.DisplayMetrics;
import android.view.View;
import android.util.Log;

import org.json.JSONObject;

public class testPromptGridInput extends BaseTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    protected int index = 1;
    public void testPromptGridInput() {
        blockForGeckoReady();

        test(1);

        testGridItem("Icon 1");
        testGridItem("Icon 2");
        testGridItem("Icon 3");
        testGridItem("Icon 4");
        testGridItem("Icon 5");
        testGridItem("Icon 6");
        testGridItem("Icon 7");
        testGridItem("Icon 8");
        testGridItem("Icon 9");
        testGridItem("Icon 10");
        testGridItem("Icon 11");

        mSolo.clickOnText("Icon 11");
        mSolo.clickOnText("OK");

        mAsserter.ok(waitForText("PASS"), "test passed", "PASS");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
    }

    public void testGridItem(String title) {
        // Force the list to scroll if necessary
        mSolo.waitForText(title, 1, 500, true);
        mAsserter.ok(waitForText(title), "Found grid item", title);
    }

    public void test(final int num) {
        // Load about:blank between each test to ensure we reset state
        loadUrl("about:blank");
        mAsserter.ok(waitForText("about:blank"), "Loaded blank page", "page title match");

        loadUrl("chrome://roboextender/content/robocop_prompt_gridinput.html#test" + num);
    }
}
