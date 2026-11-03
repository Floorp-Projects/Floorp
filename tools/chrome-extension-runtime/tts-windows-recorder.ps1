# SPDX-License-Identifier: MPL-2.0
# Test-only recorder. Compile/probe provenance is recorded separately from
# pending actual browser TTS waveform qualification; see TTS_WAVEFORM.md.
[CmdletBinding()]
param(
  [string]$RawPath,
  [string]$PacketsPath,
  [string]$ReadyPath,
  [string]$DonePath,
  [string]$StopPath,
  [switch]$CompileOnly,
  [switch]$Probe,
  [ValidateRange(1, 300)][int]$MaxSeconds = 90,
  [ValidateRange(2, 33554432)][long]$MaxBytes = 33554432
)
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
  throw 'The scratch WASAPI recorder requires Windows.'
}
if ($CompileOnly -and $Probe) {
  throw 'Choose only one preflight mode: -CompileOnly or -Probe.'
}

$source = @'
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace FloorpScratchWasapi {
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    internal class DeviceEnumeratorClass { }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IDeviceEnumerator {
        [PreserveSig] int EnumAudioEndpoints(int flow, uint mask, out IntPtr devices);
        [PreserveSig] int GetDefaultAudioEndpoint(int flow, int role, out IDevice device);
        [PreserveSig] int GetDevice([MarshalAs(UnmanagedType.LPWStr)] string id, out IDevice device);
        [PreserveSig] int RegisterEndpointNotificationCallback(IEndpointNotification callback);
        [PreserveSig] int UnregisterEndpointNotificationCallback(IEndpointNotification callback);
    }

    [ComVisible(true), Guid("7991EEC9-7E89-4D85-8390-6C703CEC60C0"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IEndpointNotification {
        [PreserveSig] int OnDeviceStateChanged([MarshalAs(UnmanagedType.LPWStr)] string id, uint state);
        [PreserveSig] int OnDeviceAdded([MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnDeviceRemoved([MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnDefaultDeviceChanged(int flow, int role, [MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnPropertyValueChanged([MarshalAs(UnmanagedType.LPWStr)] string id, PropertyKey key);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PropertyKey { public Guid FormatId; public uint PropertyId; }

    [ComVisible(true), ClassInterface(ClassInterfaceType.None)]
    public sealed class EndpointNotification : IEndpointNotification {
        private readonly string initial;
        internal volatile bool Changed;
        internal EndpointNotification(string id) { initial = id; }
        public int OnDeviceStateChanged(string id, uint state) {
            if (String.Equals(id, initial, StringComparison.Ordinal) && (state & 1) == 0) Changed = true;
            return 0;
        }
        public int OnDeviceAdded(string id) { return 0; }
        public int OnDeviceRemoved(string id) {
            if (String.Equals(id, initial, StringComparison.Ordinal)) Changed = true;
            return 0;
        }
        public int OnDefaultDeviceChanged(int flow, int role, string id) {
            if (flow == 0 && role == 0 && !String.Equals(id, initial, StringComparison.Ordinal)) Changed = true;
            return 0;
        }
        public int OnPropertyValueChanged(string id, PropertyKey key) { return 0; }
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IDevice {
        [PreserveSig] int Activate(ref Guid iid, uint context, IntPtr parameters,
            [MarshalAs(UnmanagedType.IUnknown)] out object result);
        [PreserveSig] int OpenPropertyStore(uint access, out IntPtr store);
        [PreserveSig] int GetId(out IntPtr id);
        [PreserveSig] int GetState(out uint state);
    }

    [ComImport, Guid("1CB9AD4C-DBFA-4C32-B178-C2F568A703B2"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IAudioClient {
        [PreserveSig] int Initialize(int shareMode, uint flags, long duration,
            long periodicity, IntPtr format, IntPtr sessionGuid);
        [PreserveSig] int GetBufferSize(out uint frames);
        [PreserveSig] int GetStreamLatency(out long latency);
        [PreserveSig] int GetCurrentPadding(out uint frames);
        [PreserveSig] int IsFormatSupported(int mode, IntPtr format, out IntPtr closest);
        [PreserveSig] int GetMixFormat(out IntPtr format);
        [PreserveSig] int GetDevicePeriod(out long defaultPeriod, out long minimumPeriod);
        [PreserveSig] int Start();
        [PreserveSig] int Stop();
        [PreserveSig] int Reset();
        [PreserveSig] int SetEventHandle(IntPtr handle);
        [PreserveSig] int GetService(ref Guid iid,
            [MarshalAs(UnmanagedType.IUnknown)] out object service);
    }

    [ComImport, Guid("C8ADBD64-E71E-48A0-A4DE-185C395CD317"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IAudioCaptureClient {
        [PreserveSig] int GetBuffer(out IntPtr data, out uint frames, out uint flags,
            out ulong devicePosition, out ulong qpcPosition);
        [PreserveSig] int ReleaseBuffer(uint frames);
        [PreserveSig] int GetNextPacketSize(out uint frames);
    }

    internal sealed class MixFormat {
        internal ushort Tag, Channels, Bits, ValidBits, BlockAlign;
        internal uint Rate, AverageBytes, ChannelMask;
        internal Guid SubFormat;
        internal bool Float;
        internal int ContainerBytes;

        internal static MixFormat Read(IntPtr pointer) {
            MixFormat f = new MixFormat();
            f.Tag = unchecked((ushort)Marshal.ReadInt16(pointer, 0));
            f.Channels = unchecked((ushort)Marshal.ReadInt16(pointer, 2));
            f.Rate = unchecked((uint)Marshal.ReadInt32(pointer, 4));
            f.AverageBytes = unchecked((uint)Marshal.ReadInt32(pointer, 8));
            f.BlockAlign = unchecked((ushort)Marshal.ReadInt16(pointer, 12));
            f.Bits = unchecked((ushort)Marshal.ReadInt16(pointer, 14));
            ushort extra = unchecked((ushort)Marshal.ReadInt16(pointer, 16));
            if (extra > 256) throw new InvalidDataException("Unexpected mix-format extension size.");
            f.ValidBits = f.Bits;
            if (f.Tag == 0xfffe) {
                if (extra < 22) throw new InvalidDataException("Short WAVEFORMATEXTENSIBLE.");
                f.ValidBits = unchecked((ushort)Marshal.ReadInt16(pointer, 18));
                f.ChannelMask = unchecked((uint)Marshal.ReadInt32(pointer, 20));
                byte[] guid = new byte[16];
                Marshal.Copy(IntPtr.Add(pointer, 24), guid, 0, guid.Length);
                f.SubFormat = new Guid(guid);
                if (f.SubFormat == new Guid("00000003-0000-0010-8000-00aa00389b71")) f.Float = true;
                else if (f.SubFormat != new Guid("00000001-0000-0010-8000-00aa00389b71"))
                    throw new InvalidDataException("Unsupported mix SubFormat " + f.SubFormat);
            } else if (f.Tag == 3) f.Float = true;
            else if (f.Tag != 1) throw new InvalidDataException("Unsupported mix tag " + f.Tag);
            if (f.Channels < 1 || f.Channels > 32 || f.Rate < 8000 || f.Rate > 384000)
                throw new InvalidDataException("Mix channel count/sample rate is outside the recorder bound.");
            if (f.Float) {
                if (f.Bits != 32 || f.ValidBits != 32)
                    throw new InvalidDataException("Only float32 mix samples are supported.");
            } else if ((f.Bits != 8 && f.Bits != 16 && f.Bits != 24 && f.Bits != 32) ||
                       f.ValidBits < 1 || f.ValidBits > f.Bits)
                throw new InvalidDataException("Unsupported PCM container/valid-bit count.");
            f.ContainerBytes = f.Bits / 8;
            if (f.BlockAlign != f.Channels * f.ContainerBytes ||
                f.AverageBytes != checked(f.Rate * (uint)f.BlockAlign))
                throw new InvalidDataException("Inconsistent native mix block alignment/rate.");
            return f;
        }

        internal double Sample(byte[] bytes, int offset) {
            if (Float) {
                double value = BitConverter.ToSingle(bytes, offset);
                if (Double.IsNaN(value) || Double.IsInfinity(value))
                    throw new InvalidDataException("Non-finite native audio sample.");
                return value;
            }
            int signed;
            if (Bits == 8) signed = bytes[offset] - 128;
            else if (Bits == 16) signed = unchecked((short)(bytes[offset] | (bytes[offset + 1] << 8)));
            else if (Bits == 24) {
                signed = bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16);
                if ((signed & 0x800000) != 0) signed |= unchecked((int)0xff000000);
            } else signed = unchecked((int)((uint)bytes[offset] | ((uint)bytes[offset + 1] << 8) |
                ((uint)bytes[offset + 2] << 16) | ((uint)bytes[offset + 3] << 24)));
            signed >>= Bits - ValidBits;
            return signed / Math.Pow(2.0, ValidBits - 1);
        }

        internal object Evidence() {
            return Recorder.Fields("formatTag", Tag, "channels", Channels, "sampleRate", Rate,
                "containerBits", Bits, "validBits", ValidBits, "blockAlign", BlockAlign,
                "averageBytesPerSecond", AverageBytes, "channelMask", ChannelMask,
                "subFormat", SubFormat.ToString(), "representation", Float ? "float32" : "pcm");
        }
    }

    public static class Recorder {
        private const uint Loopback = 0x20000;
        private const uint Silent = 2;
        private const uint Discontinuity = 1;
        private const uint TimestampError = 4;
        private const int MaximumPacketBytes = 16 * 1024 * 1024;
        private const long MaximumPacketCount = 100000;

        internal static Dictionary<string, object> Fields(params object[] pairs) {
            Dictionary<string, object> result = new Dictionary<string, object>();
            for (int i = 0; i < pairs.Length; i += 2) result.Add((string)pairs[i], pairs[i + 1]);
            return result;
        }

        private static double NowMs() { return DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(); }
        private static void CheckClock(double qpcToEpochOffsetMs) {
            double calibratedNow = Stopwatch.GetTimestamp() * 1000.0 / Stopwatch.Frequency + qpcToEpochOffsetMs;
            if (Math.Abs(NowMs() - calibratedNow) > 50)
                throw new InvalidDataException("Host wall clock changed relative to native QPC during capture.");
        }
        private static string Json(object value) {
            if (value == null) return "null";
            string text = value as string;
            if (text != null) {
                StringBuilder result = new StringBuilder("\"");
                foreach (char c in text) {
                    if (c == '\\' || c == '"') result.Append('\\').Append(c);
                    else if (c < 0x20) result.Append("\\u").Append(((int)c).ToString("x4"));
                    else result.Append(c);
                }
                return result.Append('"').ToString();
            }
            if (value is bool) return (bool)value ? "true" : "false";
            IDictionary dictionary = value as IDictionary;
            if (dictionary != null) {
                List<string> entries = new List<string>();
                foreach (DictionaryEntry entry in dictionary)
                    entries.Add(Json((string)entry.Key) + ":" + Json(entry.Value));
                return "{" + String.Join(",", entries.ToArray()) + "}";
            }
            if (value is double) {
                double d = (double)value;
                if (Double.IsNaN(d) || Double.IsInfinity(d)) throw new InvalidDataException("Non-finite JSON value.");
                return d.ToString("R", CultureInfo.InvariantCulture);
            }
            IFormattable number = value as IFormattable;
            if (number != null) return number.ToString(null, CultureInfo.InvariantCulture);
            throw new InvalidDataException("Unsupported metadata value type.");
        }

        private static void AtomicJson(string path, object value) {
            string temporary = path + ".tmp";
            using (FileStream file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (StreamWriter writer = new StreamWriter(file, new UTF8Encoding(false))) writer.WriteLine(Json(value));
            File.Move(temporary, path);
        }

        private static void Check(int hr, string operation) {
            if (hr < 0) throw new COMException(operation + " failed: 0x" +
                unchecked((uint)hr).ToString("x8", CultureInfo.InvariantCulture), hr);
        }

        private static string DeviceId(IDevice device) {
            IntPtr pointer = IntPtr.Zero;
            try {
                Check(device.GetId(out pointer), "IMMDevice.GetId");
                string id = Marshal.PtrToStringUni(pointer);
                if (String.IsNullOrEmpty(id) || id.Length > 4096) throw new InvalidDataException("Invalid endpoint ID.");
                return id;
            } finally { if (pointer != IntPtr.Zero) Marshal.FreeCoTaskMem(pointer); }
        }

        private static void Release(object value) {
            if (value != null && Marshal.IsComObject(value)) Marshal.ReleaseComObject(value);
        }

        public static string Probe() {
            string evidence = null;
            Exception failure = null;
            Thread thread = new Thread(delegate() {
                IDeviceEnumerator enumerator = null;
                IDevice device = null;
                IAudioClient audio = null;
                IntPtr pointer = IntPtr.Zero;
                try {
                    enumerator = (IDeviceEnumerator)new DeviceEnumeratorClass();
                    Check(enumerator.GetDefaultAudioEndpoint(0, 0, out device), "Probe GetDefaultAudioEndpoint(eRender,eConsole)");
                    string id = DeviceId(device);
                    uint state;
                    Check(device.GetState(out state), "Probe IMMDevice.GetState");
                    if ((state & 1) == 0) throw new InvalidDataException("Probe default render endpoint is not active.");
                    Guid iid = typeof(IAudioClient).GUID;
                    object native;
                    Check(device.Activate(ref iid, 0x17, IntPtr.Zero, out native), "Probe IMMDevice.Activate(IAudioClient)");
                    audio = (IAudioClient)native;
                    Check(audio.GetMixFormat(out pointer), "Probe IAudioClient.GetMixFormat");
                    if (pointer == IntPtr.Zero) throw new InvalidDataException("Probe native mix format is null.");
                    MixFormat format = MixFormat.Read(pointer);
                    evidence = Json(Fields("mode", "probe", "passed", true,
                        "backend", "wasapi-render-loopback", "endpoint", id, "endpointState", state,
                        "role", "eRender/eConsole", "nativeFormat", format.Evidence(),
                        "runtimeVersion", Environment.Version.ToString(), "os", Environment.OSVersion.ToString(),
                        "is64BitProcess", Environment.Is64BitProcess, "observedAtMs", NowMs(),
                        "audioClientInitialized", false, "captureStarted", false,
                        "audioPacketsRead", 0, "nativeQualified", false));
                } catch (Exception error) { failure = error; }
                finally {
                    if (pointer != IntPtr.Zero) Marshal.FreeCoTaskMem(pointer);
                    Release(audio); Release(device); Release(enumerator);
                }
            });
            thread.IsBackground = true;
            thread.SetApartmentState(ApartmentState.MTA);
            thread.Start();
            if (!thread.Join(15000)) throw new TimeoutException("Owned endpoint probe exceeded 15 seconds.");
            if (failure != null) throw new InvalidOperationException("Native endpoint probe failed.", failure);
            if (evidence == null) throw new InvalidDataException("Native probe returned no evidence.");
            return evidence;
        }

        public static void Run(string raw, string packets, string ready, string done, string stop,
                               int timeoutSeconds, long maximumBytes) {
            if (timeoutSeconds < 1 || timeoutSeconds > 300 || maximumBytes < 2 || maximumBytes > 33554432)
                throw new ArgumentOutOfRangeException("Capture exceeds the 300s/32MiB bound.");
            string[] paths = { raw, packets, ready, done, stop };
            HashSet<string> unique = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string path in paths) {
                string absolute = Path.GetFullPath(path);
                if (!unique.Add(absolute) || File.Exists(absolute))
                    throw new IOException("Recorder paths must be unique and not pre-existing: " + absolute);
                Directory.CreateDirectory(Path.GetDirectoryName(absolute));
            }
            Exception failure = null;
            Thread thread = new Thread(delegate() {
                try { Capture(raw, packets, ready, done, stop, timeoutSeconds, maximumBytes); }
                catch (Exception error) { failure = error; }
            });
            thread.IsBackground = true;
            // Current GitHub Windows runners are Windows10+/Server2022+. MTA keeps
            // IMMNotificationClient callbacks live without an STA UI-message pump.
            // This prototype intentionally does not target the historical Win8 STA case.
            thread.SetApartmentState(ApartmentState.MTA);
            thread.Start();
            if (!thread.Join((timeoutSeconds + 15) * 1000))
                throw new TimeoutException("Owned recorder thread exceeded its cleanup deadline; terminate the recorder process.");
            if (failure != null) throw new InvalidOperationException("Native loopback recording failed.", failure);
        }

        private static void Capture(string rawPath, string packetPath, string readyPath, string donePath, string stopPath,
                                    int timeoutSeconds, long maximumBytes) {
            IDeviceEnumerator enumerator = null;
            IDevice device = null;
            IAudioClient audio = null;
            IAudioCaptureClient capture = null;
            EndpointNotification notification = null;
            bool notificationRegistered = false;
            IntPtr formatPointer = IntPtr.Zero;
            FileStream raw = null;
            StreamWriter packetWriter = null;
            MixFormat format = null;
            bool started = false, complete = false, switched = false;
            string endpoint = null, reason = "failed";
            Exception error = null;
            long count = 0, framesTotal = 0, signalFrames = 0, silentFrames = 0, clipped = 0;
            long emptyPolls = 0, discontinuities = 0, timestampErrors = 0;
            bool initialDiscontinuity = false;
            double startMs = 0, endMs = 0, firstPacketMs = 0, lastPacketMs = 0;
            double nativePeak = 0, maxEmptyPollSpanMs = 0, emptySinceMs = 0;
            double calibrationOffsetMs = 0, uncertaintyMs = 0;
            Stopwatch elapsed = new Stopwatch();
            try {
                enumerator = (IDeviceEnumerator)new DeviceEnumeratorClass();
                Check(enumerator.GetDefaultAudioEndpoint(0, 0, out device), "GetDefaultAudioEndpoint(eRender,eConsole)");
                endpoint = DeviceId(device);
                notification = new EndpointNotification(endpoint);
                Check(enumerator.RegisterEndpointNotificationCallback(notification), "RegisterEndpointNotificationCallback");
                notificationRegistered = true;
                uint state;
                Check(device.GetState(out state), "IMMDevice.GetState");
                if ((state & 1) == 0) throw new InvalidDataException("Default render endpoint is not active.");
                Guid clientIid = typeof(IAudioClient).GUID;
                object nativeClient;
                Check(device.Activate(ref clientIid, 0x17, IntPtr.Zero, out nativeClient), "IMMDevice.Activate(IAudioClient)");
                audio = (IAudioClient)nativeClient;
                Check(audio.GetMixFormat(out formatPointer), "IAudioClient.GetMixFormat");
                format = MixFormat.Read(formatPointer);
                Check(audio.Initialize(0, Loopback, 2000000L, 0, formatPointer, IntPtr.Zero),
                    "IAudioClient.Initialize(SHARED,LOOPBACK,native-mix-format)");
                uint capacity;
                Check(audio.GetBufferSize(out capacity), "IAudioClient.GetBufferSize");
                Guid captureIid = typeof(IAudioCaptureClient).GUID;
                object nativeCapture;
                Check(audio.GetService(ref captureIid, out nativeCapture), "IAudioClient.GetService(IAudioCaptureClient)");
                capture = (IAudioCaptureClient)nativeCapture;
                raw = new FileStream(rawPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
                packetWriter = new StreamWriter(new FileStream(packetPath, FileMode.CreateNew, FileAccess.Write,
                    FileShare.Read), new UTF8Encoding(false));
                packetWriter.AutoFlush = true;
                long before = Stopwatch.GetTimestamp();
                double epoch = NowMs();
                long after = Stopwatch.GetTimestamp();
                calibrationOffsetMs = epoch - ((before / 2.0 + after / 2.0) * 1000.0 / Stopwatch.Frequency);
                uncertaintyMs = (after - before) * 500.0 / Stopwatch.Frequency + 1.0 + 1000.0 / format.Rate;
                startMs = NowMs();
                Check(audio.Start(), "IAudioClient.Start");
                started = true;
                elapsed.Start();
                emptySinceMs = startMs;
                AtomicJson(readyPath, Fields("backend", "wasapi-render-loopback", "endpoint", endpoint,
                    "role", "eRender/eConsole", "sampleRate", format.Rate, "channels", 1,
                    "sampleFormat", "s16le", "nativeFormat", format.Evidence(), "bufferCapacityFrames", capacity,
                    "readyAtMs", NowMs(), "captureStartedAtMs", startMs, "timeoutSeconds", timeoutSeconds,
                    "maximumBytes", maximumBytes, "stopPath", Path.GetFullPath(stopPath),
                    "qpcToEpochOffsetMs", calibrationOffsetMs, "uncertaintyMs", uncertaintyMs,
                    "stopwatchFrequency", Stopwatch.Frequency, "raw", Path.GetFullPath(rawPath),
                    "packets", Path.GetFullPath(packetPath), "done", Path.GetFullPath(donePath),
                    "endpointChangeMonitor", "IMMNotificationClient+final-default-id-check",
                    "nativeNoPacketsMeanRenderSilence", true,
                    "emptyIntervalCondition", "Only valid if final passed and no packet loss/device change; no positive signal is created.",
                    "timeline", "native-packet-concatenation-no-inserted-gap-frames"));
                Console.WriteLine("READY " + Path.GetFullPath(readyPath));
                bool stopRequested = false;
                while (!stopRequested) {
                    CheckClock(calibrationOffsetMs);
                    if (notification.Changed) {
                        switched = true;
                        throw new InvalidDataException("Default render endpoint changed or became inactive.");
                    }
                    if (File.Exists(stopPath)) { reason = "owned-stop-file"; break; }
                    if (elapsed.Elapsed.TotalSeconds >= timeoutSeconds)
                        throw new TimeoutException("Native loopback capture reached its timeout without owned stop-file.");
                    uint next;
                    Check(capture.GetNextPacketSize(out next), "IAudioCaptureClient.GetNextPacketSize");
                    if (next == 0) { emptyPolls++; Thread.Sleep(10); continue; }
                    while (next != 0) {
                        CheckClock(calibrationOffsetMs);
                        if (notification.Changed) {
                            switched = true;
                            throw new InvalidDataException("Default render endpoint changed while draining packets.");
                        }
                        if (File.Exists(stopPath)) { stopRequested = true; reason = "owned-stop-file"; break; }
                        if (elapsed.Elapsed.TotalSeconds >= timeoutSeconds)
                            throw new TimeoutException("Native packet drain reached its capture timeout.");
                        IntPtr data;
                        uint frames, flags;
                        ulong position, qpc;
                        int hr = capture.GetBuffer(out data, out frames, out flags, out position, out qpc);
                        Check(hr, "IAudioCaptureClient.GetBuffer");
                        // AUDCLNT_S_BUFFER_EMPTY is a positive success, not an acquired packet.
                        if (frames == 0) break;
                        try {
                            if (hr != 0) throw new InvalidDataException("Unexpected nonempty GetBuffer success status.");
                            if (++count > MaximumPacketCount) throw new IOException("Native packet metadata bound exceeded.");
                            int inputBytes = checked((int)((long)frames * format.BlockAlign));
                            int outputBytes = checked((int)((long)frames * 2));
                            if (inputBytes > MaximumPacketBytes || raw.Length + outputBytes > maximumBytes)
                                throw new IOException("Native audio packet/output exceeds bounded capture size.");
                            if ((flags & TimestampError) != 0) {
                                timestampErrors++;
                                throw new InvalidDataException("Native packet has AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR.");
                            }
                            if ((flags & Discontinuity) != 0) {
                                if (count == 1) initialDiscontinuity = true;
                                else discontinuities++;
                            }
                            bool silent = (flags & Silent) != 0;
                            byte[] input = new byte[silent ? 0 : inputBytes];
                            byte[] output = new byte[outputBytes];
                            if (!silent) {
                                if (data == IntPtr.Zero) throw new InvalidDataException("Non-silent packet has a null buffer.");
                                Marshal.Copy(data, input, 0, input.Length);
                            }
                            double packetNativePeak = 0, monoSquared = 0;
                            long packetSignalFrames = 0, packetClipped = 0;
                            for (int frame = 0; frame < frames; frame++) {
                                double mono = 0, channelPeak = 0;
                                if (!silent) {
                                    int baseOffset = checked(frame * format.BlockAlign);
                                    for (int channel = 0; channel < format.Channels; channel++) {
                                        double sample = format.Sample(input, baseOffset + channel * format.ContainerBytes);
                                        mono += sample / format.Channels;
                                        channelPeak = Math.Max(channelPeak, Math.Abs(sample));
                                    }
                                    if (channelPeak > 0) packetSignalFrames++;
                                    packetNativePeak = Math.Max(packetNativePeak, channelPeak);
                                }
                                monoSquared += mono * mono;
                                if (mono < -1 || mono > 1) packetClipped++;
                                double bounded = Math.Max(-1, Math.Min(1, mono));
                                int pcm = bounded < 0 ? (int)Math.Round(bounded * 32768.0) :
                                    (int)Math.Round(bounded * 32767.0);
                                output[frame * 2] = unchecked((byte)pcm);
                                output[frame * 2 + 1] = unchecked((byte)(pcm >> 8));
                            }
                            double observedAtMs = NowMs();
                            if (emptySinceMs != 0) {
                                maxEmptyPollSpanMs = Math.Max(maxEmptyPollSpanMs, observedAtMs - emptySinceMs);
                                emptySinceMs = 0;
                            }
                            if (count == 1) firstPacketMs = observedAtMs;
                            lastPacketMs = observedAtMs;
                            long offset = raw.Position;
                            raw.Write(output, 0, output.Length);
                            packetWriter.WriteLine(Json(Fields("byteOffset", offset, "frameCount", frames,
                                "firstSampleAtMs", qpc / 10000.0 + calibrationOffsetMs,
                                "uncertaintyMs", uncertaintyMs, "silent", silent, "flags", flags,
                                "devicePosition", position, "qpc", qpc, "qpcUnit", "100ns",
                                "observedAtMs", observedAtMs, "nativeChannelPeak", packetNativePeak,
                                "monoRms", Math.Sqrt(monoSquared / frames), "nativeSignalFrameCount", packetSignalFrames,
                                "clippedMonoFrameCount", packetClipped, "insertedGapFrames", 0)));
                            framesTotal += frames;
                            signalFrames += packetSignalFrames;
                            if (silent) silentFrames += frames;
                            clipped += packetClipped;
                            nativePeak = Math.Max(nativePeak, packetNativePeak);
                        } finally { Check(capture.ReleaseBuffer(frames), "IAudioCaptureClient.ReleaseBuffer"); }
                        Check(capture.GetNextPacketSize(out next), "IAudioCaptureClient.GetNextPacketSize");
                    }
                    if (emptySinceMs == 0) emptySinceMs = NowMs();
                }
                IDevice finalDevice = null;
                try {
                    Check(enumerator.GetDefaultAudioEndpoint(0, 0, out finalDevice), "Final default endpoint lookup");
                    switched = notification.Changed || !String.Equals(endpoint, DeviceId(finalDevice), StringComparison.Ordinal);
                    if (switched) throw new InvalidDataException("Default render endpoint changed during capture.");
                } finally { Release(finalDevice); }
                if (discontinuities != 0) throw new InvalidDataException("Native loopback packet discontinuity detected.");
                complete = true;
            } catch (Exception failure) { error = failure; complete = false; }
            finally {
                endMs = NowMs();
                if (started) {
                    try { Check(audio.Stop(), "IAudioClient.Stop"); }
                    catch (Exception failure) { if (error == null) error = failure; complete = false; }
                }
                if (emptySinceMs != 0) maxEmptyPollSpanMs = Math.Max(maxEmptyPollSpanMs, endMs - emptySinceMs);
                if (raw != null) raw.Dispose();
                if (packetWriter != null) packetWriter.Dispose();
                if (notificationRegistered) {
                    try { Check(enumerator.UnregisterEndpointNotificationCallback(notification), "UnregisterEndpointNotificationCallback"); }
                    catch (Exception failure) { if (error == null) error = failure; complete = false; }
                    switched = switched || notification.Changed;
                    if (switched) {
                        if (error == null) error = new InvalidDataException("Default endpoint notification changed during cleanup.");
                        complete = false;
                    }
                }
                Release(capture); Release(audio); Release(device); Release(enumerator);
                GC.KeepAlive(notification);
                if (formatPointer != IntPtr.Zero) Marshal.FreeCoTaskMem(formatPointer);
                AtomicJson(donePath, Fields("backend", "wasapi-render-loopback", "completed", complete, "passed", complete,
                    "endpoint", endpoint, "role", "eRender/eConsole", "deviceChanged", switched,
                    "endpointChangeMonitor", "IMMNotificationClient+final-default-id-check",
                    "nativeFormat", format == null ? null : format.Evidence(), "sampleRate", format == null ? 0 : format.Rate,
                    "channels", 1, "sampleFormat", "s16le", "captureStartedAtMs", startMs,
                    "captureEndedAtMs", endMs, "stopReason", reason, "packetCount", count,
                    "frameCount", framesTotal, "nativeSignalFrameCount", signalFrames,
                    "nativeSilentFrameCount", silentFrames, "nativeChannelPeak", nativePeak,
                    "clippedMonoFrameCount", clipped, "emptyPollCount", emptyPolls,
                    "maximumObservedEmptyPollSpanMs", maxEmptyPollSpanMs,
                    "firstPacketObservedAtMs", firstPacketMs, "lastPacketObservedAtMs", lastPacketMs,
                    "packetDiscontinuityCount", discontinuities, "initialDiscontinuityFlag", initialDiscontinuity,
                    "timestampErrorCount", timestampErrors, "insertedGapFrames", 0,
                    "lostPackets", discontinuities != 0, "qpcToEpochOffsetMs", calibrationOffsetMs,
                    "uncertaintyMs", uncertaintyMs, "stopwatchFrequency", Stopwatch.Frequency,
                    "timeline", "native-packet-concatenation-no-inserted-gap-frames",
                    "emptyPollingMeaning", "No packet returned; not a positive audio sample or synthesized silence.",
                    "nativeQualified", false, "error", error == null ? null : error.ToString()));
            }
            if (error != null) throw error;
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
if ($CompileOnly) {
  [ordered]@{
    mode = 'compile-only'
    passed = $true
    type = [FloorpScratchWasapi.Recorder].FullName
    runtimeVersion = [Environment]::Version.ToString()
    os = [Environment]::OSVersion.ToString()
    is64BitProcess = [Environment]::Is64BitProcess
    powershellVersion = $PSVersionTable.PSVersion.ToString()
    audioClientInitialized = $false
    captureStarted = $false
    audioPacketsRead = 0
    nativeQualified = $false
  } | ConvertTo-Json -Depth 5
  exit 0
}
if ($Probe) {
  [FloorpScratchWasapi.Recorder]::Probe()
  exit 0
}
foreach ($path in @($RawPath, $PacketsPath, $ReadyPath, $DonePath, $StopPath)) {
  if ([string]::IsNullOrWhiteSpace($path)) {
    throw 'Recording requires -RawPath, -PacketsPath, -ReadyPath, -DonePath, and -StopPath.'
  }
}
[FloorpScratchWasapi.Recorder]::Run(
  [IO.Path]::GetFullPath($RawPath),
  [IO.Path]::GetFullPath($PacketsPath),
  [IO.Path]::GetFullPath($ReadyPath),
  [IO.Path]::GetFullPath($DonePath),
  [IO.Path]::GetFullPath($StopPath),
  $MaxSeconds,
  $MaxBytes
)
