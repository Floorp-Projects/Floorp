/*
 * Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be found
 * in the LICENSE file in the root of the source tree. An additional
 * intellectual property rights grant can be found in the file PATENTS. All
 * contributing project authors may be found in the AUTHORS file in the root of
 * the source tree.
 */

/*
 * VoiceEngine Android test application. It starts either auto test or acts like
 * a GUI test.
 */

package org.webrtc.voiceengine.test;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;

import android.app.Activity;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

public class AndroidTest extends Activity {
    private byte[] _playBuffer = null;
    private short[] _circBuffer = new short[8000]; // can hold 50 frames

    private int _recIndex = 0;
    private int _playIndex = 0;
    // private int _streamVolume = 4;
    private int _maxVolume = 0; // Android max level (commonly 5)
    // VoE level (0-255), corresponds to level 4 out of 5
    private int _volumeLevel = 204;

    private Thread _playThread;
    private Thread _recThread;
    private Thread _autotestThread;

    private static AudioTrack _at;
    private static AudioRecord _ar;

    private File _fr = null;
    private FileInputStream _in = null;

    private boolean _isRunningPlay = false;
    private boolean _isRunningRec = false;
    private boolean _settingSet = true;
    private boolean _isCallActive = false;
    private boolean _runAutotest = false; // ENABLE AUTOTEST HERE!

    private int _channel = -1;
    private int _codecIndex = 0;
    private int _ecIndex = 0;
    private int _nsIndex = 0;
    private int _agcIndex = 0;
    private int _vadIndex = 0;
    private int _audioIndex = 3;
    private int _settingMenu = 0;
    private int _receivePort = 1234;
    private int _destinationPort = 1234;
    private String _destinationIP = "127.0.0.1";

    // "Build" settings
    private final boolean _playFromFile = false;
    // Set to true to send data to native code and back
    private final boolean _runThroughNativeLayer = true;
    private final boolean enableSend = true;
    private final boolean enableReceive = true;
    private final boolean useNativeThread = false;

    /** Called when the activity is first created. */
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        TextView tv = (TextView) findViewById(R.id.TextView01);
        tv.setText("");

        final EditText ed = (EditText) findViewById(R.id.EditText01);
        ed.setWidth(200);
        ed.setText(_destinationIP);

