/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
package org.mozilla.gecko.sync.repositories.domain.test;


import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;

import static org.junit.Assert.assertEquals;

import java.io.IOException;

@RunWith(TestRunner.class)
public class TestPasswordRecord {
    @Test
    public void testInitFromPayload() {
        ExtendedJSONObject o = null;
        try {
            o = new ExtendedJSONObject("{" +
                    "\"hostname\": \"https://example.com\",\n" +
                    "\"formSubmitURL\": \"https://example.com/login\",\n" +
                    "\"httpRealm\": null,\n" +
                    "\"username\": \"johndoe\",\n" +
                    "\"password\": \"p4ssw0rd\",\n" +
                    "\"usernameField\": \"user\",\n" +
                    "\"passwordField\": \"pass\",\n" +
                    // Above the max sane value to ensure we don't regress 1404044
                    "\"timeLastUsed\": 18446732429235952000" +
                    "}");
        } catch (IOException e) {
            Assert.fail("Somehow got an IOException when parsing json from a string D:");
        } catch (NonObjectJSONException e) {
            Assert.fail(e.getMessage());
        }
        PasswordRecord p = new PasswordRecord();
        p.initFromPayload(o);
        assertEquals(p.encryptedUsername, o.getString("username"));
        assertEquals(p.encryptedPassword, o.getString("password"));
        assertEquals(p.usernameField, o.getString("usernameField"));
        assertEquals(p.passwordField, o.getString("passwordField"));
        assertEquals(p.formSubmitURL, o.getString("formSubmitURL"));
        assertEquals(p.hostname, o.getString("hostname"));
        assertEquals(p.httpRealm, null);
    }
}
