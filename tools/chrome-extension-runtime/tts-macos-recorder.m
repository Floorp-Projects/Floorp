// SPDX-License-Identifier: MPL-2.0
// Test-only CoreAudio loopback recorder; see TTS_WAVEFORM.md for compile/probe
// provenance and the separate pending actual browser TTS waveform validation.
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreAudio/CoreAudio.h>
#import <AudioToolbox/AudioToolbox.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

static NSString * const EndpointName = @"BlackHole 2ch";
static double UTCMS(void) { return NSDate.date.timeIntervalSince1970 * 1000.0; }
static void Fail(NSString *message) {
  @throw [NSException exceptionWithName:@"RecorderFailure" reason:message userInfo:nil];
}
static void WriteJSON(NSString *path, NSDictionary *value) {
  NSError *error = nil;
  NSData *data = [NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingPrettyPrinted error:&error];
  if (!data || ![data writeToFile:path options:NSDataWritingAtomic error:&error]) Fail(error.localizedDescription ?: @"metadata write failed");
  chmod(path.fileSystemRepresentation, 0600);
}
static NSString *DeviceString(AudioDeviceID device, AudioObjectPropertySelector selector) {
  AudioObjectPropertyAddress address = { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
  CFStringRef value = NULL;
  UInt32 size = sizeof(value);
  OSStatus status = AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &value);
  return status == noErr && value ? CFBridgingRelease(value) : @"";
}
static NSDictionary *OutputDevice(void) {
  AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
  AudioDeviceID device = kAudioObjectUnknown;
  UInt32 size = sizeof(device);
  OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, &device);
  if (status != noErr) Fail([NSString stringWithFormat:@"default output query failed: %d", (int)status]);
  return @{ @"deviceID": @(device), @"name": DeviceString(device, kAudioObjectPropertyName),
    @"uid": DeviceString(device, kAudioDevicePropertyDeviceUID) };
}
static NSString *AuthorizationName(AVAuthorizationStatus status) {
  switch (status) {
    case AVAuthorizationStatusAuthorized: return @"authorized";
    case AVAuthorizationStatusDenied: return @"denied";
    case AVAuthorizationStatusRestricted: return @"restricted";
    case AVAuthorizationStatusNotDetermined: return @"not-determined";
  }
  return @"unknown";
}
static NSArray<AVCaptureDevice *> *Inputs(void) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  return [AVCaptureDevice devicesWithMediaType:AVMediaTypeAudio];
#pragma clang diagnostic pop
}
static FILE *NewPrivateFile(NSString *path) {
  int descriptor = open(path.fileSystemRepresentation, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (descriptor < 0) Fail([@"cannot create fresh private output: " stringByAppendingString:path]);
  FILE *stream = fdopen(descriptor, "wb");
  if (!stream) { close(descriptor); Fail(@"fdopen failed"); }
  return stream;
}
static NSDictionary *TimeObject(CMTime value) {
  return @{ @"value": @(value.value), @"timescale": @(value.timescale), @"flags": @(value.flags), @"epoch": @(value.epoch) };
}

@interface Recorder : NSObject <AVCaptureAudioDataOutputSampleBufferDelegate>
@property(nonatomic,strong) AVCaptureSession *session;
@property(nonatomic,strong) AVCaptureDevice *device;
@property(nonatomic,strong) NSDictionary *endpoint;
@property(nonatomic,copy) NSString *readyPath;
@property(atomic,copy) NSString *failure;
@property(atomic) BOOL lostPackets;
@property(atomic) BOOL deviceChanged;
@property(nonatomic) FILE *raw;
@property(nonatomic) FILE *packets;
@property(nonatomic) uint64_t bytes;
@property(nonatomic) uint64_t maxBytes;
@property(nonatomic) double anchorHostSeconds;
@property(nonatomic) double anchorUTCMS;
@property(nonatomic) double uncertaintyMS;
@property(nonatomic) double firstMS;
@property(atomic) double lastEndMS;
@property(nonatomic) double sampleRate;
@property(atomic) BOOL ready;
@end
@implementation Recorder
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sample fromConnection:(AVCaptureConnection *)connection {
  (void)output; (void)connection;
  if (self.failure) return;
  @try {
    if (!CMSampleBufferDataIsReady(sample)) { self.lostPackets = YES; Fail(@"native PCM buffer is not ready"); }
    CMAudioFormatDescriptionRef description = CMSampleBufferGetFormatDescription(sample);
    const AudioStreamBasicDescription *format = description ? CMAudioFormatDescriptionGetStreamBasicDescription(description) : NULL;
    if (!format || format->mFormatID != kAudioFormatLinearPCM ||
        format->mChannelsPerFrame != 1 || format->mBitsPerChannel != 16 || format->mBytesPerFrame != 2 ||
        !(format->mFormatFlags & kAudioFormatFlagIsSignedInteger) ||
        (format->mFormatFlags & (kAudioFormatFlagIsFloat | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsNonInterleaved)) ||
        !isfinite(format->mSampleRate) || format->mSampleRate <= 0) Fail(@"observed native stream is not interleaved mono PCM16 little-endian");
    if (self.ready && self.sampleRate != format->mSampleRate) { self.deviceChanged = YES; Fail(@"native audio sample rate changed"); }
    CMItemCount frames = CMSampleBufferGetNumSamples(sample);
    if (frames <= 0 || frames > 48000 || (uint64_t)frames * 2 > self.maxBytes - self.bytes) Fail(@"native PCM frame/byte bound reached");
    CMClockRef masterClock = self.session.masterClock;
    CMTime pts = CMSampleBufferGetPresentationTimeStamp(sample);
    if (!masterClock || !CMTIME_IS_NUMERIC(pts)) Fail(@"native presentation clock unavailable");
    CMTime hostTime = CMSyncConvertTime(pts, masterClock, CMClockGetHostTimeClock());
    double hostSeconds = CMTimeGetSeconds(hostTime);
    if (!CMTIME_IS_NUMERIC(hostTime) || !isfinite(hostSeconds)) Fail(@"native host-clock conversion failed");
    double firstMS = self.anchorUTCMS + (hostSeconds - self.anchorHostSeconds) * 1000.0;
    if (self.ready && fabs(firstMS - self.lastEndMS) > 5.0) { self.lostPackets = YES; Fail(@"native PCM presentation discontinuity exceeds 5ms"); }
    NSMutableData *pcm = [NSMutableData dataWithLength:(NSUInteger)frames * 2];
    AudioBufferList buffers = { .mNumberBuffers = 1, .mBuffers = {{ .mNumberChannels = 1, .mDataByteSize = (UInt32)pcm.length, .mData = pcm.mutableBytes }} };
    OSStatus status = CMSampleBufferCopyPCMDataIntoAudioBufferList(sample, 0, (int32_t)frames, &buffers);
    if (status != noErr || buffers.mBuffers[0].mDataByteSize != pcm.length) Fail(@"native PCM copy failed");
    if (fwrite(pcm.bytes, 1, pcm.length, self.raw) != pcm.length) Fail(@"raw PCM write failed");
    NSDictionary *packet = @{ @"byteOffset": @(self.bytes), @"frameCount": @(frames),
      @"firstSampleAtMs": @(firstMS), @"uncertaintyMs": @(self.uncertaintyMS), @"silent": @NO,
      @"presentationTime": TimeObject(pts), @"hostPresentationTime": TimeObject(hostTime),
      @"callbackAtMs": @(UTCMS()) };
    NSData *json = [NSJSONSerialization dataWithJSONObject:packet options:0 error:NULL];
    if (!json || fwrite(json.bytes, 1, json.length, self.packets) != json.length || fputc('\n', self.packets) == EOF) Fail(@"packet timing write failed");
    self.bytes += pcm.length;
    self.sampleRate = format->mSampleRate;
    self.lastEndMS = firstMS + (double)frames / self.sampleRate * 1000.0;
    if (!self.ready) {
      self.firstMS = firstMS;
      fflush(self.raw); fflush(self.packets);
      WriteJSON(self.readyPath, @{ @"sampleRate": @(self.sampleRate), @"channels": @1,
        @"sampleFormat": @"s16le", @"backend": @"macOS AVFoundation/CoreAudio BlackHole",
        @"endpoint": self.endpoint, @"readyAtMs": @(UTCMS()), @"nativeNoPacketsMeanRenderSilence": @NO,
        @"clockAnchor": @{ @"hostSeconds": @(self.anchorHostSeconds), @"utcMs": @(self.anchorUTCMS), @"uncertaintyMs": @(self.uncertaintyMS) },
        @"observedASBD": @{ @"sampleRate": @(format->mSampleRate), @"formatID": @(format->mFormatID), @"formatFlags": @(format->mFormatFlags), @"bytesPerFrame": @(format->mBytesPerFrame), @"bitsPerChannel": @(format->mBitsPerChannel), @"channels": @(format->mChannelsPerFrame) } });
      self.ready = YES;
    }
  } @catch (NSException *exception) { self.failure = exception.reason ?: @"native recording failure"; }
}
@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSMutableDictionary<NSString *,NSString *> *options = [NSMutableDictionary dictionary];
    BOOL probe = NO;
    for (int index = 1; index < argc; index++) {
      NSString *key = @(argv[index]);
      if ([key isEqualToString:@"--probe"]) { probe = YES; continue; }
      if (![key hasPrefix:@"--"] || index + 1 >= argc) { fprintf(stderr, "invalid recorder arguments\n"); return 2; }
      options[key] = @(argv[++index]);
    }
    Recorder *recorder = [Recorder new];
    dispatch_queue_t queue = dispatch_queue_create("floorp.tts.loopback.pcm", DISPATCH_QUEUE_SERIAL);
    @try {
      NSDictionary *output = OutputDevice();
      NSArray<AVCaptureDevice *> *devices = Inputs();
      if (probe) {
        NSMutableArray *inputs = [NSMutableArray array];
        for (AVCaptureDevice *device in devices) [inputs addObject:@{ @"name": device.localizedName, @"uid": device.uniqueID, @"connected": @(device.connected) }];
        NSDictionary *metadata = @{ @"probeOnly": @YES, @"recordingStarted": @NO, @"output": output, @"inputs": inputs,
          @"authorization": AuthorizationName([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) };
        NSData *json = [NSJSONSerialization dataWithJSONObject:metadata options:NSJSONWritingPrettyPrinted error:NULL];
        fwrite(json.bytes, 1, json.length, stdout); fputc('\n', stdout); return 0;
      }
      for (NSString *key in @[@"--raw", @"--packets", @"--ready", @"--done", @"--stop"]) if (!options[key].length) Fail([@"missing " stringByAppendingString:key]);
      double seconds = options[@"--max-seconds"] ? options[@"--max-seconds"].doubleValue : 300;
      uint64_t limit = options[@"--max-bytes"] ? options[@"--max-bytes"].longLongValue : 33554432;
      if (!isfinite(seconds) || seconds <= 0 || seconds > 300 || limit < 2 || limit > 33554432) Fail(@"capture bounds invalid");
      if (![output[@"name"] isEqual:EndpointName]) Fail(@"actual selected CoreAudio output is not BlackHole 2ch");
      NSMutableArray<AVCaptureDevice *> *matching = [NSMutableArray array];
      for (AVCaptureDevice *device in devices) if ([device.localizedName isEqualToString:EndpointName]) [matching addObject:device];
      if (matching.count != 1 || !matching[0].connected) Fail(@"exactly one connected BlackHole 2ch input is required");
      recorder.device = matching[0];
      if (![output[@"uid"] isEqual:recorder.device.uniqueID]) Fail(@"native BlackHole input and selected output UIDs differ");
      AVAuthorizationStatus authorization = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
      if (authorization == AVAuthorizationStatusNotDetermined) {
        dispatch_semaphore_t consent = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) { (void)granted; dispatch_semaphore_signal(consent); }];
        if (dispatch_semaphore_wait(consent, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC))) Fail(@"genuine macOS audio consent timed out");
        authorization = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
      }
      if (authorization != AVAuthorizationStatusAuthorized) Fail([@"native audio authorization is " stringByAppendingString:AuthorizationName(authorization)]);
      recorder.endpoint = @{ @"inputName": recorder.device.localizedName, @"inputUID": recorder.device.uniqueID, @"output": output };
      recorder.readyPath = options[@"--ready"]; recorder.maxBytes = limit;
      recorder.raw = NewPrivateFile(options[@"--raw"]); recorder.packets = NewPrivateFile(options[@"--packets"]);
      recorder.session = [AVCaptureSession new];
      NSError *error = nil;
      AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:recorder.device error:&error];
      if (!input || ![recorder.session canAddInput:input]) Fail(error.localizedDescription ?: @"native input unavailable");
      [recorder.session addInput:input];
      AVCaptureAudioDataOutput *audio = [AVCaptureAudioDataOutput new];
      audio.audioSettings = @{ AVFormatIDKey: @(kAudioFormatLinearPCM), AVSampleRateKey: @48000, AVNumberOfChannelsKey: @1,
        AVLinearPCMBitDepthKey: @16, AVLinearPCMIsFloatKey: @NO, AVLinearPCMIsBigEndianKey: @NO, AVLinearPCMIsNonInterleaved: @NO };
      [audio setSampleBufferDelegate:recorder queue:queue];
      if (![recorder.session canAddOutput:audio]) Fail(@"native audio output unavailable");
      [recorder.session addOutput:audio];
      double before = UTCMS();
      recorder.anchorHostSeconds = CMTimeGetSeconds(CMClockGetTime(CMClockGetHostTimeClock()));
      double after = UTCMS(); recorder.anchorUTCMS = (before + after) / 2; recorder.uncertaintyMS = (after - before) / 2 + 1;
      [recorder.session startRunning];
      double began = NSProcessInfo.processInfo.systemUptime;
      double deadline = began + seconds;
      while (!recorder.failure && NSProcessInfo.processInfo.systemUptime < deadline &&
             ![NSFileManager.defaultManager fileExistsAtPath:options[@"--stop"]]) {
        NSDictionary *current = OutputDevice();
        if (![current isEqual:output] || !recorder.device.connected) { recorder.deviceChanged = YES; recorder.failure = @"native capture device changed"; break; }
        if ((!recorder.ready && NSProcessInfo.processInfo.systemUptime - began > 10) ||
            (recorder.ready && UTCMS() - recorder.lastEndMS > 1000)) {
          recorder.lostPackets = recorder.ready;
          recorder.failure = @"actual native PCM readiness/packet watchdog timed out";
          break;
        }
        [NSThread sleepForTimeInterval:0.05];
      }
    } @catch (NSException *exception) { recorder.failure = exception.reason ?: @"recorder setup failed"; }
    [recorder.session stopRunning];
    dispatch_sync(queue, ^{});
    if (recorder.raw) { if (fclose(recorder.raw)) recorder.failure = @"raw PCM close failed"; recorder.raw = NULL; }
    if (recorder.packets) { if (fclose(recorder.packets)) recorder.failure = @"packet timing close failed"; recorder.packets = NULL; }
    if (!recorder.ready && !recorder.failure) recorder.failure = @"no actual native PCM packets captured";
    BOOL passed = !recorder.failure && recorder.ready && !recorder.lostPackets && !recorder.deviceChanged;
    NSMutableDictionary *done = [@{ @"passed": @(passed), @"lostPackets": @(recorder.lostPackets), @"deviceChanged": @(recorder.deviceChanged),
      @"captureStartedAtMs": @(recorder.firstMS), @"captureEndedAtMs": @(recorder.lastEndMS), @"capturedBytes": @(recorder.bytes) } mutableCopy];
    if (recorder.failure) done[@"error"] = recorder.failure;
    if (options[@"--done"]) { @try { WriteJSON(options[@"--done"], done); } @catch (NSException *exception) { fprintf(stderr, "%s\n", exception.reason.UTF8String); return 1; } }
    if (!passed) fprintf(stderr, "%s\n", recorder.failure.UTF8String ?: "native recording failed");
    return passed ? 0 : 1;
  }
}
