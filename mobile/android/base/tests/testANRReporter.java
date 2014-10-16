package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;

import android.content.Context;
import android.content.Intent;

import com.jayway.android.robotium.solo.Condition;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;

import org.json.JSONObject;

/**
 * Tests the proper operation of the ANR reporter.
 */
public class testANRReporter extends BaseTest {

    private static final String ANR_ACTION = "android.intent.action.ANR";
    private static final String PING_DIR = "saved-telemetry-pings";
    private static final int WAIT_FOR_PING_TIMEOUT = 60000;
    private static final String ANR_PATH = "/data/anr/traces.txt";
    private static final String SAMPLE_ANR
        = "----- pid 1 at 2014-01-15 18:55:51 -----\n"
        + "Cmd line: " + AppConstants.ANDROID_PACKAGE_NAME + "\n"
        + "\n"
        + "JNI: CheckJNI is off; workarounds are off; pins=0; globals=397\n"
        + "\n"
        + "DALVIK THREADS:\n"
        + "(mutexes: tll=0 tsl=0 tscl=0 ghl=0)\n"
        + "\n"
        + "\"main\" prio=5 tid=1 WAIT\n"
        + "  | group=\"main\" sCount=1 dsCount=0 obj=0x41d6bc90 self=0x41d5a3c8\n"
        + "  | sysTid=3485 nice=0 sched=0/0 cgrp=apps handle=1074852180\n"
        + "  | state=S schedstat=( 0 0 0 ) utm=1065 stm=152 core=0\n"
        + "  at java.lang.Object.wait(Native Method)\n"
        + "  - waiting on <0x427ab340> (a org.mozilla.gecko.GeckoEditable$5)\n"
        + "  at java.lang.Object.wait(Object.java:364)\n"
        + "  at org.mozilla.gecko.GeckoEditable$5.run(GeckoEditable.java:746)\n"
        + "  at android.os.Handler.handleCallback(Handler.java:733)\n"
        + "  at android.os.Handler.dispatchMessage(Handler.java:95)\n"
        + "  at android.os.Looper.loop(Looper.java:137)\n"
        + "  at android.app.ActivityThread.main(ActivityThread.java:4998)\n"
        + "  at java.lang.reflect.Method.invokeNative(Native Method)\n"
        + "  at java.lang.reflect.Method.invoke(Method.java:515)\n"
        + "  at com.android.internal.os.ZygoteInit$MethodAndArgsCaller.run(ZygoteInit.java:777)\n"
        + "  at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:593)\n"
        + "  at dalvik.system.NativeStart.main(Native Method)\n"
        + "\n"
        + "\"Gecko\" prio=5 tid=16 SUSPENDED\n"
        + "  | group=\"main\" sCount=1 dsCount=0 obj=0x426e2b28 self=0x76ae92e8\n"
        + "  | sysTid=3541 nice=0 sched=0/0 cgrp=apps handle=1991153472\n"
        + "  | state=S schedstat=( 0 0 0 ) utm=1118 stm=145 core=0\n"
        + "  #00  pc 00000904  /system/lib/libc.so (__futex_syscall3+4294832136)\n"
        + "  #01  pc 0000eec4  /system/lib/libc.so (__pthread_cond_timedwait_relative+48)\n"
        + "  #02  pc 0000ef24  /system/lib/libc.so (__pthread_cond_timedwait+64)\n"
        + "  #03  pc 000536b7  /system/lib/libdvm.so\n"
        + "  #04  pc 00053c79  /system/lib/libdvm.so (dvmChangeStatus(Thread*, ThreadStatus)+34)\n"
        + "  #05  pc 00049507  /system/lib/libdvm.so\n"
        + "  #06  pc 0004d84b  /system/lib/libdvm.so\n"
        + "  #07  pc 0003f1df  /dev/ashmem/libxul.so (deleted)\n"
        + "  at org.mozilla.gecko.mozglue.GeckoLoader.nativeRun(Native Method)\n"
        + "  at org.mozilla.gecko.GeckoAppShell.runGecko(GeckoAppShell.java:384)\n"
        + "  at org.mozilla.gecko.GeckoThread.run(GeckoThread.java:177)\n"
        + "\n"
        + "----- end 1 -----\n"
        + "\n"
        + "\n"
        + "----- pid 2 at 2013-01-25 13:27:01 -----\n"
        + "Cmd line: system_server\n"
        + "\n"
        + "----- end 2 -----\n";

    private boolean mDone;

    private JSONObject readPingFile(final File pingFile) throws Exception {
        final long fileSize = pingFile.length();
        if (fileSize == 0 || fileSize > Integer.MAX_VALUE) {
            throw new Exception("Invalid ping file size");
        }
        final char[] buffer = new char[(int) fileSize];
        final FileReader reader = new FileReader(pingFile);
        try {
            final int readSize = reader.read(buffer);
            if (readSize == 0 || readSize > buffer.length) {
                throw new Exception("Invalid number of bytes read");
            }
        } finally {
            reader.close();
        }
        return new JSONObject(new String(buffer));
    }

