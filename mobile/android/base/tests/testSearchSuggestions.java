package org.mozilla.gecko.tests;

import java.util.ArrayList;
import java.util.HashMap;

import org.mozilla.gecko.R;
import org.mozilla.gecko.SuggestClient;
import org.mozilla.gecko.home.BrowserSearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.jayway.android.robotium.solo.Condition;

/**
 * Test for search suggestions.
 * Sends queries from AwesomeBar input and verifies that suggestions match
 * expected values.
 */
public class testSearchSuggestions extends BaseTest {
    private static final int SUGGESTION_MAX = 3;
    private static final int SUGGESTION_TIMEOUT = 15000;

    private static final String TEST_QUERY = "foo barz";
    private static final String SUGGESTION_TEMPLATE = "/robocop/robocop_suggestions.sjs?query=__searchTerms__";

    public void testSearchSuggestions() {
        // Mock the search system.
        // The BrowserSearch UI only shows up once a non-empty
        // search term is entered, but we swizzle in a new factory beforehand.
        mockSuggestClientFactory();

        blockForGeckoReady();

        // Map of expected values. See robocop_suggestions.sjs.
        final HashMap<String, ArrayList<String>> suggestMap = new HashMap<String, ArrayList<String>>();
        buildSuggestMap(suggestMap);

        focusUrlBar();

        // At this point we rely on our swizzling having worked -- which relies
        // on us not having previously run a search.
        // The test will fail later if there's already a BrowserSearch object with a
        // suggest client set, so fail here.
        BrowserSearch browserSearch = (BrowserSearch) getBrowserSearch();
        mAsserter.ok(browserSearch == null ||
                     browserSearch.mSuggestClient == null,
                     "There is no existing search client.", "");

        // Now test the incremental suggestions.
        for (int i = 0; i < TEST_QUERY.length(); i++) {
            mActions.sendKeys(TEST_QUERY.substring(i, i+1));

            final String query = TEST_QUERY.substring(0, i+1);
            mSolo.waitForView(R.id.suggestion_text);
            boolean success = waitForCondition(new Condition() {
                @Override
                public boolean isSatisfied() {
                    // Get the first suggestion row.
                    ViewGroup suggestionGroup = (ViewGroup) getActivity().findViewById(R.id.suggestion_layout);
                    if (suggestionGroup == null) {
                        mAsserter.dumpLog("Fail: suggestionGroup is null.");
                        return false;
                    }

                    final ArrayList<String> expected = suggestMap.get(query);
                    for (int i = 0; i < expected.size(); i++) {
                        View queryChild = suggestionGroup.getChildAt(i);
                        if (queryChild == null || queryChild.getVisibility() == View.GONE) {
                            mAsserter.dumpLog("Fail: queryChild is null or GONE.");
                            return false;
                        }

                        String suggestion = ((TextView) queryChild.findViewById(R.id.suggestion_text)).getText().toString();
                        if (!suggestion.equals(expected.get(i))) {
                            mAsserter.dumpLog("Suggestion '" + suggestion + "' not equal to expected '" + expected.get(i) + "'.");
                            return false;
                        }
                    }

                    return true;
                }
            }, SUGGESTION_TIMEOUT);

            mAsserter.is(success, true, "Results for query '" + query + "' matched expected suggestions");
        }
    }

    private void buildSuggestMap(HashMap<String, ArrayList<String>> suggestMap) {
        // these values assume SUGGESTION_MAX = 3
        suggestMap.put("f",        new ArrayList<String>() {{ add("f"); add("facebook"); add("fandango"); add("frys"); }});
        suggestMap.put("fo",       new ArrayList<String>() {{ add("fo"); add("forever 21"); add("food network"); add("fox news"); }});
        suggestMap.put("foo",      new ArrayList<String>() {{ add("foo"); add("food network"); add("foothill college"); add("foot locker"); }});
        suggestMap.put("foo ",     new ArrayList<String>() {{ add("foo "); add("foo fighters"); add("foo bar"); add("foo bat"); }});
        suggestMap.put("foo b",    new ArrayList<String>() {{ add("foo b"); add("foo bar"); add("foo bat"); add("foo bay"); }});
        suggestMap.put("foo ba",   new ArrayList<String>() {{ add("foo ba"); add("foo bar"); add("foo bat"); add("foo bay"); }});
        suggestMap.put("foo bar",  new ArrayList<String>() {{ add("foo bar"); }});
        suggestMap.put("foo barz", new ArrayList<String>() {{ add("foo barz"); }});
    }

    private void mockSuggestClientFactory() {
        BrowserSearch.sSuggestClientFactory = new BrowserSearch.SuggestClientFactory() {
            @Override
            public SuggestClient getSuggestClient(Context context, String template, int timeout, int max) {
                final String suggestTemplate = getAbsoluteRawUrl(SUGGESTION_TEMPLATE);

                // This one uses our template, and also doesn't check for network accessibility.
                return new SuggestClient(context, suggestTemplate, SUGGESTION_TIMEOUT, Integer.MAX_VALUE, false);
            }
        };
    }
}

