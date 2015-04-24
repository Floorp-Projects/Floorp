//
//  ActivityPackage.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import java.io.Serializable;
import java.util.Map;

public class ActivityPackage implements Serializable {
    private static final long serialVersionUID = -35935556512024097L;

    // data
    private String path;
    private String clientSdk;
    private Map<String, String> parameters;

    // logs
    private ActivityKind activityKind;
    private String suffix;

    public String getPath() {
        return path;
    }

    public void setPath(String path) {
        this.path = path;
    }

    public String getClientSdk() {
        return clientSdk;
    }

    public void setClientSdk(String clientSdk) {
        this.clientSdk = clientSdk;
    }

    public Map<String, String> getParameters() {
        return parameters;
    }

    public void setParameters(Map<String, String> parameters) {
        this.parameters = parameters;
    }

    public ActivityKind getActivityKind() {
        return activityKind;
    }

    public void setActivityKind(ActivityKind activityKind) {
        this.activityKind = activityKind;
    }

    public String getSuffix() {
        return suffix;
    }

    public void setSuffix(String suffix) {
        this.suffix = suffix;
    }

    public String toString() {
        return String.format("%s%s", activityKind.toString(), suffix);
    }

    public String getExtendedString() {
        StringBuilder builder = new StringBuilder();
        builder.append(String.format("Path:      %s\n", path));
        builder.append(String.format("ClientSdk: %s\n", clientSdk));

        if (parameters != null) {
            builder.append("Parameters:");
            for (Map.Entry<String, String> entry : parameters.entrySet()) {
                builder.append(String.format("\n\t%-16s %s", entry.getKey(), entry.getValue()));
            }
        }
        return builder.toString();
    }

    protected String getSuccessMessage() {
        try {
            return String.format("Tracked %s%s", activityKind.toString(), suffix);
        } catch (NullPointerException e) {
            return "Tracked ???";
        }
    }

    protected String getFailureMessage() {
        try {
            return String.format("Failed to track %s%s", activityKind.toString(), suffix);
        } catch (NullPointerException e) {
            return "Failed to track ???";
        }
    }
}
