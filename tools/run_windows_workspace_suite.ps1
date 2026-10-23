# SPDX-License-Identifier: MPL-2.0

param(
  [Parameter(Mandatory = $true)]
  [ValidateSet(
    "workspaces",
    "workspace-gestures",
    "workspace-session-indicator",
    "workspace-bookmarks",
    "command-palette",
    "workspace-overrides",
    "pages-modal-child",
    "modules"
  )]
  [string]$Suite,
  [string]$TestFile = ""
)

$ErrorActionPreference = "Stop"
# Inspect the runner's exit code after preserving its diagnostics.
$PSNativeCommandUseErrorActionPreference = $false

function Copy-OptionalEvidence([string]$Source, [string]$Destination) {
  try {
    if (Test-Path $Source) { Copy-Item $Source $Destination }
  } catch {
    Write-Warning "Could not copy evidence ${Source}: $($_.Exception.Message)"
  }
}

$suites = @{
  "workspaces" = @{ near = "browser-features/chrome/common/workspaces"; files = 24 }
  "workspace-gestures" = @{ near = "browser-features/chrome/common/workspace-gestures"; files = 3 }
  "workspace-session-indicator" = @{ near = "browser-features/chrome/common/workspace-session-indicator"; files = 2 }
  "workspace-bookmarks" = @{ near = "browser-features/chrome/common/workspace-bookmarks"; files = 2 }
  "command-palette" = @{ near = "browser-features/chrome/common/command-palette"; files = 9 }
  "workspace-overrides" = @{ near = "browser-features/chrome/static/overrides/modules/workspaces"; files = 2 }
  "pages-modal-child" = @{ near = "browser-features/pages-modal-child"; files = 2 }
  "modules" = @{ near = "browser-features/modules/modules/test"; files = 5 }
}
$target = $suites[$Suite]
$selectedTestFile = $null
if ($TestFile -ne "") {
  if ($Suite -ne "workspaces") {
    throw "-TestFile is supported only for the workspaces suite"
  }
  $testRoot = "browser-features/chrome/common/workspaces/test"
  $normalized = $TestFile.Replace('\', '/')
  $prefix = "$testRoot/"
  $filename = if ($normalized.StartsWith($prefix, [StringComparison]::Ordinal)) {
    $normalized.Substring($prefix.Length)
  } else {
    $normalized
  }
  if ($filename -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]*\.test\.ts$') {
    throw "-TestFile must name a .test.ts file directly under $testRoot"
  }
  $selectedTestFile = "$testRoot/$filename"
  if (!(Test-Path -LiteralPath $selectedTestFile -PathType Leaf)) {
    throw "Workspace test file does not exist: $selectedTestFile"
  }
  $entry = Get-Item -LiteralPath $selectedTestFile -Force
  if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw "Workspace test file must not be a symbolic link: $selectedTestFile"
  }
  $target = @{ near = $selectedTestFile; files = 1 }
}
$resultsRoot = "_dist/windows-workspaces"
$suiteDir = Join-Path $resultsRoot $Suite
New-Item -ItemType Directory -Force -Path $suiteDir | Out-Null
$revision = git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw "Could not record the source revision" }
$revision | Set-Content "$resultsRoot/source-commit.txt"
[PSCustomObject]@{
  os = [Environment]::OSVersion.VersionString
  userInteractive = [Environment]::UserInteractive
  sessionId = (Get-Process -Id $PID).SessionId
  headlessEnvironmentPresent = Test-Path Env:MOZ_HEADLESS
} | ConvertTo-Json | Set-Content "$resultsRoot/environment.json"
Remove-Item logs/test/latest.log -ErrorAction SilentlyContinue

$startedAt = [DateTime]::UtcNow.ToString("o")
$exitCode = 1
$coverageVerified = $false
$runnerError = $null
try {
  Write-Output "Running $Suite ($($target.files) expected browser files): $($target.near)"
  deno task test --near $target.near --timeout-ms 300000 --startup-timeout-ms 180000 2>&1 |
    Tee-Object -FilePath "$suiteDir/runner.log"
  $exitCode = $LASTEXITCODE
} catch {
  $runnerError = $_.Exception.Message
  Write-Output "Runner execution error: $runnerError"
} finally {
  if (Test-Path logs/test/latest.log) {
    try {
      $log = Get-Content logs/test/latest.log -Raw
      $coverageVerified = $log -match "(?m)^\[[^\r\n]+\] \[INFO\] Browser test result: $($target.files) passed, 0 failed\r?$" -and
        $log -match "(?m)^\[META\] exitCode=0\r?$"
    } catch {
      $runnerError = "Could not verify the collected log: $($_.Exception.Message)"
    }
  }
  $result = [PSCustomObject]@{
    suite = $Suite
    near = $target.near
    selectedTestFile = $selectedTestFile
    expectedFiles = $target.files
    exitCode = $exitCode
    coverageVerified = $coverageVerified
    runnerError = $runnerError
    startedAt = $startedAt
    finishedAt = [DateTime]::UtcNow.ToString("o")
  }
  ConvertTo-Json -InputObject $result -Depth 3 | Set-Content "$suiteDir/result.json"
  $results = @(
    Get-ChildItem $resultsRoot -Directory | Sort-Object Name | ForEach-Object {
      $resultPath = Join-Path $_.FullName "result.json"
      if (Test-Path $resultPath) { Get-Content $resultPath -Raw | ConvertFrom-Json }
    }
  )
  ConvertTo-Json -InputObject $results -Depth 3 | Set-Content "$resultsRoot/results.json"

  # Save the result before copying optional files that can be locked on Windows.
  Copy-OptionalEvidence logs/test/latest.log "$suiteDir/collected.log"
  foreach ($evidence in @("prefs.js", "workspace-bookmarks-benchmark.json", "workspace-windows-input-evidence.json")) {
    Copy-OptionalEvidence "_dist/profile/test/$evidence" "$suiteDir/$evidence"
  }
  $viteLogs = @(Get-ChildItem logs/vite-*.log -ErrorAction SilentlyContinue)
  if ($viteLogs.Count -gt 0) {
    $viteDir = Join-Path $suiteDir "vite"
    New-Item -ItemType Directory -Force -Path $viteDir | Out-Null
    foreach ($viteLog in $viteLogs) {
      Copy-OptionalEvidence $viteLog.FullName (Join-Path $viteDir $viteLog.Name)
    }
  }
  $result | Format-List | Out-String | Write-Output
  if ($env:GITHUB_STEP_SUMMARY) {
    $status = if ($exitCode -eq 0 -and $coverageVerified) { "Passed" } else { "Failed" }
    "${Suite}: $status; expected $($target.files) browser files; runner exit $exitCode; coverage verified: $coverageVerified" |
      Add-Content $env:GITHUB_STEP_SUMMARY
  }
}

if ($exitCode -ne 0 -or !$coverageVerified -or $runnerError) {
  throw "Windows suite $Suite failed or had incomplete coverage. See $suiteDir."
}
