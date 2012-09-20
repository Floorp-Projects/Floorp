        <activity android:name=".WebApps$WebApp@APPNUM@"
                  android:label="@string/webapp_generic_name"
                  android:configChanges="keyboard|keyboardHidden|mcc|mnc|orientation|screenSize"
                  android:windowSoftInputMode="stateUnspecified|adjustResize"
                  android:launchMode="singleInstance"
                  android:taskAffinity="org.mozilla.gecko.WEBAPP@APPNUM@"
                  android:process=":@ANDROID_PACKAGE_NAME@.WebApp@APPNUM@"
                  android:theme="@style/Gecko.Translucent.NoActionBar">
            <intent-filter>
                <action android:name="org.mozilla.gecko.WEBAPP@APPNUM@" />
            </intent-filter>
            <intent-filter>
                <action android:name="org.mozilla.gecko.ACTION_ALERT_CALLBACK" />
            </intent-filter>
        </activity>

