# ThinkPad built-in IMX471 webcam repair

Verified on Lenovo 21V8S0A500, Ubuntu 26.04.1, kernel 7.0.0-38-generic.
This is an explicit, machine-specific repair, not a universal webcam installer.
It combines an IMX471 sensor backport, a bounded libcamera-to-V4L2 bridge for
Edge/Teams, and a workaround for GNOME Camera's PipeWire frame-rate negotiation.
The installer deliberately refuses other machine/sensor identities.

## Source

Sensor-only backport from [ocewers/x1c14-camera-imx471](https://github.com/ocewers/x1c14-camera-imx471),
commit 485baf27151d81ffcccd4012abbd551bbee18b6a. Source retains its GPL-2.0
SPDX identifier. No vendor installer was executed. Existing IPU/graphics modules
were not replaced. The pinned upstream source already includes the `avdd`
supply name; the bundled driver is byte-for-byte identical to that source.

## Why a bridge on Ubuntu?

This is not a USB/UVC-style camera. The IMX471 sensor sits behind Intel IPU7
and needs the existing IPU drivers plus libcamera's processing pipeline to
produce usable images. The raw IPU7 V4L2 nodes are not standalone webcams.
On the tested Edge installation, a conventional V4L2 capture interface was
needed, so the relay exposes processed libcamera frames through v4l2loopback:

`IMX471 / IPU7 → libcamera → v4l2-relayd → /dev/video50 → Edge or PipeWire`

This is a compatibility workaround for this hardware/software combination,
not a requirement for every Ubuntu webcam. The raw-node hiding rule avoids
selecting unusable or competing sources, while leaving USB webcams available.

## Configuration

- DKMS imx471-backport/2026.10.08, AUTOINSTALL=yes, preserves source under
  /usr/src and rebuilds using matching target-kernel headers.
- DKMS skips the backport when a target kernel supplies in-tree/built-in IMX471.
- Existing Ubuntu kernel/header DKMS hooks and linux-generic-hwe-26.04 track
  updates. Current-kernel autoinstall verified; a future update/reboot is untested.
- v4l2loopback is also DKMS-managed. Built-in output is /dev/video50,
  YUYV 1280x720@30, labelled Built-in Sony IMX471.
- On-demand v4l2-relayd selects the simple libcamera pipeline and permits
  char-dma_heap in the systemd device sandbox for the software ISP.
- v4l2sink sync=false is essential: the default sink timing caused prolonged
  blackouts with libcamera timestamps.
- The 2026-10-13 hardening disables videorate frame duplication and initial
  padding (`drop-only=true skip-to-first=true max-rate=30`). The relay keeps
  its original base time across on-demand camera opens; do not fill these
  timestamp gaps with synthetic frames.
- Output appsrc uses a three-frame downstream-leaky queue, with live/time
  semantics. `max-bytes` alone does not enforce a bound when the producer
  ignores enough-data; stale frames must be dropped explicitly.
- The service has MemoryHigh=256M, MemoryMax=512M, MemorySwapMax=0 and a
  three-second restart delay. This is a containment backstop, not a substitute
  for pipeline bounds. Restart=always is inherited from the packaged unit.
- `normalize-framerate.c` is built as
  /usr/local/libexec/imx471-normalize-framerate. It reduces the loopback's
  frame-interval fraction without changing the rate or reading video.
  ExecStartPost normalizes at relay startup; a separately sandboxed,
  16-MiB-limited `imx471-framerate-normalizer.service` checks once per second
  because later Chromium/Edge camera opens can restore an unreduced fraction.
  It is pulled in by the relay and stops with it. This is a temporary workaround
  for PipeWire's installed V4L2 frame-rate comparison bug, not image processing.
- The included `wireplumber/60-built-in-camera-bridge.conf`, installed into
  `~/.config/wireplumber/wireplumber.conf.d/`, hides raw IPU7 nodes and the competing native
  libcamera source; the USB camera remains available.
- Secure Boot was already disabled and was not changed. DKMS signs with the
  existing local MOK key; Secure Boot key enrollment is not handled here.

## Reapply

Close camera applications and review all files first. Matching kernel headers
must be installed. The installer checks machine and sensor identity. It does
not reboot or restart audio services. If another v4l2loopback configuration
already occupies /dev/video50, resolve the conflict before running it.

Use GUI askpass, never a password in chat or shell arguments:

```fish
# Install fish and matching kernel headers first if needed. Run from this clone.
command -v /usr/sbin/ksshaskpass
# Stop if the helper is missing; see the zenity alternative below.
env SUDO_ASKPASS=/usr/sbin/ksshaskpass sudo -A fish ./install.fish
sudo -k
# Run these as your normal desktop user, NOT root:
mkdir -p ~/.config/wireplumber/wireplumber.conf.d
cp wireplumber/60-built-in-camera-bridge.conf ~/.config/wireplumber/wireplumber.conf.d/
systemctl --user restart wireplumber
gsettings set org.gnome.Snapshot last-camera-id 'Built-in Sony IMX471 (V4L2)'
```

Check that the helper exists first; use a zenity askpass helper if unavailable.
For example, create an executable helper containing `#!/bin/sh` followed by
`exec zenity --password --title="Webcam repair: administrator authentication"`
and use its absolute path as `SUDO_ASKPASS`. Never save the returned password.
Restarting WirePlumber can briefly interrupt desktop audio; do it only during
initial setup, after closing camera clients. The Snapshot setting is optional
and requires GNOME Camera to be installed. The installer requires the existing
Ubuntu IPU7/libcamera stack and does not install or replace that stack.
The installer is syntax checked but has not been rerun as a complete installer;
its component operations were applied and verified individually on this host.

## Verification and limits

Both cameras selected successfully in Teams Settings > Devices in actual Edge.
30-second local WebRTC recordings on the Teams origin, without joining a
meeting or sending video, confirmed stable delivery after fixing sink timing.
The on-demand built-in source has a cold-start delay (about 4-8 seconds observed)
before its first usable image. This is distinct from recurrent blackouts. Sensor tuning
is uncalibrated and has a slight green/dim cast.

Videos, images, browser identifiers, and raw diagnostic logs remain private
and are not included in this repository.

Final simultaneous recordings after first usable image: built-in 908 decoded
frames in about 30.4 seconds; external 903 in about 30.2 seconds. Every decoded
frame was unique; neither had blackouts or track mute/disconnect events.
Visual review confirmed modest live pose/hand changes in both recordings.

### 2026-10-13 regression and hardening

The earlier 30-second validation did not establish long-lived/reopen/suspend
stability. The relay subsequently produced black frames and repeated
`gst_mini_object_unlock` assertions, held 5.3 GiB at the host-wide OOM that
killed Edge, and grew to 9.3 GiB during reproduction. It had not restarted
since October 8. This failure occurred upstream of Edge at /dev/video50.

The bounded configuration above was installed into /etc and the relay was
restarted without restarting the user's Edge. An 18-second cold-start capture
delivered 534 frames at about 30 fps with non-black luma after initial startup;
three six-second reopens delivered 176, 176 and 174 frames without a service
restart. Service cgroup memory stayed around 50-65 MiB during these tests.
An isolated headless Microsoft Edge 154 test using the real camera delivered
893 frames in 30.04 seconds (29.73 fps), with 59 distinct sampled frame hashes,
zero black samples and no mute/unmute/ended track events. All test tracks were
stopped afterward. Cgroup memory was about 72 MiB after browser verification;
this short test does not establish indefinite leak-free operation.
Old installed configuration is backed up under
/var/backups/imx471-relay-20261013/; pre-edit user configuration and private
diagnostic logs are under
~/.local/share/hardware-repairs/webcam-20261013-debug/.

Reboot and suspend/resume remain separately unverified. Uncalibrated IMX471
sensor-property/crop warnings remain; do not mistake those existing warnings
for the resolved multi-gigabyte queue failure. An actual Teams screen-sharing
call still needs user validation; do not claim that a local capture test proves
meeting stability or eliminates memory pressure from other applications.

### GNOME Camera / Snapshot fixed-rate negotiation

Snapshot 50.0 failed with `no more input formats` / `not-negotiated` even
though Edge could capture the bridge. Its saved camera selection was corrected
from `ipu7 (V4L2)` to `Built-in Sony IMX471 (V4L2)`, but a second bug remained:
the loopback advertised 300000/10000 fps while Snapshot requested 30/1 fps.
PipeWire 1.6.2 `spa/plugins/v4l2/v4l2-utils.c:compare_fraction()` returns 1
instead of 0 for equal cross-products unless the written fractions match.

A minimal PipeWire/GStreamer capture succeeded with no fixed frame-rate
constraint, failed when `framerate=30/1` was added, and succeeded with that
same constraint after normalizing the kernel's timeperframe to 1/30. Snapshot
then negotiated a running 1280x720 YUY2 30/1 stream; the user confirmed a
working preview. No PipeWire/audio-service or Edge restart was required.

This exact upstream defect is already reported and closed:
[PipeWire issue #5491](https://gitlab.freedesktop.org/pipewire/pipewire/-/issues/5491).
No duplicate issue was filed. The installed distro package remains affected;
remove the normalizer only after verifying a corrected package with the
unreduced-fraction reproduction. A separate concurrent Edge test while
Snapshot owned the camera returned NotReadableError, so this work does not
claim simultaneous camera sharing between those applications.

## Rollback

Close clients, stop v4l2-relayd@builtin.service, and remove only the added
system configuration files corresponding to this directory. Remove the user
WirePlumber camera rule, reload systemd, and restart user WirePlumber.
Use dkms remove imx471-backport/2026.10.08 --all to unregister the backport.
DKMS may restore the archived original manual module at
/lib/modules/<kernel>/updates/webcam-repair/imx471.ko; remove that too if fully
rolling back and run depmod. Never remove stock kernel modules.
The relay stop also stops imx471-framerate-normalizer.service; remove its
/etc/systemd/system unit and /usr/local/libexec/imx471-normalize-framerate
when rolling back the normalization workaround.

## Licensing and provenance

The sensor driver retains its upstream GPL-2.0 SPDX identifier and copyright
notices; its license text is in `LICENSES/GPL-2.0.txt`. The standalone
`normalize-framerate.c` helper is MIT-licensed (`LICENSES/MIT.txt`). The
installer, configuration, and documentation are also MIT-licensed. See
`PROVENANCE.md` for the source pin, local changes, and validation scope.