        final Button buttonStart = (Button) findViewById(R.id.Button01);
        buttonStart.setWidth(200);
        if (_runAutotest) {
            buttonStart.setText("Run test");
        } else {
            buttonStart.setText("Start Call");
        }
        // button.layout(50, 50, 100, 40);
        buttonStart.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {

                if (_runAutotest) {
                    startAutoTest();
                } else {
                    if (_isCallActive) {

                        if (stopCall() != -1) {
                            _isCallActive = false;
                            buttonStart.setText("Start Call");
                        }
                    } else {

                        _destinationIP = ed.getText().toString();
                        if (startCall() != -1) {
                            _isCallActive = true;
                            buttonStart.setText("Stop Call");
                        }
                    }
                }

                // displayTextFromFile();
                // recordAudioToFile();
                // if(!_playFromFile)
                // {
                // recAudioInThread();
                // }
                // playAudioInThread();
            }
        });

        final Button buttonStop = (Button) findViewById(R.id.Button02);
        buttonStop.setWidth(200);
        buttonStop.setText("Close app");
        buttonStop.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {

                if (!_runAutotest) {
                    ShutdownVoE();
                }

                // This call terminates and should close the activity
                finish();

                // playAudioFromFile();
                // if(!_playFromFile)
                // {
                // stopRecAudio();
                // }
                // stopPlayAudio();
            }
        });


        String ap1[] = {"EC off", "AECM"};
        final ArrayAdapter<String> adapterAp1 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        ap1);
        String ap2[] =
                        {"NS off", "NS low", "NS moderate", "NS high",
                                        "NS very high"};
        final ArrayAdapter<String> adapterAp2 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        ap2);
        String ap3[] = {"AGC off", "AGC adaptive", "AGC fixed"};
        final ArrayAdapter<String> adapterAp3 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        ap3);
        String ap4[] =
                        {"VAD off", "VAD conventional", "VAD high rate",
                                        "VAD mid rate", "VAD low rate"};
        final ArrayAdapter<String> adapterAp4 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        ap4);
        String codecs[] = {"iSAC", "PCMU", "PCMA", "iLBC"};
        final ArrayAdapter<String> adapterCodecs = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        codecs);

        final Spinner spinnerSettings1 = (Spinner) findViewById(R.id.Spinner01);
        final Spinner spinnerSettings2 = (Spinner) findViewById(R.id.Spinner02);
        spinnerSettings1.setMinimumWidth(200);
        String settings[] =
                        {"Codec", "Echo Control", "Noise Suppression",
                         "Automatic Gain Control",
                         "Voice Activity Detection"};
        ArrayAdapter<String> adapterSettings1 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        settings);
        spinnerSettings1.setAdapter(adapterSettings1);
        spinnerSettings1.setOnItemSelectedListener(
                        new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView adapterView, View view,
                            int position, long id) {

                _settingMenu = position;
                _settingSet = false;
                if (position == 0) {
                    spinnerSettings2.setAdapter(adapterCodecs);
                    spinnerSettings2.setSelection(_codecIndex);
                }
                if (position == 1) {
                    spinnerSettings2.setAdapter(adapterAp1);
                    spinnerSettings2.setSelection(_ecIndex);
                }
                if (position == 2) {
                    spinnerSettings2.setAdapter(adapterAp2);
                    spinnerSettings2.setSelection(_nsIndex);
                }
                if (position == 3) {
                    spinnerSettings2.setAdapter(adapterAp3);
                    spinnerSettings2.setSelection(_agcIndex);
                }
                if (position == 4) {
                    spinnerSettings2.setAdapter(adapterAp4);
                    spinnerSettings2.setSelection(_vadIndex);
                }
            }

            public void onNothingSelected(AdapterView adapterView) {
                WebrtcLog("No setting1 selected");
            }
        });

        spinnerSettings2.setMinimumWidth(200);
        ArrayAdapter<String> adapterSettings2 = new ArrayAdapter<String>(
                        this,
                        android.R.layout.simple_spinner_dropdown_item,
                        codecs);
        spinnerSettings2.setAdapter(adapterSettings2);
        spinnerSettings2.setOnItemSelectedListener(
                        new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView adapterView, View view,
                            int position, long id) {

                // avoid unintentional setting
                if (_settingSet == false) {
                    _settingSet = true;
                    return;
                }

                // Change volume
                if (_settingMenu == 0) {
                    WebrtcLog("Selected audio " + position);
                    setAudioProperties(position);
                    spinnerSettings2.setSelection(_audioIndex);
                }

                // Change codec
                if (_settingMenu == 1) {
                    _codecIndex = position;
                    WebrtcLog("Selected codec " + position);
                    if (0 != SetSendCodec(_channel, _codecIndex)) {
                        WebrtcLog("VoE set send codec failed");
                    }
                }

                // Change EC
                if (_settingMenu == 2) {
                    boolean enable = true;
                    int ECmode = 5; // AECM
                    int AESmode = 0;

                    _ecIndex = position;
                    WebrtcLog("Selected EC " + position);

                    if (position == 0) {
                        enable = false;
                    }
                    if (position > 1) {
                        ECmode = 4; // AES
                        AESmode = position - 1;
                    }

                    if (0 != SetECStatus(enable, ECmode)) {
                        WebrtcLog("VoE set EC status failed");
                    }
                }

                // Change NS
                if (_settingMenu == 3) {
                    boolean enable = true;

                    _nsIndex = position;
                    WebrtcLog("Selected NS " + position);

                    if (position == 0) {
                        enable = false;
                    }
                    if (0 != SetNSStatus(enable, position + 2)) {
                        WebrtcLog("VoE set NS status failed");
                    }
                }

                // Change AGC
                if (_settingMenu == 4) {
                    boolean enable = true;

                    _agcIndex = position;
                    WebrtcLog("Selected AGC " + position);

                    if (position == 0) {
                        enable = false;
                        position = 1; // default
                    }
                    if (0 != SetAGCStatus(enable, position + 2)) {
                        WebrtcLog("VoE set AGC status failed");
                    }
                }

                // Change VAD
                if (_settingMenu == 5) {
                    boolean enable = true;

                    _vadIndex = position;
                    WebrtcLog("Selected VAD " + position);

                    if (position == 0) {
                        enable = false;
                        position++;
                    }
                    if (0 != SetVADStatus(_channel, enable, position - 1)) {
                        WebrtcLog("VoE set VAD status failed");
                    }
                }
            }

            public void onNothingSelected(AdapterView adapterView) {
            }
        });

        // Setup VoiceEngine
        if (!_runAutotest && !useNativeThread) SetupVoE();

        // Suggest to use the voice call audio stream for hardware volume
        // controls
        setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);

        // Get max Android volume and adjust default volume to map exactly to an
        // Android level
        AudioManager am =
                        (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        _maxVolume = am.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
        if (_maxVolume <= 0) {
            WebrtcLog("Could not get max volume!");
        } else {
            int androidVolumeLevel = (_volumeLevel * _maxVolume) / 255;
            _volumeLevel = (androidVolumeLevel * 255) / _maxVolume;
        }

        WebrtcLog("Started Webrtc Android Test");
    }

    // Will be called when activity is shutdown.
    // NOTE: Activity may be killed without this function being called,
    // but then we should not need to clean up.
    protected void onDestroy() {
        super.onDestroy();
        // ShutdownVoE();
    }

    private void SetupVoE() {
        // Create VoiceEngine
        Create(); // Error logging is done in native API wrapper

        // Initialize
        if (0 != Init(false, false)) {
            WebrtcLog("VoE init failed");
        }

        // Create channel
        _channel = CreateChannel();
        if (0 != _channel) {
            WebrtcLog("VoE create channel failed");
        }

    }

    private void ShutdownVoE() {
        // Delete channel
        if (0 != DeleteChannel(_channel)) {
            WebrtcLog("VoE delete channel failed");
        }

        // Terminate
        if (0 != Terminate()) {
            WebrtcLog("VoE terminate failed");
        }

        // Delete VoiceEngine
        Delete(); // Error logging is done in native API wrapper
    }

    int startCall() {

        if (useNativeThread == true) {

            Create();
            return 0;
        }

        if (enableReceive == true) {
            // Set local receiver
            if (0 != SetLocalReceiver(_channel, _receivePort)) {
                WebrtcLog("VoE set local receiver failed");
            }

            if (0 != StartListen(_channel)) {
                WebrtcLog("VoE start listen failed");
                return -1;
            }

            // Route audio to earpiece
            if (0 != SetLoudspeakerStatus(false)) {
                WebrtcLog("VoE set louspeaker status failed");
                return -1;
            }

            /*
             * WebrtcLog("VoE start record now"); if (0 !=
             * StartRecordingPlayout(_channel, "/sdcard/singleUserDemoOut.pcm",
             * false)) { WebrtcLog("VoE Recording Playout failed"); }
             * WebrtcLog("VoE start Recording Playout end");
             */
            // Start playout
            if (0 != StartPlayout(_channel)) {
                WebrtcLog("VoE start playout failed");
                return -1;
            }

            // Start playout file
            // if (0 != StartPlayingFileLocally(_channel,
            // "/sdcard/singleUserDemo.pcm", true)) {
            // WebrtcLog("VoE start playout file failed");
            // return -1;
            // }
        }

        if (enableSend == true) {
            if (0 != SetSendDestination(_channel, _destinationPort,
                            _destinationIP)) {
                WebrtcLog("VoE set send  destination failed");
                return -1;
            }

            if (0 != SetSendCodec(_channel, _codecIndex)) {
                WebrtcLog("VoE set send codec failed");
                return -1;
            }

            /*
             * if (0 != StartPlayingFileAsMicrophone(_channel,
             * "/sdcard/singleUserDemo.pcm", true)) {
             * WebrtcLog("VoE start playing file as microphone failed"); }
             */
            if (0 != StartSend(_channel)) {
                WebrtcLog("VoE start send failed");
                return -1;
            }

            // if (0 != StartPlayingFileAsMicrophone(_channel,
            // "/sdcard/singleUserDemo.pcm", true)) {
            // WebrtcLog("VoE start playing file as microphone failed");
            // return -1;
            // }
        }

        return 0;
    }

    int stopCall() {

        if (useNativeThread == true) {

            Delete();
            return 0;
        }

        if (enableSend == true) {
            // Stop playing file as microphone
            /*
             * if (0 != StopPlayingFileAsMicrophone(_channel)) {
             * WebrtcLog("VoE stop playing file as microphone failed"); return
             * -1; }
             */
            // Stop send
            if (0 != StopSend(_channel)) {
                WebrtcLog("VoE stop send failed");
                return -1;
            }
        }

        if (enableReceive == true) {
            // if (0 != StopRecordingPlayout(_channel)) {
            // WebrtcLog("VoE stop Recording Playout failed");
            // }
            // WebrtcLog("VoE stop Recording Playout ended");

            // Stop listen
            if (0 != StopListen(_channel)) {
                WebrtcLog("VoE stop listen failed");
                return -1;
            }

            // Stop playout file
            // if (0 != StopPlayingFileLocally(_channel)) {
            // WebrtcLog("VoE stop playout file failed");
            // return -1;
            // }

            // Stop playout
            if (0 != StopPlayout(_channel)) {
                WebrtcLog("VoE stop playout failed");
                return -1;
            }

            // Route audio to loudspeaker
            if (0 != SetLoudspeakerStatus(true)) {
                WebrtcLog("VoE set louspeaker status failed");
                return -1;
            }
        }

        return 0;
    }

    int startAutoTest() {

        _autotestThread = new Thread(_autotestProc);
        _autotestThread.start();

        return 0;
    }

    private Runnable _autotestProc = new Runnable() {
        public void run() {
            // TODO(xians): choose test from GUI
            // 1 = standard, not used
            // 2 = extended, 2 = base
            RunAutoTest(1, 2);
        }
    };

    int setAudioProperties(int val) {

        // AudioManager am = (AudioManager)
        // getSystemService(Context.AUDIO_SERVICE);

        if (val == 0) {
            // _streamVolume =
            // am.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
            // am.setStreamVolume(AudioManager.STREAM_VOICE_CALL,
            // (_streamVolume+1), 0);

            int androidVolumeLevel = (_volumeLevel * _maxVolume) / 255;
            if (androidVolumeLevel < _maxVolume) {
                _volumeLevel = ((androidVolumeLevel + 1) * 255) / _maxVolume;
                if (0 != SetSpeakerVolume(_volumeLevel)) {
                    WebrtcLog("VoE set speaker volume failed");
                }
            }
        } else if (val == 1) {
            // _streamVolume =
            // am.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
            // am.setStreamVolume(AudioManager.STREAM_VOICE_CALL,
            // (_streamVolume-1), 0);

            int androidVolumeLevel = (_volumeLevel * _maxVolume) / 255;
            if (androidVolumeLevel > 0) {
                _volumeLevel = ((androidVolumeLevel - 1) * 255) / _maxVolume;
                if (0 != SetSpeakerVolume(_volumeLevel)) {
                    WebrtcLog("VoE set speaker volume failed");
                }
            }
        } else if (val == 2) {
            // route audio to back speaker
            if (0 != SetLoudspeakerStatus(true)) {
                WebrtcLog("VoE set loudspeaker status failed");
            }
            _audioIndex = 2;
        } else if (val == 3) {
            // route audio to earpiece
            if (0 != SetLoudspeakerStatus(false)) {
                WebrtcLog("VoE set loudspeaker status failed");
            }
            _audioIndex = 3;
        }

        return 0;
    }

    int displayTextFromFile() {

        TextView tv = (TextView) findViewById(R.id.TextView01);
        FileReader fr = null;
        char[] fileBuffer = new char[64];

        try {
            fr = new FileReader("/sdcard/test.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            tv.setText("File not found!");
        }

        try {
            fr.read(fileBuffer);
        } catch (IOException e) {
            e.printStackTrace();
        }

        String readString = new String(fileBuffer);
        tv.setText(readString);
        // setContentView(tv);

        return 0;
    }

    int recordAudioToFile() {
        File fr = null;
        // final to be reachable within onPeriodicNotification
        byte[] recBuffer = new byte[320];

        int recBufSize =
                        AudioRecord.getMinBufferSize(16000,
                                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                                        AudioFormat.ENCODING_PCM_16BIT);
        AudioRecord rec =
                        new AudioRecord(MediaRecorder.AudioSource.MIC, 16000,
                                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                                        AudioFormat.ENCODING_PCM_16BIT,
                                        recBufSize);

        fr = new File("/sdcard/record.pcm");
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(fr);
        } catch (FileNotFoundException e1) {
            e1.printStackTrace();
        }

        // start recording
        try {
            rec.startRecording();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        for (int i = 0; i < 550; i++) {
            // note, there is a short version of write as well!
            int wrBytes = rec.read(recBuffer, 0, 320);

            try {
                out.write(recBuffer);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        // stop playout
        try {
            rec.stop();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        return 0;
    }

    int playAudioFromFile() {

        File fr = null;
        // final to be reachable within onPeriodicNotification
        // final byte[] playBuffer = new byte [320000];
        // final to be reachable within onPeriodicNotification
        final byte[] playBuffer = new byte[320];

        final int playBufSize =
                        AudioTrack.getMinBufferSize(16000,
                                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                                        AudioFormat.ENCODING_PCM_16BIT);
        // final int playBufSize = 1920; // 100 ms buffer
        // byte[] playBuffer = new byte [playBufSize];
        final AudioTrack play =
                        new AudioTrack(AudioManager.STREAM_VOICE_CALL, 16000,
                                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                                        AudioFormat.ENCODING_PCM_16BIT,
                                        playBufSize, AudioTrack.MODE_STREAM);

        // implementation of the playpos callback functions
        play.setPlaybackPositionUpdateListener(
                        new AudioTrack.OnPlaybackPositionUpdateListener() {

            int count = 0;

            public void onPeriodicNotification(AudioTrack track) {
                // int wrBytes = play.write(playBuffer, count, 320);
                count += 320;
            }

            public void onMarkerReached(AudioTrack track) {

            }
        });

        // set the notification period = 160 samples
        // int ret = play.setPositionNotificationPeriod(160);

        fr = new File("/sdcard/record.pcm");
        FileInputStream in = null;
        try {
            in = new FileInputStream(fr);
        } catch (FileNotFoundException e1) {
            e1.printStackTrace();
        }

        // try {
        // in.read(playBuffer);
        // } catch (IOException e) {
        // e.printStackTrace();
        // }

        // play all at once
        // int wrBytes = play.write(playBuffer, 0, 320000);


        // start playout
        try {
            play.play();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        // returns the number of samples that has been written
        // int headPos = play.getPlaybackHeadPosition();

        // play with multiple writes
        for (int i = 0; i < 500; i++) {
            try {
                in.read(playBuffer);
            } catch (IOException e) {
                e.printStackTrace();
            }


            // note, there is a short version of write as well!
            int wrBytes = play.write(playBuffer, 0, 320);

            Log.d("testWrite", "wrote");
        }

        // stop playout
        try {
            play.stop();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        return 0;
    }

    int playAudioInThread() {

        if (_isRunningPlay) {
            return 0;
        }

        // File fr = null;
        // final byte[] playBuffer = new byte[320];
        if (_playFromFile) {
            _playBuffer = new byte[320];
        } else {
            // reset index
            _playIndex = 0;
        }
        // within
        // onPeriodicNotification

        // Log some info (static)
        WebrtcLog("Creating AudioTrack object");
        final int minPlayBufSize =
                        AudioTrack.getMinBufferSize(16000,
                                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                                        AudioFormat.ENCODING_PCM_16BIT);
        WebrtcLog("Min play buf size = " + minPlayBufSize);
        WebrtcLog("Min volume = " + AudioTrack.getMinVolume());
        WebrtcLog("Max volume = " + AudioTrack.getMaxVolume());
        WebrtcLog("Native sample rate = "
                        + AudioTrack.getNativeOutputSampleRate(
                                        AudioManager.STREAM_VOICE_CALL));

        final int playBufSize = minPlayBufSize; // 3200; // 100 ms buffer
        // byte[] playBuffer = new byte [playBufSize];
        try {
            _at = new AudioTrack(
                            AudioManager.STREAM_VOICE_CALL,
                            16000,
                            AudioFormat.CHANNEL_CONFIGURATION_MONO,
                            AudioFormat.ENCODING_PCM_16BIT,
                            playBufSize, AudioTrack.MODE_STREAM);
        } catch (Exception e) {
            WebrtcLog(e.getMessage());
        }

        // Log some info (non-static)
        WebrtcLog("Notification marker pos = "
                        + _at.getNotificationMarkerPosition());
        WebrtcLog("Play head pos = " + _at.getPlaybackHeadPosition());
        WebrtcLog("Pos notification dt = "
                        + _at.getPositionNotificationPeriod());
        WebrtcLog("Playback rate = " + _at.getPlaybackRate());
        WebrtcLog("Sample rate = " + _at.getSampleRate());

        // implementation of the playpos callback functions
        // _at.setPlaybackPositionUpdateListener(
        // new AudioTrack.OnPlaybackPositionUpdateListener() {
        //
        // int count = 3200;
        //
        // public void onPeriodicNotification(AudioTrack track) {
        // // int wrBytes = play.write(playBuffer, count, 320);
        // count += 320;
        // }
        //
        // public void onMarkerReached(AudioTrack track) {
        // }
        // });

        // set the notification period = 160 samples
        // int ret = _at.setPositionNotificationPeriod(160);

        if (_playFromFile) {
            _fr = new File("/sdcard/singleUserDemo.pcm");
            try {
                _in = new FileInputStream(_fr);
            } catch (FileNotFoundException e1) {
                e1.printStackTrace();
            }
        }

        // try {
        // in.read(playBuffer);
        // } catch (IOException e) {
        // e.printStackTrace();
        // }

        _isRunningPlay = true;

        // buffer = new byte[3200];
        _playThread = new Thread(_playProc);
        // ar.startRecording();
        // bytesRead = 3200;
        // recording = true;
        _playThread.start();

        return 0;
    }

    int stopPlayAudio() {
        if (!_isRunningPlay) {
            return 0;
        }

        _isRunningPlay = false;

        return 0;
    }

    private Runnable _playProc = new Runnable() {
        public void run() {

            // set high thread priority
            android.os.Process.setThreadPriority(
                            android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);

            // play all at once
            // int wrBytes = play.write(playBuffer, 0, 320000);

            // fill the buffer
            // play.write(playBuffer, 0, 3200);

            // play.flush();

            // start playout
            try {
                _at.play();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            }

            // play with multiple writes
            int i = 0;
            for (; i < 3000 && _isRunningPlay; i++) {

                if (_playFromFile) {
                    try {
                        _in.read(_playBuffer);
                    } catch (IOException e) {
                        e.printStackTrace();
                    }

                    int wrBytes = _at.write(_playBuffer, 0 /* i * 320 */, 320);
                } else {
                    int wrSamples =
                                    _at.write(_circBuffer, _playIndex * 160,
                                                    160);

                    // WebrtcLog("Played 10 ms from buffer, _playIndex = " +
                    // _playIndex);
                    // WebrtcLog("Diff = " + (_recIndex - _playIndex));

                    if (_playIndex == 49) {
                        _playIndex = 0;
                    } else {
                        _playIndex += 1;
                    }
                }

                // WebrtcLog("Wrote 10 ms to buffer, head = "
                // + _at.getPlaybackHeadPosition());
            }

            // stop playout
            try {
                _at.stop();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            }

            // returns the number of samples that has been written
            WebrtcLog("Test stopped, i = " + i + ", head = "
                            + _at.getPlaybackHeadPosition());
            int headPos = _at.getPlaybackHeadPosition();

            // flush the buffers
            _at.flush();

            // release the object
            _at.release();
            _at = null;

            // try {
            // Thread.sleep() must be within a try - catch block
            // Thread.sleep(3000);
            // }catch (Exception e){
            // System.out.println(e.getMessage());
            // }

            _isRunningPlay = false;

        }
    };

    int recAudioInThread() {

        if (_isRunningRec) {
            return 0;
        }

        // within
        // onPeriodicNotification

        // reset index
        _recIndex = 20;

        // Log some info (static)
        WebrtcLog("Creating AudioRecord object");
        final int minRecBufSize = AudioRecord.getMinBufferSize(16000,
                        AudioFormat.CHANNEL_CONFIGURATION_MONO,
                        AudioFormat.ENCODING_PCM_16BIT);
        WebrtcLog("Min rec buf size = " + minRecBufSize);
        // WebrtcLog("Min volume = " + AudioTrack.getMinVolume());
        // WebrtcLog("Max volume = " + AudioTrack.getMaxVolume());
        // WebrtcLog("Native sample rate = "
        // + AudioRecord
        // .getNativeInputSampleRate(AudioManager.STREAM_VOICE_CALL));

        final int recBufSize = minRecBufSize; // 3200; // 100 ms buffer
        try {
            _ar = new AudioRecord(
                            MediaRecorder.AudioSource.MIC,
                            16000,
                            AudioFormat.CHANNEL_CONFIGURATION_MONO,
                            AudioFormat.ENCODING_PCM_16BIT,
                            recBufSize);
        } catch (Exception e) {
            WebrtcLog(e.getMessage());
        }

        // Log some info (non-static)
        WebrtcLog("Notification marker pos = "
                        + _ar.getNotificationMarkerPosition());
        // WebrtcLog("Play head pos = " + _ar.getRecordHeadPosition());
        WebrtcLog("Pos notification dt rec= "
                        + _ar.getPositionNotificationPeriod());
        // WebrtcLog("Playback rate = " + _ar.getRecordRate());
        // WebrtcLog("Playback rate = " + _ar.getPlaybackRate());
        WebrtcLog("Sample rate = " + _ar.getSampleRate());
        // WebrtcLog("Playback rate = " + _ar.getPlaybackRate());
        // WebrtcLog("Playback rate = " + _ar.getPlaybackRate());

        _isRunningRec = true;

        _recThread = new Thread(_recProc);

        _recThread.start();

        return 0;
    }

    int stopRecAudio() {
        if (!_isRunningRec) {
            return 0;
        }

        _isRunningRec = false;

        return 0;
    }

    private Runnable _recProc = new Runnable() {
        public void run() {

            // set high thread priority
            android.os.Process.setThreadPriority(
                            android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);

            // start recording
            try {
                _ar.startRecording();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            }

            // keep recording to circular buffer
            // for a while
            int i = 0;
            int rdSamples = 0;
            short[] tempBuffer = new short[160]; // Only used for native case

            for (; i < 3000 && _isRunningRec; i++) {
                if (_runThroughNativeLayer) {
                    rdSamples = _ar.read(tempBuffer, 0, 160);
                    // audioLoop(tempBuffer, 160); // Insert into native layer
                } else {
                    rdSamples = _ar.read(_circBuffer, _recIndex * 160, 160);

                    // WebrtcLog("Recorded 10 ms to buffer, _recIndex = " +
                    // _recIndex);
                    // WebrtcLog("rdSamples = " + rdSamples);

                    if (_recIndex == 49) {
                        _recIndex = 0;
                    } else {
                        _recIndex += 1;
                    }
                }
            }

            // stop recording
            try {
                _ar.stop();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            }

            // release the object
            _ar.release();
            _ar = null;

            // try {
            // Thread.sleep() must be within a try - catch block
            // Thread.sleep(3000);
            // }catch (Exception e){
            // System.out.println(e.getMessage());
            // }

            _isRunningRec = false;

            // returns the number of samples that has been written
            // WebrtcLog("Test stopped, i = " + i + ", head = "
            // + _at.getPlaybackHeadPosition());
            // int headPos = _at.getPlaybackHeadPosition();
        }
    };

    private void WebrtcLog(String msg) {
        Log.d("*Webrtc*", msg);
    }

    // //////////////// Native function prototypes ////////////////////

    private native static boolean NativeInit();

    private native int RunAutoTest(int testType, int extendedSel);

    private native boolean Create();

    private native boolean Delete();

    private native int Init(boolean enableTrace, boolean useExtTrans);

    private native int Terminate();

    private native int CreateChannel();

    private native int DeleteChannel(int channel);

    private native int SetLocalReceiver(int channel, int port);

    private native int SetSendDestination(int channel, int port,
                    String ipaddr);

    private native int StartListen(int channel);

    private native int StartPlayout(int channel);

    private native int StartSend(int channel);

    private native int StopListen(int channel);

    private native int StopPlayout(int channel);

    private native int StopSend(int channel);

    private native int StartPlayingFileLocally(int channel, String fileName,
                    boolean loop);

    private native int StopPlayingFileLocally(int channel);

    private native int StartRecordingPlayout(int channel, String fileName,
                    boolean loop);

    private native int StopRecordingPlayout(int channel);

    private native int StartPlayingFileAsMicrophone(int channel,
                    String fileName, boolean loop);

    private native int StopPlayingFileAsMicrophone(int channel);

    private native int NumOfCodecs();

    private native int SetSendCodec(int channel, int index);

    private native int SetVADStatus(int channel, boolean enable, int mode);

    private native int SetNSStatus(boolean enable, int mode);

    private native int SetAGCStatus(boolean enable, int mode);

    private native int SetECStatus(boolean enable, int mode);

    private native int SetSpeakerVolume(int volume);

    private native int SetLoudspeakerStatus(boolean enable);

    /*
     * this is used to load the 'webrtc-voice-demo-jni'
     * library on application startup.
     * The library has already been unpacked into
     * /data/data/webrtc.android.AndroidTest/lib/libwebrtc-voice-demo-jni.so
     * at installation time by the package manager.
     */
    static {
        Log.d("*Webrtc*", "Loading webrtc-voice-demo-jni...");
        System.loadLibrary("webrtc-voice-demo-jni");

        Log.d("*Webrtc*", "Calling native init...");
        if (!NativeInit()) {
            Log.e("*Webrtc*", "Native init failed");
            throw new RuntimeException("Native init failed");
        } else {
            Log.d("*Webrtc*", "Native init successful");
        }
    }
}
