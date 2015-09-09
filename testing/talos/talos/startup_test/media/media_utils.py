# vim:set ts=2 sw=2 sts=2 et cindent:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"""
 Utilities needed for performing media tests. At present, we do
 0. Audio Recording from the monitor of default sink device.
 1. Silence Removal.
 2. PESQ based quality measurement.
 4. Linux platform support
"""

import os
import re
import subprocess
import threading
import mozfile

here = os.path.dirname(os.path.realpath(__file__))

"""
Constants for audio tools, input and processed audio files
For Linux platform, PulseAudio and LibSox are assumed to be
installed.
"""
_TOOLS_PATH_ = os.path.join(here, 'tools')
_TEST_PATH_ = os.path.join(here, 'html')
_PESQ_ = os.path.join(_TOOLS_PATH_, 'PESQ')
_MEDIA_TOOLS_ = os.path.join(_TOOLS_PATH_, 'MediaUtils')
_INPUT_FILE_ = os.path.join(_TEST_PATH_, 'input16.wav')
# These files are created and removed as part of test run
_RECORDED_FILE_ = os.path.join(_TEST_PATH_, 'record.wav')
_RECORDED_NO_SILENCE_ = os.path.join(_TEST_PATH_, 'record_no_silence.wav')

# Constants used as parameters to Sox recorder and silence trimmer
# TODO: Make these dynamically configurable
_SAMPLE_RATE_ = '48000'  # 48khz - seems to work with pacat better than 16khz
_NUM_CHANNELS_ = '1'  # mono channel
_SOX_ABOVE_PERIODS_ = '1'  # One period of silence in the beginning
_SOX_ABOVE_DURATION_ = '2'  # Duration to trim till proper audio
_SOX_ABOVE_THRESHOLD_ = '5%'  # Audio level to trim at the beginning
_SOX_BELOW_PERIODS_ = '1'  # One period of silence in the beginning
_SOX_BELOW_DURATION_ = '2'  # Duration to trim till proper audio
_SOX_BELOW_THRESHOLD_ = '5%'  # Audio level to trim at the beginning
_PESQ_SAMPLE_RATE_ = '+16000'  # 16Khz
_VOLUME_100_PERCENT_ = '65536'  # Max volume
_DEFAULT_REC_DURATION_ = 10  # Defaults to 10 seconds worth of recording


class AudioRecorder(threading.Thread):
    """
    Thread to record audio
    """

    def __init__(self, parent, output_file):
        self.output_file = output_file
        threading.Thread.__init__(self)

    # Set record duration, typically set to length
    # of the audio test being run.
    def setDuration(self, duration):
        self.rec_duration = duration

    # Set source monitor for recording
    # We pick the first monitor for the sink available
    def setRecordingDevice(self, device):
        self.rec_device = device
        # Adjust volume of the sink to 100%, since quality was
        # bit degraded when this was not done.
        cmd = ['pacmd', 'set-source-volume', self.rec_device,
               _VOLUME_100_PERCENT_]
        cmd = [str(s) for s in cmd]
        try:
            p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
            p.communicate()
        except:
            return False, "Audio Recorder : pacmd set-source-volume failed"

        # no need to set the success message, since this function
        # is used internally
        return True

    # Start recording.
    def run(self):

        if not self.rec_device:
            return

        if not self.rec_duration:
            self.rec_duration = _DEFAULT_REC_DURATION_

        # PACAT command to record 16 bit singned little endian mono channel
        # audio from the sink self.rec_device
        pa_command = ['pacat', '-r', '-d', self.rec_device, '--format=s16le',
                      '--fix-rate', '--channels=1']
        pa_command = [str(s) for s in pa_command]

        # Sox command to convert raw audio from PACAT output to .wav format"
        sox_command = ['sox', '-t', 'raw', '-r', _SAMPLE_RATE_,
                       '--encoding=signed-integer', '-Lb', 16, '-c',
                       _NUM_CHANNELS_, '-', self.output_file, 'rate',
                       '16000', 'trim', 0, self.rec_duration]

        sox_command = [str(s) for s in sox_command]

        # Run the commands the PIPE them together
        p1 = subprocess.Popen(pa_command, stdout=subprocess.PIPE)
        p2 = subprocess.Popen(sox_command, stdin=p1.stdout,
                              stdout=subprocess.PIPE)
        p2.communicate()[0]
        # No need to kill p2 since it is terminated as part of trim duration
        if p1:
            p1.kill()


class AudioUtils(object):
    '''
    Utility class for managing pre and post recording operations
    It includes operations to
      - start/stop audio recorder based on PusleAudio and SOX
      - Trim the silence off the recorded audio based on SOX
      - Compute PESQ scores
    '''

    # Reference to the recorder thread.
    recorder = None

    # Function to find the monitor for sink available
    # on the platform.
    def setupAudioDeviceForRecording(self):
        # Use pactl to find the monitor for our Sink
        # This picks the first sink available
        cmd = ['pactl', 'list']
        output = subprocess.check_output(cmd)
        result = re.search('\s*Name: (\S*\.monitor)', output)
        if result:
            self.recorder.setRecordingDevice(result.group(1))
            return True, "Recording Device: %s Set" % result.group(1)
        else:
            return False, "Unable to Set Recording Device"

    # Run SNR on the audio reference file and recorded file
    def computeSNRAndDelay(self):
        snr_delay = "-1.000,-1"

        if not os.path.exists(_MEDIA_TOOLS_):
            return False, "SNR Tool not found"

        cmd = [_MEDIA_TOOLS_, '-c', 'snr', '-r', _INPUT_FILE_, '-t',
               _RECORDED_NO_SILENCE_]
        cmd = [str(s) for s in cmd]

        output = subprocess.check_output(cmd)
        # SNR_Delay=1.063,5
        result = re.search('SNR_DELAY=(\d+\.\d+),(\d+)', output)

        # delete the recorded file with no silence
        mozfile.remove(_RECORDED_NO_SILENCE_)

        if result:
            snr_delay = str(result.group(1)) + ',' + str(result.group(2))
            return True, snr_delay
        else:
            """
            We return status as True since SNR computation went through
            successfully but scores computation failed due to severly
            degraded audio quality.
            """
            return True, snr_delay

    # Kick-off Audio Recording Thread
    def startRecording(self, duration):

        if self.recorder and self.recorder.is_alive():
            return False, "An Running Instance Of Recorder Found"

        self.recorder = AudioRecorder(self, _RECORDED_FILE_)
        if not self.recorder:
            return False, "Audio Recorder Setup Failed"

        status, message = self.setupAudioDeviceForRecording()
        if status is True:
            self.recorder.setDuration(duration)
            self.recorder.start()
            # check if the thread did start
            if self.recorder.is_alive():
                return True, message
            else:
                return False, "Audio Recorder Setup Failed"
        else:
            return False, message

    # Let complete Audio Recording and remove silence at the
    # beginning and towards the end of recorded audio.
    def stopRecording(self):
        self.recorder.join()
        # clean up silence and delete the recorded file
        """
        http://digitalcardboard.com/blog/2009/08/25/the-sox-of-silence/
        ./sox record.wav out1.wav silence 1 2 5% 1 2 5% reverse silence
        1 2 5%
        """
        cmd = ['sox', _RECORDED_FILE_, _RECORDED_NO_SILENCE_, 'silence',
               _SOX_ABOVE_PERIODS_, _SOX_ABOVE_DURATION_,
               _SOX_ABOVE_THRESHOLD_, 'reverse', 'silence',
               _SOX_BELOW_PERIODS_, _SOX_BELOW_DURATION_,
               _SOX_BELOW_THRESHOLD_, 'reverse']
        cmd = [str(s) for s in cmd]
        subprocess.call(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        # Delete the recorded file
        mozfile.remove(_RECORDED_FILE_)
