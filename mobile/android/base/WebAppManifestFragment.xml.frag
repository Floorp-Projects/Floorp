        <activity android:name=".WebApps$WebApp@APPNUM@"
                  android:label="WebApp@APPNUM@"
                  android:configChanges="keyboard|keyboardHidden|mcc|mnc|orientation|screenSize"
                  android:windowSoftInputMode="stateUnspecified|adjustResize"
                  android:launchMode="singleInstance"
                  android:process=":@ANDROID_PACKAGE_NAME@.WebApp@APPNUM@"
                  android:theme="@style/Gecko.NoActionBar">
            <intent-filter>
                <action android:name="org.mozilla.gecko.WEBAPP@APPNUM@" />
            </intent-filter>
        </activity>

