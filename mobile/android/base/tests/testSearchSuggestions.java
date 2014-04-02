package org.mozilla.gecko.tests;

import java.util.ArrayList;
import java.util.HashMap;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.home.BrowserSearch;
import org.mozilla.gecko.home.SuggestClient;

import android.app.Activity;
import android.support.v4.app.Fragment;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * Test for search suggestions.
 * Sends queries from AwesomeBar input and verifies that suggestions match
 * expected values.
 */
public class testSearchSuggestions extends BaseTest {
    private static final int SUGGESTION_MAX = 3;
    private static final int SUGGESTION_TIMEOUT = 5000;
    private static final String TEST_QUERY = "foo barz";
    private static final String SUGGESTION_TEMPLATE = "/robocop/robocop_suggestions.sjs?query=__searchTerms__";

    public void testSearchSuggestions() {
        blockForGeckoReady();

        // Map of expected values. See robocop_suggestions.sjs.
        final HashMap<String, ArrayList<String>> suggestMap = new HashMap<String, ArrayList<String>>();
        buildSuggestMap(suggestMap);

        focusUrlBar();

        for (int i = 0; i < TEST_QUERY.length(); i++) {
            Actions.EventExpecter enginesEventExpecter = null;

            if (i == 0) {
                enginesEventExpecter = mActions.expectGeckoEvent("SearchEngines:Data");
            }

            mActions.sendKeys(TEST_QUERY.substring(i, i+1));

            // The BrowserSearch UI only shows up once a non-empty
            // search term is entered
            if (enginesEventExpecter != null) {
                connectSuggestClient(getActivity());
                enginesEventExpecter.blockForEvent();
                enginesEventExpecter.unregisterListener();
                enginesEventExpecter = null;
            }

            final String query = TEST_QUERY.substring(0, i+1);
            boolean success = waitForTest(new BooleanTest() {
                @Override
                public boolean test() {
                    // get the first suggestion row
                    ViewGroup suggestionGroup = (ViewGroup) getActivity().findViewById(R.id.suggestion_layout);
                    if (suggestionGroup == null)
                        return false;

                    ArrayList<String> expected = suggestMap.get(query);
                    for (int i = 0; i < expected.size(); i++) {
                        View queryChild = suggestionGroup.getChildAt(i);
                        if (queryChild == null || queryChild.getVisibility() == View.GONE)
                            return false;

                        String suggestion = ((TextView) queryChild.findViewById(R.id.suggestion_text)).getText().toString();
                        if (!suggestion.equals(expected.get(i)))
                            return false;
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

    private void connectSuggestClient(final Activity activity) {
        waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                final Fragment browserSearch = getBrowserSearch();
                return (browserSearch != null);
            }
        }, SUGGESTION_TIMEOUT);

        final BrowserSearch browserSearch = (BrowserSearch) getBrowserSearch();

        final String suggestTemplate = getAbsoluteRawUrl(SUGGESTION_TEMPLATE);
        final SuggestClient client = new SuggestClient(activity, suggestTemplate,
                SUGGESTION_TIMEOUT);
        browserSearch.setSuggestClient(client);
    }
}

