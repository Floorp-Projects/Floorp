package com.adjust.sdk.plugin;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Locale;

public class MacAddressUtil {
    public static String getMacAddress(Context context) {
        final String rawAddress = getRawMacAddress(context);
        if (rawAddress == null) {
            return null;
        }
        final String upperAddress = rawAddress.toUpperCase(Locale.US);
        return removeSpaceString(upperAddress);
    }

    private static String getRawMacAddress(Context context) {
        // android devices should have a wlan address
        final String wlanAddress = loadAddress("wlan0");
        if (wlanAddress != null) {
            return wlanAddress;
        }

        // emulators should have an ethernet address
        final String ethAddress = loadAddress("eth0");
        if (ethAddress != null) {
            return ethAddress;
        }

        // query the wifi manager (requires the ACCESS_WIFI_STATE permission)
        try {
            final WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
            final String wifiAddress = wifiManager.getConnectionInfo().getMacAddress();
            if (wifiAddress != null) {
                return wifiAddress;
            }
        } catch (Exception e) {
            /* no-op */
        }

        return null;
    }

    private static String loadAddress(final String interfaceName) {
        try {
            final String filePath = "/sys/class/net/" + interfaceName + "/address";
            final StringBuilder fileData = new StringBuilder(1000);
            final BufferedReader reader = new BufferedReader(new FileReader(filePath), 1024);
            final char[] buf = new char[1024];
            int numRead;

            String readData;
            while ((numRead = reader.read(buf)) != -1) {
                readData = String.valueOf(buf, 0, numRead);
                fileData.append(readData);
            }

            reader.close();
            return fileData.toString();
        } catch (IOException e) {
            return null;
        }
    }

    private static String removeSpaceString(final String inputString) {
        if (inputString == null) {
            return null;
        }

        String outputString = inputString.replaceAll("\\s", "");
        if (TextUtils.isEmpty(outputString)) {
            return null;
        }

        return outputString;
    }
}
