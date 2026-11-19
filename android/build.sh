#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${ANDROID_PLATFORM:?Set ANDROID_PLATFORM to the SDK platform directory containing android.jar}"
: "${ANDROID_BUILD_TOOLS:?Set ANDROID_BUILD_TOOLS to the Android SDK build-tools directory}"
mkdir -p build/classes build/dex
java com.sun.tools.javac.Main -source 8 -target 8 -Xlint:-options -encoding UTF-8   -bootclasspath "$ANDROID_BUILD_TOOLS/core-lambda-stubs.jar:$ANDROID_PLATFORM/android.jar"   -d build/classes src/pl/psplink/*.java
"$ANDROID_BUILD_TOOLS/aapt" package -f -M AndroidManifest.xml   -I "$ANDROID_PLATFORM/android.jar" -F build/base.apk
java -cp "$ANDROID_BUILD_TOOLS/lib/d8.jar" com.android.tools.r8.D8   --lib "$ANDROID_PLATFORM/android.jar" --min-api 23   --output build/dex build/classes/pl/psplink/*.class
(cd build/dex && zip -q ../base.apk classes.dex)
"$ANDROID_BUILD_TOOLS/zipalign" -f 4 build/base.apk build/aligned.apk
java -jar "$ANDROID_BUILD_TOOLS/lib/apksigner.jar" sign   --ks test-key.jks --ks-key-alias test --ks-pass pass:android --key-pass pass:android   --out PSP-Link-Test.apk build/aligned.apk
java -jar "$ANDROID_BUILD_TOOLS/lib/apksigner.jar" verify --verbose PSP-Link-Test.apk
