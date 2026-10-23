# SPDX-License-Identifier: MPL-2.0

param(
  [Parameter(Mandatory = $true)]
  [ValidateRange(1, 2147483647)]
  [int]$BrowserProcessId,
  [Parameter(Mandatory = $true)]
  [ValidatePattern('^0x[0-9A-Fa-f]+$')]
  [string]$WindowHandle,
  [Parameter(Mandatory = $true)]
  [ValidateSet("Previous", "Next")]
  [string]$Direction,
  [ValidateRange(1, 8)]
  [int]$RepeatCount = 1
)

$ErrorActionPreference = "Stop"

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
  throw "Native workspace key input requires Windows."
}
if (![Environment]::UserInteractive) {
  throw "Native workspace key input requires an interactive Windows desktop."
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class FloorpWorkspaceInputNative {
  [StructLayout(LayoutKind.Sequential)]
  public struct KEYBDINPUT {
    public ushort wVk;
    public ushort wScan;
    public uint dwFlags;
    public uint time;
    public UIntPtr dwExtraInfo;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct MOUSEINPUT {
    public int dx;
    public int dy;
    public uint mouseData;
    public uint dwFlags;
    public uint time;
    public UIntPtr dwExtraInfo;
  }
  [StructLayout(LayoutKind.Explicit)]
  public struct INPUTUNION {
    [FieldOffset(0)] public KEYBDINPUT ki;
    [FieldOffset(0)] public MOUSEINPUT mi;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct INPUT {
    public uint type;
    public INPUTUNION data;
  }

  [DllImport("user32.dll", SetLastError = true)]
  public static extern uint SendInput(uint count, INPUT[] inputs, int size);
  [DllImport("user32.dll")]
  public static extern uint MapVirtualKey(uint code, uint mapType);
  [DllImport("user32.dll")]
  public static extern short GetAsyncKeyState(int virtualKey);
  [DllImport("user32.dll")]
  public static extern bool IsWindow(IntPtr window);
  [DllImport("user32.dll")]
  public static extern bool IsWindowVisible(IntPtr window);
  [DllImport("user32.dll")]
  public static extern IntPtr GetAncestor(IntPtr window, uint flags);
  [DllImport("user32.dll")]
  public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
  [DllImport("user32.dll")]
  public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr window);
  [DllImport("user32.dll")]
  public static extern bool BringWindowToTop(IntPtr window);
  [DllImport("user32.dll")]
  public static extern bool ShowWindow(IntPtr window, int command);

  public static INPUT Key(ushort virtualKey, bool keyUp) {
    INPUT input = new INPUT();
    input.type = 1; // INPUT_KEYBOARD
    input.data.ki.wVk = virtualKey;
    input.data.ki.wScan = (ushort)MapVirtualKey(virtualKey, 0);
    input.data.ki.dwFlags = keyUp ? 2u : 0u; // KEYEVENTF_KEYUP
    return input;
  }
  public static uint Send(INPUT[] inputs) {
    return SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(INPUT)));
  }
}
'@

$keyCode = if ($Direction -eq "Previous") { 0x7C } else { 0x7D }
$keyName = if ($Direction -eq "Previous") { "F13" } else { "F14" }
$handleValue = [Convert]::ToInt64($WindowHandle.Substring(2), 16)
$targetWindow = [IntPtr]::new($handleValue)
$target = $null
$injectionAttempted = $false
$failure = $null
$evidence = [ordered]@{
  status = "starting"
  processId = $BrowserProcessId
  windowHandle = $WindowHandle
  direction = $Direction
  key = $keyName
  repeatCount = $RepeatCount
  sentInputCount = 0
  expectedInputCount = 0
  cleanupInputCount = 0
  foregroundVerified = $false
  executable = $null
  processStartedAt = $null
  sessionId = $null
  error = $null
}

function Assert-TargetWindow {
  $target.Refresh()
  if ($target.HasExited -or ![FloorpWorkspaceInputNative]::IsWindow($targetWindow)) {
    throw "The captured browser process or window has exited."
  }
  $windowProcessId = [uint32]0
  [void][FloorpWorkspaceInputNative]::GetWindowThreadProcessId($targetWindow, [ref]$windowProcessId)
  if ($windowProcessId -ne $BrowserProcessId -or
      [FloorpWorkspaceInputNative]::GetAncestor($targetWindow, 2) -ne $targetWindow -or
      ![FloorpWorkspaceInputNative]::IsWindowVisible($targetWindow)) {
    throw "The native handle is not a visible top-level window of the captured browser."
  }
}

function Send-CheckedInput([FloorpWorkspaceInputNative+INPUT[]]$Inputs) {
  $sent = [FloorpWorkspaceInputNative]::Send($Inputs)
  $evidence.sentInputCount += $sent
  if ($sent -ne $Inputs.Count) {
    throw "SendInput inserted $sent of $($Inputs.Count) events (Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))."
  }
}

