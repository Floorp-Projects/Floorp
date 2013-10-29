        <activity android:name="org.mozilla.gecko.WebApps$WebApp@APPNUM@"
                  android:label="@string/webapp_generic_name"
                  android:configChanges="keyboard|keyboardHidden|mcc|mnc|orientation|screenSize"
                  android:windowSoftInputMode="stateUnspecified|adjustResize"
                  android:launchMode="singleTask"
                  android:taskAffinity="org.mozilla.gecko.WEBAPP@APPNUM@"
                  android:process=":@ANDROID_PACKAGE_NAME@.WebApp@APPNUM@"
                  android:excludeFromRecents="true"
                  android:theme="@style/Gecko.App">
        </activity>

        <!-- Alias WebApp so we can launch it from the package namespace. See
             .App alias comment in AndroidManifest.xml.in for more details. -->
        <activity-alias android:name=".WebApps$WebApp@APPNUM@"
                        android:label="@string/webapp_generic_name"
                        android:targetActivity="org.mozilla.gecko.WebApps$WebApp@APPNUM@">
            <intent-filter>
                <action android:name="org.mozilla.gecko.WEBAPP@APPNUM@" />
            </intent-filter>
            <intent-filter>
                <action android:name="org.mozilla.gecko.ACTION_ALERT_CALLBACK" />
            </intent-filter>
        </activity-alias>
