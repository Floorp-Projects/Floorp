#!/bin/bash

# Firefox preferences override script
# Receives firefox.js path as command line argument and overrides settings with override.ini

set -euo pipefail

# Check command line arguments
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <path-to-firefox.js>" >&2
    exit 1
fi

FIREFOX_JS_PATH="$1"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OVERRIDE_INI="${SCRIPT_DIR}/override.ini"

# Check if override.ini exists
if [[ ! -f "$OVERRIDE_INI" ]]; then
    echo "Error: override.ini not found at $OVERRIDE_INI" >&2
    exit 1
fi

# Check if firefox.js file exists
if [[ ! -f "$FIREFOX_JS_PATH" ]]; then
    echo "Error: firefox.js not found at $FIREFOX_JS_PATH" >&2
    exit 1
fi

# Create temporary file
TEMP_FILE=$(mktemp)
trap 'rm -f "$TEMP_FILE"' EXIT

# Read override.ini and apply settings
echo "Applying override.ini settings..."

# Copy existing firefox.js
cp "$FIREFOX_JS_PATH" "$TEMP_FILE"

# Array to store new preferences to add
declare -a NEW_PREFS=()

# Parse override.ini
while IFS= read -r line; do
    # Skip empty lines and comment lines
    if [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]]; then
        continue
    fi

    # Parse preference line (pref_name = value)
    if [[ "$line" =~ ^[[:space:]]*([^=]+)[[:space:]]*=[[:space:]]*(.+)$ ]]; then
        pref_name="${BASH_REMATCH[1]// /}"  # Remove leading/trailing spaces
        pref_value="${BASH_REMATCH[2]}"

        # Remove leading/trailing spaces and handle quotes properly
        pref_value=$(echo "$pref_value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

        # Handle values that are not quoted
        if [[ "$pref_value" =~ ^\".*\"$ ]]; then
            # Already quoted
            formatted_value="$pref_value"
        elif [[ "$pref_value" =~ ^(true|false)$ ]]; then
            # Boolean value
            formatted_value="$pref_value"
        elif [[ "$pref_value" =~ ^[0-9]+$ ]]; then
            # Numeric value
            formatted_value="$pref_value"
        else
            # Treat as string
            formatted_value="\"$pref_value\""
        fi

        # Search for existing preference
        escaped_pref_name=$(printf '%s\n' "$pref_name" | sed 's/[[\.*^$()+?{|]/\\&/g')

        if grep -q "pref(\"$escaped_pref_name\"" "$TEMP_FILE"; then
            # Replace existing preference (using | as delimiter to avoid conflicts with URLs)
            sed -i.bak "s|pref(\"$escaped_pref_name\"[^)]*);|pref(\"$pref_name\", $formatted_value);|" "$TEMP_FILE"
            echo "Updated: $pref_name = $formatted_value"
        else
            # Add as new preference
            NEW_PREFS+=("pref(\"$pref_name\", $formatted_value);")
            echo "Added: $pref_name = $formatted_value"
        fi
    fi
done < "$OVERRIDE_INI"

# Add new preferences to the end of the file
if [[ ${#NEW_PREFS[@]} -gt 0 ]]; then
    echo "" >> "$TEMP_FILE"
    echo "// Floorp custom preferences (added by override.sh)" >> "$TEMP_FILE"
    for pref in "${NEW_PREFS[@]}"; do
        echo "$pref" >> "$TEMP_FILE"
    done
fi

# Apply changes to firefox.js
mv "$TEMP_FILE" "$FIREFOX_JS_PATH"

echo "Firefox preferences update completed."
echo "Modified file: $FIREFOX_JS_PATH"
