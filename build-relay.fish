#!/usr/bin/fish
# Does not install packages or modify the running service.
set root (path dirname (status filename))
pkg-config --exists gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gio-unix-2.0
or begin
    echo "Missing development headers: obtain approval before installing libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev." >&2
    exit 1
end
mkdir -p $root/build
or exit 1
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -fanalyzer \
    '-DV4L2_RELAYD_VERSION="0.2.0-imx471-idle2"' \
    $root/relay/v4l2-relayd.c -o $root/build/imx471-v4l2-relayd \
    (pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gio-unix-2.0 | string split -n ' ')
