# SPDX-License-Identifier: MPL-2.0
# End-user setup on a disposable CI test machine only. VB-CABLE is never part
# of a Floorp package or installation flow. Origin: https://vb-audio.com/Cable/
# Donationware/license: https://vb-audio.com/Services/licensing.htm
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_OS -ne 'Windows' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
  throw 'Audio driver setup is restricted to disposable GitHub-hosted Windows CI runners.'
}
$output = Join-Path $env:GITHUB_WORKSPACE '_dist/chrome-runtime-tests/windows-audio-backend.json'
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
$evidence = [ordered]@{ source = 'existing-native-endpoint'; endpoints = @(); error = $null }
function Get-RenderEndpoints {
  return @(Get-PnpDevice -Class AudioEndpoint -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.Status -eq 'OK' -and $_.InstanceId -match '\{0\.0\.0\.' } |
    Select-Object FriendlyName, InstanceId, Status)
}
try {
  Set-Service AudioEndpointBuilder -StartupType Automatic
  Set-Service Audiosrv -StartupType Automatic
  Start-Service AudioEndpointBuilder
  Start-Service Audiosrv
  $endpoints = Get-RenderEndpoints
  if ($endpoints.Count -eq 0) {
    $url = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
    $sha256 = 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb'
    $directory = Join-Path $env:RUNNER_TEMP 'floorp-audio-end-user'
    New-Item -ItemType Directory -Force $directory | Out-Null
    $archive = Join-Path $directory 'VBCABLE_Driver_Pack45.zip'
    Invoke-WebRequest -Uri $url -OutFile $archive -TimeoutSec 60
    if ((Get-Item $archive).Length -ne 1318877 -or (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
      throw 'The official audio driver archive differs from the authenticated test dependency.'
    }
    Expand-Archive -Path $archive -DestinationPath (Join-Path $directory 'driver')
    $installer = Join-Path $directory 'driver/VBCABLE_Setup_x64.exe'
    $catalog = Join-Path $directory 'driver/vbaudio_cable64_win10.cat'
    $signature = Get-AuthenticodeSignature $installer
    $catalogSignature = Get-AuthenticodeSignature $catalog
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=BUREL VINCENT Entrepreneur individuel(,|$)') {
      throw 'The audio installer must have a valid expected VB-Audio publisher signature.'
    }
    if ($catalogSignature.Status -ne 'Valid' -or $catalogSignature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Microsoft Windows Hardware Compatibility Publisher(,|$)') {
      throw 'The driver must retain its valid Microsoft hardware compatibility catalog signature.'
    }
    $evidence.source = $url
    $evidence.archiveSha256 = $sha256
    $evidence.publisher = $signature.SignerCertificate.Subject
    $evidence.publisherThumbprint = $signature.SignerCertificate.Thumbprint
    $evidence.catalogPublisher = $catalogSignature.SignerCertificate.Subject
    $process = Start-Process -FilePath $installer -ArgumentList '-i', '-h' -PassThru
    if (-not $process.WaitForExit(120000)) {
      Stop-Process -Id $process.Id -Force
      throw 'Vendor audio installation exceeded its two-minute bound; no reboot or signature-policy bypass was attempted.'
    }
    $evidence.installerExitCode = $process.ExitCode
    if ($process.ExitCode -notin @(0, 3010)) { throw "Audio installer failed: $($process.ExitCode)" }
    Restart-Service Audiosrv -Force
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
      $endpoints = Get-RenderEndpoints
      if ($endpoints.Count -gt 0) { break }
      Start-Sleep -Seconds 1
    }
  }
  $evidence.endpoints = $endpoints
  if ($endpoints.Count -eq 0) {
    throw 'No actual render endpoint appeared. This image may require a reboot; qualification requires a genuine endpoint and cannot use a signature bypass or synthetic capture result.'
  }
} catch {
  $evidence.error = $_.Exception.Message
  throw
} finally {
  $evidence | ConvertTo-Json -Depth 5 | Set-Content $output
}
