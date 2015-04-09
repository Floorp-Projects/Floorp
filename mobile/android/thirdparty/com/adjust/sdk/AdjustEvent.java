package com.adjust.sdk;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by pfms on 05/11/14.
 */
public class AdjustEvent {
    String eventToken;
    Double revenue;
    String currency;
    Map<String, String> callbackParameters;
    Map<String, String> partnerParameters;

    private static ILogger logger = AdjustFactory.getLogger();

    public AdjustEvent(String eventToken) {
        if (!checkEventToken(eventToken, logger)) return;

        this.eventToken = eventToken;
    }

    public void setRevenue(double revenue, String currency) {
        if (!checkRevenue(revenue, currency)) return;

        this.revenue = revenue;
        this.currency = currency;
    }

    public void addCallbackParameter(String key, String value) {
        if (!isValidParameter(key, "key", "Callback")) return;
        if (!isValidParameter(value, "value", "Callback")) return;

        if (callbackParameters == null) {
            callbackParameters = new HashMap<String, String>();
        }

        String previousValue = callbackParameters.put(key, value);

        if (previousValue != null) {
            logger.warn("key %s was overwritten", key);
        }
    }

    public void addPartnerParameter(String key, String value) {
        if (!isValidParameter(key, "key", "Partner")) return;
        if (!isValidParameter(value, "value", "Partner")) return;

        if (partnerParameters == null) {
            partnerParameters = new HashMap<String, String>();
        }

        String previousValue = partnerParameters.put(key, value);

        if (previousValue != null) {
            logger.warn("key %s was overwritten", key);
        }
    }

    public boolean isValid() {
        return eventToken != null;
    }

    private static boolean checkEventToken(String eventToken, ILogger logger) {
        if (eventToken == null) {
            logger.error("Missing Event Token");
            return false;
        }
        if (eventToken.length() != 6) {
            logger.error("Malformed Event Token '%s'", eventToken);
            return false;
        }
        return true;
    }

    private boolean checkRevenue(Double revenue, String currency) {
        if (revenue != null) {
            if (revenue < 0.0) {
                logger.error("Invalid amount %.4f", revenue);
                return false;
            }

            if (currency == null) {
                logger.error("Currency must be set with revenue");
                return false;
            }
            if (currency == "") {
                logger.error("Currency is empty");
                return false;
            }

        } else if (currency != null) {
            logger.error("Revenue must be set with currency");
            return false;
        }
        return true;
    }

    private boolean isValidParameter(String attribute, String attributeType, String parameterName) {
        if (attribute == null) {
            logger.error("%s parameter %s is missing", parameterName, attributeType);
            return false;
        }
        if (attribute == "") {
            logger.error("%s parameter %s is empty", parameterName, attributeType);
            return false;
        }

        return true;
    }
}
