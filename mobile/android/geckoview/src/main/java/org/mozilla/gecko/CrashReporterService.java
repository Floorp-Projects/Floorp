package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.MinidumpAnalyzer;
import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;
import org.mozilla.gecko.util.ProxySelector;

import android.app.IntentService;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLDecoder;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.security.MessageDigest;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.GZIPOutputStream;

public class CrashReporterService extends IntentService {
    private static final String LOGTAG = "CrashReporter";
    private static final String ACTION_REPORT_CRASH = "org.mozilla.gecko.reportCrash";
    private static final String PASSED_MINI_DUMP_KEY = "minidumpPath";
    private static final String PASSED_MINI_DUMP_SUCCESS_KEY = "minidumpSuccess";
    private static final String MINI_DUMP_PATH_KEY = "upload_file_minidump";
    private static final String PAGE_URL_KEY = "URL";
    private static final String NOTES_KEY = "Notes";
    private static final String SERVER_URL_KEY = "ServerURL";

    private static final String CRASH_REPORT_SUFFIX = "/mozilla/Crash Reports/";
    private static final String PENDING_SUFFIX = CRASH_REPORT_SUFFIX + "pending";
    private static final String SUBMITTED_SUFFIX = CRASH_REPORT_SUFFIX + "submitted";

    private File mPendingMinidumpFile;
    private File mPendingExtrasFile;
    private HashMap<String, String> mExtrasStringMap;
    private boolean mMinidumpSucceeded;

    public CrashReporterService() {
        super("CrashReporterService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (intent == null || !intent.getAction().equals(ACTION_REPORT_CRASH)) {
            Log.d(LOGTAG, "Invalid or unknown action");
            return;
        }

        Class<?> reporterActivityCls = getFennecReporterActivity();
        if (reporterActivityCls != null) {
            intent.setClass(this, reporterActivityCls);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            return;
        }

        submitCrash(intent);
    }

