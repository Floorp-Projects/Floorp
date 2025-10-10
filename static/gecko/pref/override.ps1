param(
    [Parameter(Mandatory=$true)]
    [string]$FirefoxJsPath
)

# Firefox preferences override script
# Receives firefox.js path as command line argument and overrides settings with override.ini

# ANSI color codes
$ColorReset = "`e[0m"
$ColorBlue = "`e[34m"
$ColorGreen = "`e[32m"
$ColorYellow = "`e[33m"
$ColorRed = "`e[31m"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$OverrideIni = Join-Path $ScriptDir "override.ini"

# Check if override.ini exists
if (-not (Test-Path $OverrideIni)) {
    Write-Host "${ColorRed}Error: override.ini not found at $OverrideIni${ColorReset}" -ForegroundColor Red
    exit 1
}

# Check if firefox.js file exists
if (-not (Test-Path $FirefoxJsPath)) {
    Write-Host "${ColorRed}Error: firefox.js not found at $FirefoxJsPath${ColorReset}" -ForegroundColor Red
    exit 1
}

# Create temporary file
$TempFile = [System.IO.Path]::GetTempFileName()

try {
    # Copy existing firefox.js
    Copy-Item $FirefoxJsPath $TempFile

    # Array to store new preferences to add
    $NewPrefs = @()

    # Read firefox.js content
    $FirefoxContent = Get-Content $TempFile -Raw

    # Parse override.ini
    $OverrideContent = Get-Content $OverrideIni
    foreach ($line in $OverrideContent) {
        # Skip empty lines and comment lines
        if ([string]::IsNullOrWhiteSpace($line) -or $line.Trim().StartsWith('#')) {
            continue
        }

        # Parse preference line (pref_name = value)
        if ($line -match '^\s*([^=]+)\s*=\s*(.+)$') {
            $prefName = $matches[1].Trim()
            $prefValue = $matches[2].Trim()

            # Handle values that are not quoted
            if ($prefValue -match '^".*"$') {
                # Already quoted
                $formattedValue = $prefValue
            } elseif ($prefValue -match '^(true|false)$') {
                # Boolean value
                $formattedValue = $prefValue
            } elseif ($prefValue -match '^[0-9]+$') {
                # Numeric value
                $formattedValue = $prefValue
            } else {
                # Treat as string
                $formattedValue = "`"$prefValue`""
            }

            # Escape special regex characters for the preference name
            $escapedPrefName = [regex]::Escape($prefName)

            # Search for existing preference
            $existingPrefPattern = "pref\(`"$escapedPrefName`"[^)]*\);"
            
            if ($FirefoxContent -match $existingPrefPattern) {
                # Replace existing preference
                $replacement = "pref(`"$prefName`", $formattedValue);"
                $FirefoxContent = $FirefoxContent -replace $existingPrefPattern, $replacement
            } else {
                # Add as new preference
                $newPref = "pref(`"$prefName`", $formattedValue);"
                $NewPrefs += $newPref
            }
        }
    }

    # Add new preferences to the end of the file
    if ($NewPrefs.Count -gt 0) {
        $FirefoxContent += "`n"
        $FirefoxContent += "// Floorp custom preferences (added by override.ps1)`n"
        foreach ($pref in $NewPrefs) {
            $FirefoxContent += "$pref`n"
        }
    }

    # Write the updated content back to the temporary file
    Set-Content -Path $TempFile -Value $FirefoxContent -NoNewline

    # Apply changes to firefox.js
    Move-Item $TempFile $FirefoxJsPath -Force

} catch {
    Write-Host "${ColorRed}✗ An error occurred: $($_.Exception.Message)${ColorReset}" -ForegroundColor Red
    Write-Host "${ColorRed}Stack trace:${ColorReset}" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
} finally {
    # Clean up temporary file if it still exists
    if (Test-Path $TempFile) {
        Remove-Item $TempFile -Force
    }
} 