try {
  $expectedExecutable = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..\_dist\bin\floorp\floorp.exe")
  )
  $target = Get-Process -Id $BrowserProcessId -ErrorAction Stop
  # Retain the process handle so later checks refer to this process identity.
  $capturedProcessHandle = $target.Handle
  if ($capturedProcessHandle -eq [IntPtr]::Zero) { throw "Could not capture the browser process handle." }
  $evidence.executable = $target.MainModule.FileName
  $evidence.processStartedAt = $target.StartTime.ToUniversalTime().ToString("o", [Globalization.CultureInfo]::InvariantCulture)
  $evidence.sessionId = $target.SessionId
  $caller = Get-Process -Id $PID
  try {
    if (![string]::Equals($evidence.executable, $expectedExecutable, [StringComparison]::OrdinalIgnoreCase) -or
        $target.SessionId -ne $caller.SessionId) {
      throw "The target is not this repository's browser in the current Windows session."
    }
  } finally {
    $caller.Dispose()
  }
  Assert-TargetWindow

  # Do not alter an existing physical or synthetic modifier/key press.
  foreach ($virtualKey in @(0x10, 0x11, 0x12, 0x5B, 0x5C, 0x7C, 0x7D)) {
    if (([int][FloorpWorkspaceInputNative]::GetAsyncKeyState($virtualKey) -band 0x8000) -ne 0) {
      throw "A modifier or test function key is already held on the desktop."
    }
  }
  for ($attempt = 0; $attempt -lt 10; $attempt++) {
    Assert-TargetWindow
    if ([FloorpWorkspaceInputNative]::GetForegroundWindow() -eq $targetWindow) { break }
    [void][FloorpWorkspaceInputNative]::ShowWindow($targetWindow, 9) # SW_RESTORE
    [void][FloorpWorkspaceInputNative]::BringWindowToTop($targetWindow)
    [void][FloorpWorkspaceInputNative]::SetForegroundWindow($targetWindow)
    Start-Sleep -Milliseconds 100
  }
  Assert-TargetWindow
  if ([FloorpWorkspaceInputNative]::GetForegroundWindow() -ne $targetWindow) {
    throw "Windows did not grant foreground input to the verified browser window."
  }
  $evidence.foregroundVerified = $true

  $inputs = [Collections.Generic.List[FloorpWorkspaceInputNative+INPUT]]::new()
  $inputs.Add([FloorpWorkspaceInputNative]::Key(0xA2, $false)) # ControlLeft
  $inputs.Add([FloorpWorkspaceInputNative]::Key(0xA0, $false)) # ShiftLeft
  $inputs.Add([FloorpWorkspaceInputNative]::Key($keyCode, $false))
  $releases = @(
    [FloorpWorkspaceInputNative]::Key($keyCode, $true)
    [FloorpWorkspaceInputNative]::Key(0xA0, $true)
    [FloorpWorkspaceInputNative]::Key(0xA2, $true)
  )
  $evidence.expectedInputCount = $RepeatCount + 5
  $injectionAttempted = $true
  if ($RepeatCount -eq 1) {
    # A single press includes its releases in one OS input batch.
    $inputs.AddRange([FloorpWorkspaceInputNative+INPUT[]]$releases)
    Send-CheckedInput $inputs.ToArray()
  } else {
    Send-CheckedInput $inputs.ToArray()
    for ($index = 1; $index -lt $RepeatCount; $index++) {
      # Let Gecko process each held keydown instead of coalescing a burst.
      Start-Sleep -Milliseconds 100
      Assert-TargetWindow
      if ([FloorpWorkspaceInputNative]::GetForegroundWindow() -ne $targetWindow) {
        throw "The browser lost foreground input during the held key sequence."
      }
      Send-CheckedInput @([FloorpWorkspaceInputNative]::Key($keyCode, $false))
    }
    Send-CheckedInput $releases
  }
  Assert-TargetWindow
  if ([FloorpWorkspaceInputNative]::GetForegroundWindow() -ne $targetWindow) {
    throw "The browser lost foreground input during the key batch."
  }
  $evidence.status = "sent"
} catch {
  $failure = $_.Exception.Message
  $evidence.status = "failed"
  $evidence.error = $failure
} finally {
  if ($injectionAttempted) {
    # Also release our keys after a partial SendInput failure.
    $releases = @(
      [FloorpWorkspaceInputNative]::Key($keyCode, $true)
      [FloorpWorkspaceInputNative]::Key(0xA0, $true)
      [FloorpWorkspaceInputNative]::Key(0xA2, $true)
    )
    $evidence.cleanupInputCount = [FloorpWorkspaceInputNative]::Send($releases)
    if ($evidence.cleanupInputCount -ne $releases.Count) {
      $failure = "SendInput could not release all injected keys."
      $evidence.status = "failed"
      $evidence.error = $failure
    }
  }
  if ($null -ne $target) { $target.Dispose() }
}

[PSCustomObject]$evidence | ConvertTo-Json -Compress
if ($null -ne $failure) {
  [Console]::Error.WriteLine($failure)
  exit 1
}
