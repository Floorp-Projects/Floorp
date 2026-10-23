# SPDX-License-Identifier: MPL-2.0

param(
  [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"

# This captures an entire desktop, so it must never run for a local test user.
if ($env:GITHUB_ACTIONS -cne "true" -or
    [Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
  throw "Desktop diagnostics are restricted to Windows GitHub Actions runners."
}

$expectedDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\logs\test"))
$requestedDirectory = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
  $expectedDirectory
} else {
  [IO.Path]::GetFullPath($OutputDirectory)
}
if (![string]::Equals(
    $requestedDirectory.TrimEnd([char[]]"\/"),
    $expectedDirectory.TrimEnd([char[]]"\/"),
    [StringComparison]::OrdinalIgnoreCase)) {
  throw "Desktop diagnostics must be written to the repository logs/test directory."
}

New-Item -ItemType Directory -Force -Path $expectedDirectory | Out-Null
$stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss-fff", [Globalization.CultureInfo]::InvariantCulture)
$basePath = Join-Path $expectedDirectory "windows-desktop-$stamp-$PID"
$imagePath = "$basePath.png"
$metadataPath = "$basePath.json"
$captureClock = [Diagnostics.Stopwatch]::StartNew()
$errors = [Collections.Generic.List[string]]::new()
$evidence = [ordered]@{
  capturedAt = [DateTime]::UtcNow.ToString("o")
  status = "starting"
  os = [Environment]::OSVersion.VersionString
  userInteractive = [Environment]::UserInteractive
  sessionId = $null
  dpiAware = $null
  virtualScreen = $null
  screens = @()
  processWindows = @()
  nativeStacks = @()
  screenshot = $null
  errors = @()
}

function Save-Evidence {
  $evidence.errors = $errors.ToArray()
  $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metadataPath -Encoding utf8
}

function Capture-NativeStack([object]$WindowInfo) {
  $report = [PSCustomObject][ordered]@{
    processId = $WindowInfo.processId
    status = "starting"
    uiThreadId = $null
    executable = $null
    threads = @()
    waitChain = $null
    debugger = $null
    log = $null
    stdout = $null
    stderr = $null
    exitCode = $null
    error = $null
  }
  # Checkpoint the same report object before any native query or debugger work.
  $evidence.nativeStacks = @($evidence.nativeStacks) + @($report)
  Save-Evidence
  $target = $null
  $debugger = $null
  try {
    $target = Get-Process -Id $WindowInfo.processId -ErrorAction Stop
    $expectedBrowser = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\_dist\bin\floorp\floorp.exe"))
    $report.executable = $target.MainModule.FileName
    if (![string]::Equals($report.executable, $expectedBrowser, [StringComparison]::OrdinalIgnoreCase) -or
        $target.SessionId -ne $evidence.sessionId -or
        $target.MainWindowHandle.ToInt64() -ne $WindowInfo.mainWindowHandle) {
      throw "The hung process no longer matches the repository's Windows browser window."
    }
    if (![FloorpTestDesktopNative]::IsHungAppWindow($target.MainWindowHandle)) {
      $report.status = "responsive-again"
      return [PSCustomObject]$report
    }
    $windowProcessId = [uint32]0
    $report.uiThreadId = [FloorpTestDesktopNative]::GetWindowThreadProcessId(
      $target.MainWindowHandle, [ref]$windowProcessId
    )
    if ($windowProcessId -ne $target.Id) { throw "The window process identity changed." }
    $report.status = "collecting-wait-chain"
    Save-Evidence
    $report.waitChain = [FloorpTestDesktopNative]::CaptureWaitChainBounded($report.uiThreadId, 2000)
    Save-Evidence
    $threadRows = [Collections.Generic.List[object]]::new()
    foreach ($thread in $target.Threads) {
      try {
        $state = $thread.ThreadState
        $threadRows.Add([PSCustomObject]@{
          threadId = $thread.Id
          state = [string]$state
          waitReason = if ($state -eq [Diagnostics.ThreadState]::Wait) {
            [string]$thread.WaitReason
          } else { $null }
        })
      } catch {
        $threadRows.Add([PSCustomObject]@{ threadId = $thread.Id; error = $_.Exception.Message })
      } finally {
        $thread.Dispose()
      }
    }
    $report.threads = $threadRows.ToArray()
    Save-Evidence
    $candidates = @(
      (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Debuggers\x64\cdb.exe"),
      (Join-Path $env:ProgramFiles "Windows Kits\10\Debuggers\x64\cdb.exe")
    )
    $cdb = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (!$cdb) {
      $report.status = "debugger-unavailable"
      return [PSCustomObject]$report
    }
    # Leave time for cleanup inside the caller's 45-second subprocess deadline.
    if ($captureClock.ElapsedMilliseconds -gt 20000) {
      $report.status = "capture-budget-exhausted"
      return [PSCustomObject]$report
    }
    $report.debugger = $cdb
    $stackPath = "$basePath-native-$($target.Id)-stack.txt"
    $stackOutputPath = "$basePath-native-$($target.Id)-stdout.txt"
    $stackErrorPath = "$basePath-native-$($target.Id)-stderr.txt"
    $report.log = [IO.Path]::GetFileName($stackPath)
    $report.stdout = [IO.Path]::GetFileName($stackOutputPath)
    $report.stderr = [IO.Path]::GetFileName($stackErrorPath)
    $report.status = "collecting-cdb-stack"
    Save-Evidence
    # Non-invasive attach, local symbols only, stack addresses/module names only.
    # No dump or arbitrary memory-read commands and no symbol-server requests.
    $commands = "lm; ~* k 0n64; .detach; q"
    $arguments = @(
      "-logo", ('"{0}"' -f $stackPath),
      "-pv", "-sins", "-netsyms:no", "-nosqm", "-noshell", "-p", [string]$target.Id,
      "-y", ('"{0}"' -f [IO.Path]::GetDirectoryName($report.executable)),
      "-c", ('"{0}"' -f $commands)
    )
    $debugger = Start-Process -FilePath $cdb -ArgumentList $arguments -NoNewWindow -PassThru `
      -RedirectStandardOutput $stackOutputPath -RedirectStandardError $stackErrorPath
    if (!$debugger.WaitForExit(20000)) {
      $report.status = "debugger-timeout"
      $debugger.Kill()
      [void]$debugger.WaitForExit(1000)
      throw "CDB exceeded its twenty-second diagnostic deadline."
    }
    $report.exitCode = $debugger.ExitCode
    if ($debugger.ExitCode -ne 0) { throw "CDB exited with code $($debugger.ExitCode)." }
    $report.status = "completed"
  } catch {
    $report.error = $_.Exception.Message
    if ($report.status -ne "debugger-timeout") { $report.status = "failed" }
    $errors.Add("Native stack PID $($WindowInfo.processId): $($_.Exception.Message)")
  } finally {
    if ($null -ne $debugger) {
      try {
        if (!$debugger.HasExited) {
          $debugger.Kill()
          [void]$debugger.WaitForExit(1000)
        }
      } finally { $debugger.Dispose() }
    }
    if ($null -ne $target) { $target.Dispose() }
  }
  return [PSCustomObject]$report
}

$bitmap = $null
$graphics = $null
try {
  # Keep a checkpoint even if a desktop API fails to return before the caller's
  # subprocess deadline. This script does not alter or activate any windows.
  Save-Evidence
  $evidence.sessionId = (Get-Process -Id $PID).SessionId
  Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

public sealed class FloorpWaitChainNode {
  public string objectType { get; set; }
  public int objectTypeCode { get; set; }
  public string objectStatus { get; set; }
  public int objectStatusCode { get; set; }
  public uint? processId { get; set; }
  public uint? threadId { get; set; }
  public uint? waitTime { get; set; }
  public uint? contextSwitches { get; set; }
  public string objectName { get; set; }
  public long? timeout100ns { get; set; }
  public bool? alertable { get; set; }
}

public sealed class FloorpWaitChainResult {
  public string status { get; set; }
  public int? errorCode { get; set; }
  public string error { get; set; }
  public bool? cycle { get; set; }
  public FloorpWaitChainNode[] nodes { get; set; }
}

public static class FloorpTestDesktopNative {
  [DllImport("user32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  public static extern bool SetProcessDPIAware();

  [DllImport("user32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  public static extern bool IsHungAppWindow(IntPtr window);

  [DllImport("user32.dll")]
  public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

  [DllImport("advapi32.dll", SetLastError = true)]
  private static extern IntPtr OpenThreadWaitChainSession(uint flags, IntPtr callback);

  [DllImport("advapi32.dll", SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool GetThreadWaitChain(
    IntPtr session, IntPtr context, uint flags, uint threadId, ref uint nodeCount,
    IntPtr nodes, [MarshalAs(UnmanagedType.Bool)] out bool cycle);

  [DllImport("advapi32.dll")]
  private static extern void CloseThreadWaitChainSession(IntPtr session);

  public static FloorpWaitChainResult CaptureWaitChainBounded(uint threadId, int timeoutMs) {
    // WCT has no synchronous deadline. Its worker makes no PowerShell callbacks,
    // so a stalled query cannot consume the debugger's time budget.
    Task<FloorpWaitChainResult> task = Task.Run(() => CaptureWaitChain(threadId));
    if (!task.Wait(timeoutMs)) {
      return new FloorpWaitChainResult { status = "timeout", nodes = new FloorpWaitChainNode[0] };
    }
    return task.Result;
  }

  private static FloorpWaitChainResult CaptureWaitChain(uint threadId) {
    FloorpWaitChainResult result = new FloorpWaitChainResult {
      status = "starting", nodes = new FloorpWaitChainNode[0]
    };
    IntPtr session = IntPtr.Zero;
    IntPtr buffer = IntPtr.Zero;
    try {
      session = OpenThreadWaitChainSession(0, IntPtr.Zero);
      if (session == IntPtr.Zero) {
        result.status = "failed";
        result.errorCode = Marshal.GetLastWin32Error();
        return result;
      }
      // WAITCHAIN_NODE_INFO: two DWORD enums, then an 8-byte aligned union.
      // WCHAR[128], LARGE_INTEGER, BOOL occupy the largest arm (size 280).
      const int nodeSize = 280;
      const int maxNodes = 16;
      buffer = Marshal.AllocHGlobal(nodeSize * maxNodes);
      Marshal.Copy(new byte[nodeSize * maxNodes], 0, buffer, nodeSize * maxNodes);
      uint count = maxNodes;
      bool cycle;
      // Flags 0 follow only this process; the first foreign thread is reported
      // but its wait chain is not traversed. No COM callbacks are registered.
      bool success = GetThreadWaitChain(session, IntPtr.Zero, 0, threadId, ref count, buffer, out cycle);
      int error = success ? 0 : Marshal.GetLastWin32Error();
      result.errorCode = error;
      if (!success && error != 234) { // ERROR_MORE_DATA still returns a partial chain.
        result.status = "failed";
        return result;
      }
      result.status = success ? "completed" : "partial";
      result.cycle = cycle;
      string[] types = { "Invalid", "CriticalSection", "SendMessage", "Mutex", "Alpc", "Com",
        "ThreadWait", "ProcessWait", "Thread", "ComActivation", "Unknown", "SocketIo", "SmbIo" };
      string[] statuses = { "Invalid", "NoAccess", "Running", "Blocked", "PidOnly", "PidOnlyRpcss",
        "Owned", "NotOwned", "Abandoned", "Unknown", "Error" };
      List<FloorpWaitChainNode> rows = new List<FloorpWaitChainNode>();
      for (int i = 0; i < Math.Min(count, (uint)maxNodes); i++) {
        IntPtr node = IntPtr.Add(buffer, i * nodeSize);
        int type = Marshal.ReadInt32(node, 0);
        int status = Marshal.ReadInt32(node, 4);
        FloorpWaitChainNode row = new FloorpWaitChainNode {
          objectTypeCode = type, objectType = type >= 0 && type < types.Length ? types[type] : "Unrecognized",
          objectStatusCode = status,
          objectStatus = status >= 0 && status < statuses.Length ? statuses[status] : "Unrecognized"
        };
        if (type == 8) { // WctThreadType selects the ThreadObject union arm.
          row.processId = unchecked((uint)Marshal.ReadInt32(node, 8));
          row.threadId = unchecked((uint)Marshal.ReadInt32(node, 12));
          row.waitTime = unchecked((uint)Marshal.ReadInt32(node, 16));
          row.contextSwitches = unchecked((uint)Marshal.ReadInt32(node, 20));
        } else {
          row.objectName = Marshal.PtrToStringUni(IntPtr.Add(node, 8), 128).TrimEnd('\0');
          row.timeout100ns = Marshal.ReadInt64(node, 264);
          row.alertable = Marshal.ReadInt32(node, 272) != 0;
        }
        rows.Add(row);
      }
      result.nodes = rows.ToArray();
    } catch (Exception error) {
      result.status = "failed";
      result.error = error.Message;
    } finally {
      if (buffer != IntPtr.Zero) Marshal.FreeHGlobal(buffer);
      if (session != IntPtr.Zero) CloseThreadWaitChainSession(session);
    }
    return result;
  }
}
'@
  $evidence.dpiAware = [FloorpTestDesktopNative]::SetProcessDPIAware()
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing

  $bounds = [Windows.Forms.SystemInformation]::VirtualScreen
  $evidence.virtualScreen = [ordered]@{
    x = $bounds.X
    y = $bounds.Y
    width = $bounds.Width
    height = $bounds.Height
  }
  $evidence.screens = @(
    [Windows.Forms.Screen]::AllScreens | ForEach-Object {
      [PSCustomObject]@{
        device = $_.DeviceName
        primary = $_.Primary
        x = $_.Bounds.X
        y = $_.Bounds.Y
        width = $_.Bounds.Width
        height = $_.Bounds.Height
      }
    }
  )
  $evidence.status = "capturing-screen"
  Save-Evidence

  if ($bounds.Width -le 0 -or $bounds.Height -le 0 -or
      ([long]$bounds.Width * [long]$bounds.Height) -gt 100000000) {
    throw "The CI desktop returned invalid or excessive capture dimensions."
  }
  $bitmap = [Drawing.Bitmap]::new($bounds.Width, $bounds.Height)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  $graphics.CopyFromScreen(
    $bounds.Location,
    [Drawing.Point]::Empty,
    $bounds.Size,
    [Drawing.CopyPixelOperation]::SourceCopy
  )
  $bitmap.Save($imagePath, [Drawing.Imaging.ImageFormat]::Png)
  $evidence.screenshot = [IO.Path]::GetFileName($imagePath)
  $evidence.status = "collecting-window-inventory"
  Save-Evidence

  $windows = [Collections.Generic.List[object]]::new()
  foreach ($process in Get-Process) {
    try {
      $windowHandle = $process.MainWindowHandle
      $isBrowserProcess = $process.ProcessName -match "^(floorp|noraneko)(-bin)?$"
      if ($windowHandle -eq [IntPtr]::Zero -and !$isBrowserProcess) { continue }
      $windows.Add([PSCustomObject]@{
        processId = $process.Id
        processName = $process.ProcessName
        sessionId = $process.SessionId
        mainWindowTitle = $process.MainWindowTitle
        mainWindowHandle = $windowHandle.ToInt64()
        # This query does not send input or window messages to the application.
        responding = if ($windowHandle -eq [IntPtr]::Zero) {
          $null
        } else {
          ![FloorpTestDesktopNative]::IsHungAppWindow($windowHandle)
        }
      })
    } catch {
      $errors.Add("Window inventory PID $($process.Id): $($_.Exception.Message)")
    } finally {
      $process.Dispose()
    }
  }
  $evidence.processWindows = $windows.ToArray()
  $evidence.status = "collecting-native-stacks"
  Save-Evidence
  foreach ($window in $windows) {
    if ($window.responding -eq $false -and $window.processName -match "^(floorp|noraneko)(-bin)?$") {
      [void](Capture-NativeStack $window)
      Save-Evidence
    }
  }
  $evidence.status = if ($errors.Count -eq 0) { "completed" } else { "partial" }
} catch {
  $errors.Add($_.Exception.Message)
  $evidence.status = "failed"
} finally {
  if ($null -ne $graphics) { $graphics.Dispose() }
  if ($null -ne $bitmap) { $bitmap.Dispose() }
  Save-Evidence
}

Write-Output "Windows desktop diagnostics: $metadataPath"
if ($errors.Count -gt 0) {
  foreach ($captureError in $errors) { Write-Warning $captureError }
  exit 1
}
exit 0
