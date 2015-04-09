package com.adjust.sdk;

import org.json.JSONObject;

import java.io.Serializable;

/**
 * Created by pfms on 07/11/14.
 */
public class AdjustAttribution implements Serializable {
    private static final long serialVersionUID = 1L;

    public String trackerToken;
    public String trackerName;
    public String network;
    public String campaign;
    public String adgroup;
    public String creative;

    public static AdjustAttribution fromJson(JSONObject jsonObject) {
        if (jsonObject == null) return null;

        AdjustAttribution attribution = new AdjustAttribution();

        attribution.trackerToken = jsonObject.optString("tracker_token", null);
        attribution.trackerName = jsonObject.optString("tracker_name", null);
        attribution.network = jsonObject.optString("network", null);
        attribution.campaign = jsonObject.optString("campaign", null);
        attribution.adgroup = jsonObject.optString("adgroup", null);
        attribution.creative = jsonObject.optString("creative", null);

        return attribution;
    }

    public boolean equals(Object other) {
        if (other == this) return true;
        if (other == null) return false;
        if (getClass() != other.getClass()) return false;
        AdjustAttribution otherAttribution = (AdjustAttribution) other;

        if (!equalString(trackerToken,  otherAttribution.trackerToken)) return false;
        if (!equalString(trackerName,   otherAttribution.trackerName)) return false;
        if (!equalString(network,       otherAttribution.network)) return false;
        if (!equalString(campaign,      otherAttribution.campaign)) return false;
        if (!equalString(adgroup,       otherAttribution.adgroup)) return false;
        if (!equalString(creative,      otherAttribution.creative)) return false;
        return true;
    }

    private boolean equalString(String first, String second) {
        if (first == null || second == null) {
            return first == null && second == null;
        }
        return first.equals(second);
    }

    @Override
    public String toString() {
        return String.format("tt:%s tn:%s net:%s cam:%s adg:%s cre:%s",
                trackerToken, trackerName, network, campaign, adgroup, creative);
    }
}
