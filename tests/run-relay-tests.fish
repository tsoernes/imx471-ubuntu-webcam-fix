#!/usr/bin/fish
set root (path dirname (path dirname (status filename)))
mkdir -p $root/build
or exit 1
set checks -fanalyzer
set binary $root/build/relay-lifecycle-test
if test "$RELAY_TEST_SANITIZE" = 1
    set checks -fsanitize=address,undefined -fno-omit-frame-pointer
    set binary $root/build/relay-lifecycle-sanitized
end
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter $checks \
    '-DV4L2_RELAYD_VERSION="test"' \
    $root/tests/relay-lifecycle.c -o $binary \
    (pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gio-unix-2.0 | string split -n ' ')
or exit 1
env ASAN_OPTIONS=detect_leaks=0 G_DEBUG=fatal-warnings MALLOC_ARENA_MAX=2 $binary
