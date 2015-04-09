//
//  ActivityState.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectInputStream.GetField;
import java.io.Serializable;
import java.util.Calendar;
import java.util.Locale;

public class ActivityState implements Serializable, Cloneable {
    private static final long serialVersionUID = 9039439291143138148L;
    private transient String readErrorMessage = "Unable to read '%s' field in migration device with message (%s)";
    private transient ILogger logger;

    // persistent data
    protected String uuid;
    protected boolean enabled;
    protected boolean askingAttribution;

    // global counters
    protected int eventCount;
    protected int sessionCount;

    // session attributes
    protected int subsessionCount;
    protected long sessionLength;   // all durations in milliseconds
    protected long timeSpent;
    protected long lastActivity;    // all times in milliseconds since 1970

    protected long lastInterval;

    protected ActivityState() {
        logger = AdjustFactory.getLogger();
        // create UUID for new devices
        uuid = Util.createUuid();
        enabled = true;
        askingAttribution = false;

        eventCount = 0; // no events yet
        sessionCount = 0; // the first session just started
        subsessionCount = -1; // we don't know how many subsessions this first  session will have
        sessionLength = -1; // same for session length and time spent
        timeSpent = -1; // this information will be collected and attached to the next session
        lastActivity = -1;
        lastInterval = -1;
    }

    protected void resetSessionAttributes(long now) {
        subsessionCount = 1; // first subsession
        sessionLength = 0; // no session length yet
        timeSpent = 0; // no time spent yet
        lastActivity = now;
        lastInterval = -1;
    }

    @Override
    public String toString() {
        return String.format(Locale.US,
                "ec:%d sc:%d ssc:%d sl:%.1f ts:%.1f la:%s uuid:%s",
                eventCount, sessionCount, subsessionCount,
                sessionLength / 1000.0, timeSpent / 1000.0,
                stamp(lastActivity), uuid);
    }

    @Override
    public ActivityState clone() {
        try {
            return (ActivityState) super.clone();
        } catch (CloneNotSupportedException e) {
            return null;
        }
    }


    private void readObject(ObjectInputStream stream) throws IOException, ClassNotFoundException {
        GetField fields = stream.readFields();

        eventCount = readIntField(fields, "eventCount", 0);
        sessionCount = readIntField(fields, "sessionCount", 0);
        subsessionCount = readIntField(fields, "subsessionCount", -1);
        sessionLength = readLongField(fields, "sessionLength", -1l);
        timeSpent = readLongField(fields, "timeSpent", -1l);
        lastActivity = readLongField(fields, "lastActivity", -1l);
        lastInterval = readLongField(fields, "lastInterval", -1l);

        // new fields
        uuid = readStringField(fields, "uuid", null);
        enabled = readBooleanField(fields, "enabled", true);
        askingAttribution = readBooleanField(fields, "askingAttribution", false);

        // create UUID for migrating devices
        if (uuid == null) {
            uuid = Util.createUuid();
        }
    }

    private String readStringField(GetField fields, String name, String defaultValue) {
        try {
            return (String) fields.get(name, defaultValue);
        } catch (Exception e) {
            logger.debug(readErrorMessage, name, e.getMessage());
            return defaultValue;
        }
    }

    private boolean readBooleanField(GetField fields, String name, boolean defaultValue) {
        try {
            return fields.get(name, defaultValue);
        } catch (Exception e) {
            logger.debug(readErrorMessage, name, e.getMessage());
            return defaultValue;
        }
    }

    private int readIntField(GetField fields, String name, int defaultValue) {
        try {
            return fields.get(name, defaultValue);
        } catch (Exception e) {
            logger.debug(readErrorMessage, name, e.getMessage());
            return defaultValue;
        }
    }

    private long readLongField(GetField fields, String name, long defaultValue) {
        try {
            return fields.get(name, defaultValue);
        } catch (Exception e) {
            logger.debug(readErrorMessage, name, e.getMessage());
            return defaultValue;
        }
    }

    private static String stamp(long dateMillis) {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(dateMillis);
        return String.format(Locale.US,
                "%02d:%02d:%02d",
                calendar.HOUR_OF_DAY,
                calendar.MINUTE,
                calendar.SECOND);
    }
}
