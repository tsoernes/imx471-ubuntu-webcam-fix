#!/usr/bin/fish
# Explicit opt-in; no package installation, DKMS changes or audio restarts.
if test (id -u) -ne 0
    echo "Run build-relay.fish as your normal user, then sudo -A fish ./install-idle-fix.fish" >&2
    exit 1
end
set root (path dirname (status filename))
test -x /usr/bin/gst-launch-1.0
and gst-inspect-1.0 fdsrc >/dev/null
and gst-inspect-1.0 fdsink >/dev/null
and gst-inspect-1.0 rawvideoparse >/dev/null
and gst-inspect-1.0 fdsrc | string match -qr '^  is-live '
or begin
    echo "Requires GStreamer >=1.26 with tools/base plugins; obtain approval before installing them." >&2
    exit 1
end
if not test (cat /sys/class/dmi/id/product_name) = 21V8S0A500
    echo "Refusing an unverified machine." >&2
    exit 1
end
test -x $root/build/imx471-v4l2-relayd
and $root/build/imx471-v4l2-relayd --version | string match -q '*0.2.0-imx471-idle2*'
or begin
    echo "Build the reviewed relay first with fish ./build-relay.fish" >&2
    exit 1
end
set dropins /etc/systemd/system/v4l2-relayd@builtin.service.d
set backup /var/backups/imx471-idle-(date +%Y%m%d-%H%M%S)
mkdir -p $backup $dropins
and cp -a $dropins $backup/dropins
or exit 1
if test -f /usr/local/libexec/imx471-v4l2-relayd
    cp -a /usr/local/libexec/imx471-v4l2-relayd $backup/
    or exit 1
end
install -Dm755 $root/build/imx471-v4l2-relayd /usr/local/libexec/imx471-v4l2-relayd
and install -m644 $root/built-in-relay-service.conf $dropins/50-camera-pipeline.conf
and install -m644 $root/idle-memory-service.conf $dropins/60-idle-memory.conf
and systemctl daemon-reload
and systemctl restart v4l2-relayd@builtin.service
and systemctl is-active --quiet v4l2-relayd@builtin.service
or begin
    echo "Installation failed. Restore service drop-ins from $backup/dropins before retrying." >&2
    exit 1
end
echo "Installed instance-specific idle-memory fix. Backup: $backup"
systemctl show v4l2-relayd@builtin.service -p MainPID -p ActiveState -p MemoryCurrent -p NRestarts
