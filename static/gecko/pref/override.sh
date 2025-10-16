#!/bin/bash

# Firefox preferences override script
# Receives firefox.js path as command line argument and overrides settings with override.ini

set -euo pipefail

# ANSI color codes
COLOR_RESET='\033[0m'
COLOR_BLUE='\033[34m'
COLOR_GREEN='\033[32m'
COLOR_YELLOW='\033[33m'
COLOR_RED='\033[31m'

# Check command line arguments
if [[ $# -ne 1 ]]; then
    echo -e "${COLOR_RED}Error: Usage: $0 <path-to-firefox.js>${COLOR_RESET}" >&2
    exit 1
fi

FIREFOX_JS_PATH="$1"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OVERRIDE_INI="${SCRIPT_DIR}/override.ini"

# Check if override.ini exists
if [[ ! -f "$OVERRIDE_INI" ]]; then
    echo -e "${COLOR_RED}Error: override.ini not found at $OVERRIDE_INI${COLOR_RESET}" >&2
    exit 1
fi

# Check if firefox.js file exists
if [[ ! -f "$FIREFOX_JS_PATH" ]]; then
    echo -e "${COLOR_RED}Error: firefox.js not found at $FIREFOX_JS_PATH${COLOR_RESET}" >&2
    exit 1
fi

# Create temporary file
TEMP_FILE=$(mktemp)
trap 'rm -f "$TEMP_FILE"' EXIT

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

        # Remove leading/trailing spaces
        pref_value=$(echo "$pref_value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

        # Strip surrounding quotes (one or more) to normalize
        stripped_value=$(printf '%s' "$pref_value" | sed -E 's/^"+//;s/"+$//')

        # Determine final formatted value
        if [[ "$stripped_value" =~ ^(true|false)$ ]]; then
            # Boolean value
            formatted_value="$stripped_value"
        elif [[ "$stripped_value" =~ ^[0-9]+$ ]]; then
            # Numeric value
            formatted_value="$stripped_value"
        elif [[ -z "$stripped_value" ]]; then
            # Empty string -> produce ""
            formatted_value='""'
        else
            # String value -> wrap in single pair of double quotes
            # Escape any existing double quotes inside the string
            safe_value=$(printf '%s' "$stripped_value" | sed 's/"/\\"/g')
            formatted_value="\"$safe_value\""
        fi

        # Search for existing preference
        escaped_pref_name=$(printf '%s\n' "$pref_name" | sed 's/[[\.*^$()+?{|]/\\&/g')

        if grep -q "pref(\"$escaped_pref_name\"" "$TEMP_FILE"; then
            # Replace existing preference (using | as delimiter to avoid conflicts with URLs)
            sed -i.bak "s|pref(\"$escaped_pref_name\"[^)]*);|pref(\"$pref_name\", $formatted_value);|" "$TEMP_FILE"
        else
            # Add as new preference
            NEW_PREFS+=("pref(\"$pref_name\", $formatted_value);")
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
if ! mv "$TEMP_FILE" "$FIREFOX_JS_PATH" 2>/dev/null; then
    echo -e "${COLOR_RED}✗ Error: Failed to apply changes to firefox.js${COLOR_RESET}" >&2
    echo -e "${COLOR_RED}→ Check file permissions for: $FIREFOX_JS_PATH${COLOR_RESET}" >&2
    exit 1
fi