    private Class<?> getFennecReporterActivity() {
        try {
            return Class.forName("org.mozilla.gecko.CrashReporterActivity");
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    private boolean moveFile(File inFile, File outFile) {
        Log.i(LOGTAG, "moving " + inFile + " to " + outFile);
        if (inFile.renameTo(outFile))
            return true;
        try {
            outFile.createNewFile();
            Log.i(LOGTAG, "couldn't rename minidump file");
            // so copy it instead
            FileChannel inChannel = new FileInputStream(inFile).getChannel();
            FileChannel outChannel = new FileOutputStream(outFile).getChannel();
            long transferred = inChannel.transferTo(0, inChannel.size(), outChannel);
            inChannel.close();
            outChannel.close();

            if (transferred > 0)
                inFile.delete();
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while copying minidump file: ", e);
            return false;
        }
        return true;
    }

    private void submitCrash(Intent intent) {
        mMinidumpSucceeded = intent.getBooleanExtra(PASSED_MINI_DUMP_SUCCESS_KEY, false);
        if (!mMinidumpSucceeded) {
            Log.i(LOGTAG, "Failed to get minidump.");
        }
        String passedMinidumpPath = intent.getStringExtra(PASSED_MINI_DUMP_KEY);
        File passedMinidumpFile = new File(passedMinidumpPath);
        File pendingDir = new File(getFilesDir(), PENDING_SUFFIX);
        pendingDir.mkdirs();
        mPendingMinidumpFile = new File(pendingDir, passedMinidumpFile.getName());
        moveFile(passedMinidumpFile, mPendingMinidumpFile);

        File extrasFile = new File(passedMinidumpPath.replaceAll("\\.dmp", ".extra"));
        mPendingExtrasFile = new File(pendingDir, extrasFile.getName());
        moveFile(extrasFile, mPendingExtrasFile);

        // Compute the minidump hash and generate the stack traces
        computeMinidumpHash(mPendingExtrasFile, mPendingMinidumpFile);

        try {
            GeckoLoader.loadMozGlue(this);

            if (!MinidumpAnalyzer.GenerateStacks(mPendingMinidumpFile.getPath(), /* fullStacks */ false)) {
                Log.e(LOGTAG, "Could not generate stacks for this minidump: " + passedMinidumpPath);
            }
        } catch (UnsatisfiedLinkError e) {
            Log.e(LOGTAG, "Could not load libmozglue.so, stacks for this crash won't be generated");
        }

        // Extract the annotations from the .extra file
        mExtrasStringMap = new HashMap<String, String>();
        readStringsFromFile(mPendingExtrasFile.getPath(), mExtrasStringMap);

        try {
            // Find the profile name and path. Since we don't have any other way of getting it within
            // this context we extract it from the crash dump path.
            final File profileDir = passedMinidumpFile.getParentFile().getParentFile();
            final String profileName = getProfileName(profileDir);

            if (profileName != null) {
                // Extract the crash dump ID and telemetry client ID, we need profile access for the latter.
                final String passedMinidumpName = passedMinidumpFile.getName();
                // Strip the .dmp suffix from the minidump name to obtain the crash ID.
                final String crashId = passedMinidumpName.substring(0, passedMinidumpName.length() - 4);
                final GeckoProfile profile = GeckoProfile.get(this, profileName, profileDir);
                final String clientId = profile.getClientId();
            }
        } catch (GeckoProfileDirectories.NoMozillaDirectoryException | IOException e) {
            Log.e(LOGTAG, "Cannot send the crash ping: ", e);
        }

        // Notify GeckoApp that we've crashed, so it can react appropriately during the next start.
        try {
            File crashFlag = new File(GeckoProfileDirectories.getMozillaDirectory(this), "CRASHED");
            crashFlag.createNewFile();
        } catch (GeckoProfileDirectories.NoMozillaDirectoryException | IOException e) {
            Log.e(LOGTAG, "Cannot set crash flag: ", e);
        }

        sendReport(mPendingMinidumpFile, mExtrasStringMap, mPendingExtrasFile);
    }


    private String getProfileName(File profileDir) throws GeckoProfileDirectories.NoMozillaDirectoryException {
        final File mozillaDir = GeckoProfileDirectories.getMozillaDirectory(this);
        final INIParser parser = GeckoProfileDirectories.getProfilesINI(mozillaDir);
        String profileName = null;

        if (parser.getSections() != null) {
            for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements(); ) {
                final INISection section = e.nextElement();
                final String path = section.getStringProperty("Path");
                final boolean isRelative = (section.getIntProperty("IsRelative") == 1);

                if ((isRelative && path.equals(profileDir.getName())) ||
                        path.equals(profileDir.getPath())) {
                    profileName = section.getStringProperty("Name");
                    break;
                }
            }
        }

        return profileName;
    }


    private void computeMinidumpHash(File extraFile, File minidump) {
        try {
            FileInputStream stream = new FileInputStream(minidump);
            MessageDigest md = MessageDigest.getInstance("SHA-256");

            try {
                byte[] buffer = new byte[4096];
                int readBytes;

                while ((readBytes = stream.read(buffer)) != -1) {
                    md.update(buffer, 0, readBytes);
                }
            } finally {
                stream.close();
            }

            byte[] digest = md.digest();
            StringBuilder hash = new StringBuilder(84);

            hash.append("MinidumpSha256Hash=");

            for (int i = 0; i < digest.length; i++) {
                hash.append(Integer.toHexString((digest[i] & 0xf0) >> 4));
                hash.append(Integer.toHexString(digest[i] & 0x0f));
            }

            hash.append('\n');

            FileWriter writer = new FileWriter(extraFile, /* append */ true);

            try {
                writer.write(hash.toString());
            } finally {
                writer.close();
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while computing the minidump hash: ", e);
        }
    }

    private boolean readStringsFromFile(String filePath, Map<String, String> stringMap) {
        try {
            BufferedReader reader = new BufferedReader(new FileReader(filePath));
            return readStringsFromReader(reader, stringMap);
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while reading strings: ", e);
            return false;
        }
    }

    private boolean readStringsFromReader(BufferedReader reader, Map<String, String> stringMap) throws IOException {
        String line;
        while ((line = reader.readLine()) != null) {
            int equalsPos = -1;
            if ((equalsPos = line.indexOf('=')) != -1) {
                String key = line.substring(0, equalsPos);
                String val = unescape(line.substring(equalsPos + 1));
                stringMap.put(key, val);
            }
        }
        reader.close();
        return true;
    }

    private String generateBoundary() {
        // Generate some random numbers to fill out the boundary
        int r0 = (int)(Integer.MAX_VALUE * Math.random());
        int r1 = (int)(Integer.MAX_VALUE * Math.random());
        return String.format("---------------------------%08X%08X", r0, r1);
    }

    private void sendPart(OutputStream os, String boundary, String name, String data) {
        try {
            os.write(("--" + boundary + "\r\n" +
                    "Content-Disposition: form-data; name=\"" + name + "\"\r\n" +
                    "\r\n" +
                    data + "\r\n"
            ).getBytes());
        } catch (Exception ex) {
            Log.e(LOGTAG, "Exception when sending \"" + name + "\"", ex);
        }
    }

    private void sendFile(OutputStream os, String boundary, String name, File file) throws IOException {
        os.write(("--" + boundary + "\r\n" +
                "Content-Disposition: form-data; name=\"" + name + "\"; " +
                "filename=\"" + file.getName() + "\"\r\n" +
                "Content-Type: application/octet-stream\r\n" +
                "\r\n"
        ).getBytes());
        FileChannel fc = new FileInputStream(file).getChannel();
        fc.transferTo(0, fc.size(), Channels.newChannel(os));
        fc.close();
    }

    private void sendReport(File minidumpFile, Map<String, String> extras, File extrasFile) {
        Log.i(LOGTAG, "sendReport: " + minidumpFile.getPath());

        String spec = extras.get(SERVER_URL_KEY);
        if (spec == null) {
            return;
        }

        try {
            final URL url = new URL(URLDecoder.decode(spec, "UTF-8"));
            final URI uri = new URI(url.getProtocol(), url.getUserInfo(),
                    url.getHost(), url.getPort(),
                    url.getPath(), url.getQuery(), url.getRef());
            HttpURLConnection conn = (HttpURLConnection) ProxySelector.openConnectionWithProxy(uri);
            conn.setRequestMethod("POST");
            String boundary = generateBoundary();
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
            conn.setRequestProperty("Content-Encoding", "gzip");

            OutputStream os = new GZIPOutputStream(conn.getOutputStream());
            for (String key : extras.keySet()) {
                if (key.equals(PAGE_URL_KEY)) {
                    continue;
                }

                if (!key.equals(SERVER_URL_KEY) && !key.equals(NOTES_KEY)) {
                    sendPart(os, boundary, key, extras.get(key));
                }
            }

            StringBuilder sb = new StringBuilder();
            sb.append(extras.containsKey(NOTES_KEY) ? extras.get(NOTES_KEY) + "\n" : "");
            sb.append(Build.MANUFACTURER).append(' ')
                    .append(Build.MODEL).append('\n')
                    .append(Build.FINGERPRINT);
            sendPart(os, boundary, NOTES_KEY, sb.toString());

            sendPart(os, boundary, "Android_Manufacturer", Build.MANUFACTURER);
            sendPart(os, boundary, "Android_Model", Build.MODEL);
            sendPart(os, boundary, "Android_Board", Build.BOARD);
            sendPart(os, boundary, "Android_Brand", Build.BRAND);
            sendPart(os, boundary, "Android_Device", Build.DEVICE);
            sendPart(os, boundary, "Android_Display", Build.DISPLAY);
            sendPart(os, boundary, "Android_Fingerprint", Build.FINGERPRINT);
            sendPart(os, boundary, "Android_CPU_ABI", Build.CPU_ABI);
            sendPart(os, boundary, "Android_PackageName", getPackageName());
            try {
                sendPart(os, boundary, "Android_CPU_ABI2", Build.CPU_ABI2);
                sendPart(os, boundary, "Android_Hardware", Build.HARDWARE);
            } catch (Exception ex) {
                Log.e(LOGTAG, "Exception while sending SDK version 8 keys", ex);
            }
            sendPart(os, boundary, "Android_Version",  Build.VERSION.SDK_INT + " (" + Build.VERSION.CODENAME + ")");
            sendPart(os, boundary, PASSED_MINI_DUMP_SUCCESS_KEY, mMinidumpSucceeded ? "True" : "False");
            sendFile(os, boundary, MINI_DUMP_PATH_KEY, minidumpFile);
            os.write(("\r\n--" + boundary + "--\r\n").getBytes());
            os.flush();
            os.close();
            BufferedReader br = new BufferedReader(
                    new InputStreamReader(conn.getInputStream()));
            HashMap<String, String>  responseMap = new HashMap<String, String>();
            readStringsFromReader(br, responseMap);

            if (conn.getResponseCode() == HttpURLConnection.HTTP_OK) {
                File submittedDir = new File(getFilesDir(),
                        SUBMITTED_SUFFIX);
                submittedDir.mkdirs();
                minidumpFile.delete();
                extrasFile.delete();
                String crashid = responseMap.get("CrashID");
                File file = new File(submittedDir, crashid + ".txt");
                FileOutputStream fos = new FileOutputStream(file);
                fos.write("Crash ID: ".getBytes());
                fos.write(crashid.getBytes());
                fos.close();
                Log.i(LOGTAG, "Successfully sent crash report: " + crashid);
            } else {
                Log.w(LOGTAG, "Received failure HTTP response code from server: " + conn.getResponseCode());
            }
        } catch (IOException e) {
            Log.e(LOGTAG, "exception during send: ", e);
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, "exception during new URI: ", e);
        }
    }

    private String unescape(String string) {
        return string.replaceAll("\\\\\\\\", "\\").replaceAll("\\\\n", "\n").replaceAll("\\\\t", "\t");
    }
}
