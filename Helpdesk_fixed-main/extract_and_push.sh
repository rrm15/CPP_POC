#!/bin/bash
# Extract the helpdesk source code from the zip file

set -e

echo "Extracting Helpdesk_Control_fixed.zip..."
unzip -o Helpdesk_Control_fixed.zip

echo "Removing zip file..."
rm Helpdesk_Control_fixed.zip

echo "Staging all extracted files..."
git add -A

echo "Committing extracted source..."
git commit -m "Extract helpdesk source code - initial commit for concurrent write safeguards and gcov coverage implementation"

echo "Pushing to main branch..."
git push origin main

echo "Done! Source code is now available in the repository."
