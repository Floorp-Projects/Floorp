//
//  Constants.java
//  Adjust
//
//  Created by keyboardsurfer on 2013-11-08.
//  Copyright (c) 2012-2014 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import java.util.Arrays;
import java.util.List;

/**
 * @author keyboardsurfer
 * @since 8.11.13
 */
public interface Constants {
    int ONE_SECOND = 1000;
    int ONE_MINUTE = 60 * ONE_SECOND;
    int THIRTY_MINUTES = 30 * ONE_MINUTE;

    int CONNECTION_TIMEOUT = Constants.ONE_MINUTE;
    int SOCKET_TIMEOUT = Constants.ONE_MINUTE;

    String BASE_URL = "https://app.adjust.com";
    String SCHEME = "https";
    String AUTHORITY = "app.adjust.com";
    String CLIENT_SDK = "android4.0.0";
    String LOGTAG = "Adjust";

    String ACTIVITY_STATE_FILENAME = "AdjustIoActivityState";
    String ATTRIBUTION_FILENAME = "AdjustAttribution";

    String MALFORMED = "malformed";
    String SMALL = "small";
    String NORMAL = "normal";
    String LONG = "long";
    String LARGE = "large";
    String XLARGE = "xlarge";
    String LOW = "low";
    String MEDIUM = "medium";
    String HIGH = "high";
    String REFERRER = "referrer";

    String ENCODING = "UTF-8";
    String MD5 = "MD5";
    String SHA1 = "SHA-1";

    // List of known plugins, possibly not active
    List<String> PLUGINS = Arrays.asList();
}
