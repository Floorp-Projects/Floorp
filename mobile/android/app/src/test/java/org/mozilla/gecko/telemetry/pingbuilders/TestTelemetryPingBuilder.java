/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

/**
 * Unit test methods of the {@link TelemetryPingBuilder} class.
 */
@RunWith(TestRunner.class)
public class TestTelemetryPingBuilder {
    @Test
    public void testMandatoryFieldsNone() {
        final NoMandatoryFieldsBuilder builder = new NoMandatoryFieldsBuilder();
        builder.setNonMandatoryField();
        assertNotNull("Builder does not throw and returns a non-null value", builder.build());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMandatoryFieldsMissing() {
        final MandatoryFieldsBuilder builder = new MandatoryFieldsBuilder();
        builder.setNonMandatoryField()
                .build(); // should throw
    }

    @Test
    public void testMandatoryFieldsIncluded() {
        final MandatoryFieldsBuilder builder = new MandatoryFieldsBuilder();
        builder.setNonMandatoryField()
                .setMandatoryField();
        assertNotNull("Builder does not throw and returns non-null value", builder.build());
    }

    private static class NoMandatoryFieldsBuilder extends TelemetryPingBuilder {
        @Override
        public String getDocType() {
            return "";
        }

        @Override
        public String[] getMandatoryFields() {
            return new String[0];
        }

        public NoMandatoryFieldsBuilder setNonMandatoryField() {
            payload.put("non-mandatory", true);
            return this;
        }
    }

    private static class MandatoryFieldsBuilder extends TelemetryPingBuilder {
        private static final String MANDATORY_FIELD = "mandatory-field";

        @Override
        public String getDocType() {
            return "";
        }

        @Override
        public String[] getMandatoryFields() {
            return new String[] {
                    MANDATORY_FIELD,
            };
        }

        public MandatoryFieldsBuilder setNonMandatoryField() {
            payload.put("non-mandatory", true);
            return this;
        }

        public MandatoryFieldsBuilder setMandatoryField() {
            payload.put(MANDATORY_FIELD, true);
            return this;
        }
    }
}