    public void testANRReporter() throws Exception {
        blockForGeckoReady();

        // Cannot test ANR reporter if it's disabled.
        if (!AppConstants.MOZ_ANDROID_ANR_REPORTER) {
            mAsserter.ok(true, "ANR reporter is disabled", null);
            return;
        }

        // For the ANR reporter to work, we need to provide sample ANR traces to it.
        // Therefore, we need the ANR file to exist and writable. If not, we don't
        // have the right permissions to create the file, so we just bail.
        final File anrFile = new File(ANR_PATH);
        if (!anrFile.exists()) {
            mAsserter.ok(true, "ANR file does not exist", null);
            return;
        }
        if (!anrFile.canWrite()) {
            mAsserter.ok(true, "ANR file is not writable", null);
            return;
        }

        final FileWriter anrWriter = new FileWriter(anrFile);
        try {
            anrWriter.write(SAMPLE_ANR);
        } finally {
            anrWriter.close();
        }

        // Block the UI thread to simulate an ANR
        final Runnable uiBlocker = new Runnable() {
            @Override
            public synchronized void run() {
                while (!mDone) {
                    try {
                        wait();
                    } catch (final InterruptedException e) {
                    }
                }
            }
        };
        getActivity().runOnUiThread(uiBlocker);

        // Make sure our initial ping directory is empty.
        final File pingDir = new File(mProfile, PING_DIR);
        final String[] initialFiles = pingDir.list();
        mAsserter.ok(initialFiles == null || initialFiles.length == 0,
                     "Ping directory is empty", null);

        final Intent anrIntent = new Intent(ANR_ACTION);
        anrIntent.setPackage(AppConstants.ANDROID_PACKAGE_NAME);
        mAsserter.is(anrIntent.getPackage(), AppConstants.ANDROID_PACKAGE_NAME,
                     "Successfully set package name");

        final Context testContext = getInstrumentation().getContext();
        mAsserter.isnot(testContext, null, "testContext should not be null");

        // Trigger the ANR.
        mAsserter.info("Triggering ANR", null);
        testContext.sendBroadcast(anrIntent);

        // ANR reporter is supposed to ignore duplicate ANRs.
        // This will be checked later when we look for ping files.
        mAsserter.info("Triggering second ANR", null);
        testContext.sendBroadcast(new Intent(anrIntent));

        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                mAsserter.info("Waiting for ping", null);

                try {
                    // Sleep to allow the ANR reporter thread time to process the ANR.
                    Thread.sleep(1000);
                } catch (final InterruptedException e) {
                }

                final File[] newFiles = pingDir.listFiles();
                if (newFiles == null || newFiles.length == 0) {
                    // Keep waiting.
                    return false;
                }
                // Make sure we have a complete file. We skip assertions and catch all
                // exceptions here because the condition may not be satisfied now but may
                // be satisfied later. After the wait is over, we will repeat the same
                // steps with assertions and exceptions.
                try {
                    return readPingFile(newFiles[0]).has("slug");
                } catch (final Exception e) {
                    return false;
                }
            }
        }, WAIT_FOR_PING_TIMEOUT);

        mAsserter.ok(pingDir.exists(), "Ping directory exists", null);
        mAsserter.ok(pingDir.isDirectory(), "Ping directory is a directory", null);

        final File[] newFiles = pingDir.listFiles();
        mAsserter.isnot(newFiles, null, "Ping directory is not empty");
        mAsserter.is(newFiles.length, 1, "ANR reporter wrote one ping");
        mAsserter.ok(newFiles[0].exists(), "Ping exists", null);
        mAsserter.ok(newFiles[0].isFile(), "Ping is a file", null);
        mAsserter.ok(newFiles[0].canRead(), "Ping is readable", null);
        mAsserter.info("Found ping file", newFiles[0].getPath());

        // Check standard properties required by Telemetry server.
        final JSONObject pingObject = readPingFile(newFiles[0]);
        mAsserter.ok(pingObject.has("slug"), "Ping has slug property", null);
        mAsserter.ok(pingObject.has("reason"), "Ping has reason property", null);
        mAsserter.ok(pingObject.has("payload"), "Ping has payload property", null);

        final JSONObject pingPayload = pingObject.getJSONObject("payload");
        mAsserter.ok(pingPayload.has("ver"), "Payload has ver property", null);
        mAsserter.ok(pingPayload.has("info"), "Payload has info property", null);
        mAsserter.ok(pingPayload.has("androidANR"), "Payload has androidANR property", null);

        final JSONObject pingInfo = pingPayload.getJSONObject("info");
        mAsserter.ok(pingInfo.has("reason"), "Info has reason property", null);
        mAsserter.ok(pingInfo.has("appName"), "Info has appName property", null);
        mAsserter.ok(pingInfo.has("appUpdateChannel"), "Info has appUpdateChannel property", null);
        mAsserter.ok(pingInfo.has("appVersion"), "Info has appVersion property", null);
        mAsserter.ok(pingInfo.has("appBuildID"), "Info has appBuildID property", null);

        // Do some profile clean up. This is not absolutely necessary because the profile
        // is blown away after test runs anyways, so we don't check return values here.
        for (final File ping : newFiles) {
            ping.delete();
        }
        pingDir.delete();

        // Unblock UI thread
        synchronized (uiBlocker) {
            mDone = true;
            uiBlocker.notify();
        }

        // Clear the sample ANR
        final FileWriter anrClearer = new FileWriter(anrFile);
        anrClearer.close();
    }
}
