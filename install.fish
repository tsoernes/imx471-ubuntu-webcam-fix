#!/usr/bin/fish
# Run explicitly as root after reviewing README.md; never an automatic bootstrap.
if test (id -u) -ne 0
    echo "Run with GUI askpass: sudo -A fish "(status filename) >&2
    exit 1
end
set root (path dirname (status filename))
if not test (cat /sys/class/dmi/id/product_name) = 21V8S0A500
    echo "This repair is verified only on Lenovo 21V8S0A500; refusing another machine." >&2
    exit 1
end
if not test -e /sys/bus/acpi/devices/TBE20A0:00
    echo "Expected IMX471 ACPI device missing; refusing installation." >&2
    exit 1
end
if not test -e /lib/modules/(uname -r)/build/Makefile
    echo "Install matching kernel headers first." >&2
    exit 1
end
apt-get install -y build-essential dkms v4l-utils v4l2loopback-dkms v4l2-relayd libcamera-tools gstreamer1.0-libcamera
or exit 1
set helper_build (mktemp -d)
or exit 1
cc -std=gnu11 -O2 -Wall -Wextra -Werror $root/normalize-framerate.c -o $helper_build/imx471-normalize-framerate
or begin
    rmdir $helper_build
    exit 1
end
install -Dm755 $helper_build/imx471-normalize-framerate /usr/local/libexec/imx471-normalize-framerate
set helper_result $status
rm $helper_build/imx471-normalize-framerate
rmdir $helper_build
if test $helper_result -ne 0
    exit $helper_result
end
mkdir -p /usr/src/imx471-backport-2026.10.08
cp $root/sensor-module/imx471.c $root/sensor-module/Makefile $root/sensor-module/dkms.conf /usr/src/imx471-backport-2026.10.08/
or exit 1
if not test -d /var/lib/dkms/imx471-backport/2026.10.08
    dkms add -m imx471-backport -v 2026.10.08
    or exit 1
end
dkms autoinstall -k (uname -r)
or exit 1
install -Dm644 $root/built-in-relay.conf /etc/v4l2-relayd.d/builtin.conf
and install -Dm644 $root/built-in-relay-service.conf /etc/systemd/system/v4l2-relayd@builtin.service.d/50-camera-pipeline.conf
and install -Dm644 $root/imx471-framerate-normalizer.service /etc/systemd/system/imx471-framerate-normalizer.service
and install -Dm644 $root/built-in-loopback.conf /etc/modprobe.d/zz-built-in-webcam.conf
and install -Dm644 $root/built-in-modules.conf /etc/modules-load.d/built-in-webcam.conf
or exit 1
modprobe imx471
and modprobe v4l2loopback
and systemctl daemon-reload
and systemctl enable --now v4l2-relayd.service v4l2-relayd@builtin.service
and systemctl restart v4l2-relayd@builtin.service
or exit 1
echo "Restart user WirePlumber after closing camera clients; see README."
