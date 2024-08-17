#!/bin/bash

# Downloads and installs the latest long-term-support (LTS) .NET Runtime
# under Wine in the same WINEPREFIX that Guilty Gear Xrd runs in.

EXE_NAME=not_yet_known
DOWNLOAD_URL='http://aka.ms/dotnet/6.0/dotnet-sdk-win-PLATFORM.exe'  # substitue x64 or x86 for PLATFORM
WINE_LAUNCHER_NAME=launch_GGXrdReversalTool_linux.sh
failed=false

print_failed_to_request() {
  echo Failed to request .NET installer from "$DOWNLOAD_URL"
}

if [ ! -f "$WINE_LAUNCHER_NAME" ]; then
  echo "$WINE_LAUNCHER_NAME" script not found. Please make sure it is present in this directory and try again.
fi

GUILTYGEAR_WINEPREFIX=$(./"$WINE_LAUNCHER_NAME" "unused" "--only-print-wineprefix")
if [ $? != 0 ]; then
  echo "Failed to obtain GUILTYGEAR_WINEPREFIX using the $WINE_LAUNCHER_NAME script"
  exit 1
fi

PLATFORM="unknown"
if [ -d "$GUILTYGEAR_WINEPREFIX/drive_c/windows/syswow64" ]; then
  PLATFORM="x64"
else
  PLATFORM="x86"
fi

DOWNLOAD_URL=$(echo "$DOWNLOAD_URL" | sed -r "s/PLATFORM/$PLATFORM/")

# Grab the name and the download link of the latest .NET 6.0 version
# Note that it may change over time to an even newer version,
# and I don't know how to check if it's already installed, so
# one day it may download a new version and install it, even
# if you already have it installed.
# So you should only run this script manually if you really
# need to install .NET.

# Only ask for headers, "-D -" prints response headers into stdout,
# -o /dev/null redirects the file download itself into nothingness.
# -sS makes it silent but print errors on error.

# tr -d '\r' deletes "caret return" character from the output.
# This is needed because in the HTTP protocol specification, \r\n
# is defined as the separator for HTTP header 'name: value' pairs.
HTTP_HEADERS=$(curl -sS -D - $DOWNLOAD_URL -o /dev/null | tr -d '\r')
if [ $? != 0 ]; then
  echo "$HTTP_HEADERS"  # print the errors
  print_failed_to_request
  exit 1
fi

echo "$HTTP_HEADERS" | grep -P 'HTTP/\d+(\.\d+)* 301(\s+.*)?' > /dev/null || failed=true
if [ $failed == true ]; then
  echo Error: The "$DOWNLOAD_URL" site responded not with a redirect.
  print_failed_to_request
  exit 1
fi

# Grab the Location header, usually present in a redirection response
LOCATION="$(echo "$HTTP_HEADERS" | grep -P "^Location: " | sed -r 's/^Location: //')"

# Remove everything up to and including the last /
EXE_NAME="$(echo "$LOCATION" | sed -r 's/.*\///')"

if [ ! -n "$EXE_NAME" ]; then
  print_failed_to_request
  exit 1
fi

if [ ! -f "$EXE_NAME" ]; then
 echo "$EXE_NAME not found. Downloading from $LOCATION"
 curl "$LOCATION" -o "$EXE_NAME"
else
 echo "$EXE_NAME" already exists. Will try to launch it
fi
if [ -f "$WINE_LAUNCHER_NAME" ]; then
  ./"$WINE_LAUNCHER_NAME" "$EXE_NAME"
else
  echo "$WINE_LAUNCHER_NAME" script not found. Please make sure it is present in this directory and try again.
fi
