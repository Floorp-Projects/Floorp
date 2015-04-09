//
//  Util.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2012-10-11.
//  Copyright (c) 2012-2014 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.content.Context;
import android.content.pm.PackageManager;

import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.HttpConnectionParams;
import org.apache.http.params.HttpParams;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.NotSerializableException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OptionalDataException;
import java.text.SimpleDateFormat;
import java.util.Locale;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Collects utility functions used by Adjust.
 */
public class Util {

    private static SimpleDateFormat dateFormat;
    private static final String DATE_FORMAT = "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'Z";

    protected static String createUuid() {
        return UUID.randomUUID().toString();
    }

    public static String quote(String string) {
        if (string == null) {
            return null;
        }

        Pattern pattern = Pattern.compile("\\s");
        Matcher matcher = pattern.matcher(string);
        if (!matcher.find()) {
            return string;
        }

        return String.format("'%s'", string);
    }

    public static String dateFormat(long date) {
        if (null == dateFormat) {
            dateFormat = new SimpleDateFormat(DATE_FORMAT, Locale.US);
        }
        return dateFormat.format(date);
    }

    public static String getPlayAdId(Context context) {
        return Reflection.getPlayAdId(context);
    }

    public static Boolean isPlayTrackingEnabled(Context context) {
        return Reflection.isPlayTrackingEnabled(context);
    }

    public static <T> T readObject(Context context, String filename, String objectName) {
        ILogger logger = AdjustFactory.getLogger();
        try {
            FileInputStream inputStream = context.openFileInput(filename);
            BufferedInputStream bufferedStream = new BufferedInputStream(inputStream);
            ObjectInputStream objectStream = new ObjectInputStream(bufferedStream);

            try {
                @SuppressWarnings("unchecked")
                T t = (T) objectStream.readObject();
                logger.debug("Read %s: %s", objectName, t);
                return t;
            } catch (ClassNotFoundException e) {
                logger.error("Failed to find %s class", objectName);
            } catch (OptionalDataException e) {
                /* no-op */
            } catch (IOException e) {
                logger.error("Failed to read %s object", objectName);
            } catch (ClassCastException e) {
                logger.error("Failed to cast %s object", objectName);
            } finally {
                objectStream.close();
            }

        } catch (FileNotFoundException e) {
            logger.verbose("%s file not found", objectName);
        } catch (Exception e) {
            logger.error("Failed to open %s file for reading (%s)", objectName, e);
        }

        return null;
    }

    public static <T> void writeObject(T object, Context context, String filename, String objectName) {
        ILogger logger = AdjustFactory.getLogger();
        try {
            FileOutputStream outputStream = context.openFileOutput(filename, Context.MODE_PRIVATE);
            BufferedOutputStream bufferedStream = new BufferedOutputStream(outputStream);
            ObjectOutputStream objectStream = new ObjectOutputStream(bufferedStream);

            try {
                objectStream.writeObject(object);
                logger.debug("Wrote %s: %s", objectName, object);
            } catch (NotSerializableException e) {
                logger.error("Failed to serialize %s", objectName);
            } finally {
                objectStream.close();
            }

        } catch (Exception e) {
            logger.error("Failed to open %s for writing (%s)", objectName, e);
        }
    }

    public static String parseResponse(HttpResponse httpResponse, ILogger logger) {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            httpResponse.getEntity().writeTo(out);
            out.close();
            String response = out.toString().trim();
            logger.verbose("Response: %s", response);
            return response;
        } catch (Exception e) {
            logger.error("Failed to parse response (%s)", e);
            return null;
        }
    }

    public static JSONObject parseJsonResponse(HttpResponse httpResponse, ILogger logger) {
        if (httpResponse == null) {
            return null;
        }
        String stringResponse = null;
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            httpResponse.getEntity().writeTo(out);
            out.close();
            stringResponse = out.toString().trim();
        } catch (Exception e) {
            logger.error("Failed to parse response (%s)", e.getMessage());
        }

        logger.verbose("Response: %s", stringResponse);
        if (stringResponse == null) return null;

        JSONObject jsonResponse = null;
        try {
            jsonResponse = new JSONObject(stringResponse);
        } catch (JSONException e) {
            logger.error("Failed to parse json response: %s (%s)", stringResponse, e.getMessage());
        }

        if (jsonResponse == null) return null;

        String message = jsonResponse.optString("message", null);

        if (message == null) {
            message = "No message found";
        }

        if (httpResponse.getStatusLine().getStatusCode() == HttpStatus.SC_OK) {
            logger.info("%s", message);
        } else {
            logger.error("%s", message);
        }

        return jsonResponse;
    }

    public static HttpClient getHttpClient() {
        HttpParams httpParams = new BasicHttpParams();
        HttpConnectionParams.setConnectionTimeout(httpParams, Constants.CONNECTION_TIMEOUT);
        HttpConnectionParams.setSoTimeout(httpParams, Constants.SOCKET_TIMEOUT);
        return AdjustFactory.getHttpClient(httpParams);
    }

    public static boolean checkPermission(Context context, String permission) {
        int result = context.checkCallingOrSelfPermission(permission);
        return result == PackageManager.PERMISSION_GRANTED;
    }
}
