#!/bin/bash
find . -type f \( -name "*.nut" -o -name "*.mdl" \) | while read -r file; do
    if file "$file" | grep -q "gzip compressed"; then
        echo "Extracting -> $file"
        gunzip -c "$file" > "$file.tmp" && mv "$file.tmp" "$file"
    fi
done
