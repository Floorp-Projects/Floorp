package com.adjust.sdk;

public enum ActivityKind {
    UNKNOWN, SESSION, EVENT, CLICK, ATTRIBUTION;

    public static ActivityKind fromString(String string) {
        if ("session".equals(string)) {
            return SESSION;
        } else if ("event".equals(string)) {
            return EVENT;
        } else if ("click".equals(string)) {
            return CLICK;
        } else if ("attribution".equals(string)) {
            return ATTRIBUTION;
        } else {
            return UNKNOWN;
        }
    }

    @Override
    public String toString() {
        switch (this) {
            case SESSION:
                return "session";
            case EVENT:
                return "event";
            case CLICK:
                return "click";
            case ATTRIBUTION:
                return "attribution";
            default:
                return "unknown";
        }
    }
}
