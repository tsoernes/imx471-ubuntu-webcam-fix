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

## Optional idle-memory fix (2026-10-29)

The distro relay stops capture by setting its input graph to `NULL`, but keeps
the graph and bus-watch reference alive. Long-lived library/allocator state can
therefore remain resident even when PipeWire reports the camera as suspended.
The local process reached about 258 MiB RSS after repeated use; its cgroup was
above the 256 MiB soft limit and repeatedly stalled on reclaim.

`relay/v4l2-relayd.c` vendors Ubuntu's 0.2.0 source with a small lifecycle patch:

- In the installed isolated-input mode, run libcamera and Mesa in an on-demand
  worker process. Stop/reap it and free the parent's input graph on final close.
  The output pipeline stays alive so the virtual camera remains visible.
- Transfer fixed-size raw frames over a bounded kernel pipe; there is no image
  file, socket daemon, or unbounded queue. Rebase frames onto the persistent
  output clock so reopening does not restart output timestamps.
- Keeping all input processing in-process was tested first: freeing the graph
  alone still left per-open library retention. Process isolation ensures those
  camera-library allocations disappear with the worker, not merely its graph.
- A non-isolated fallback retains a two-second idle grace period and cancels
  destruction on quick reopen. The installed service uses immediate isolated
  cleanup, not that fallback.
- Return free allocator pages with glibc `malloc_trim(0)`; the instance-specific
  override limits malloc arenas to two and enables normal allocator trimming.
- Balance the temporary output-appsrc reference when constructing each input
  graph. Handle a missing appsink sample during shutdown and nullable cleanup.

The package executable `/usr/bin/v4l2-relayd` is **not replaced**. Only this
camera instance opts into `/usr/local/libexec/imx471-v4l2-relayd` through an
additional service override. Existing queue bounds, `sync=false`, frame-rate
normalization and memory limits remain in place. No periodic service restarts
or additional resident watchdog are introduced. A source worker exists only
while a client has the camera open and shares the same service memory limits.

Verified on the stated host: three real Edge 155 captures at about 30 fps,
followed by twelve native reopens, with no automatic relay restarts. Idle RSS
stayed about **24 MiB** (formerly ~258 MiB); cgroup memory stayed about
**26 MiB** across the native tests, with no soft-limit reclaim/OOM events.
This establishes short-run reopen behavior, not indefinite leak freedom or
Teams/suspend/resume stability. Detailed measurements are in `PROVENANCE.md`.

### Build, test and explicitly install

Obtain approval before installing any software on a managed workstation.
Required development packages: `libgstreamer1.0-dev` and
`libgstreamer-plugins-base1.0-dev`, plus the compiler/pkg-config/Fish toolchain.
Synthetic tests need the normal GStreamer base plugins; no camera is accessed.
The installed mode also needs `/usr/bin/gst-launch-1.0` and the base plugins
`fdsrc`, `fdsink` and `rawvideoparse` (already present on the tested host).
GStreamer **1.26 or newer** is required for `fdsrc:is-live`; the tested Ubuntu
26.04 installation uses 1.28.2, and CI uses an Ubuntu 26.04 container.

```fish
fish ./build-relay.fish
fish tests/run-relay-tests.fish
env RELAY_TEST_ISOLATE=1 fish tests/run-relay-tests.fish
# Close camera clients first. Use a validated GUI askpass helper as above.
env SUDO_ASKPASS=/usr/sbin/ksshaskpass sudo -A fish ./install-idle-fix.fish
sudo -k
```

This separate installer does not install packages or touch the sensor driver.
It backs up the existing service drop-ins under
`/var/backups/imx471-idle-<timestamp>/`, installs the reviewed binary and
overrides, reloads systemd, and restarts **only** the built-in camera relay.
The original main installer still defaults to the packaged relay; reapply this
optional override after rebuilding the custom binary as needed.

`tests/edge-capture.cjs` is an optional real-camera regression test using an
already installed Playwright and real Microsoft Edge in an isolated profile.
It does not attach to the user's browser, install anything, save images, or
join Teams. It opens the named built-in camera three times, measures frames
and luma, closes tracks, waits for idle cleanup, and reports cgroup RAM.

```fish
# Omit PLAYWRIGHT_MODULE if require('playwright') already resolves normally.
env PLAYWRIGHT_MODULE=/absolute/path/to/existing/playwright node tests/edge-capture.cjs
```

For rollback, close clients and remove only
`/etc/systemd/system/v4l2-relayd@builtin.service.d/60-idle-memory.conf`, then
reload systemd and restart `v4l2-relayd@builtin.service`. The updated 50-series
override defaults back to the untouched packaged relay. To restore the exact
pre-install configuration, restore its backed-up 50-series file as well.
Remove `/usr/local/libexec/imx471-v4l2-relayd` only after the service no longer
uses it. Existing frame-rate normalization and bounded buffering are independent
of this optional change and should be retained.

## Licensing and provenance

The sensor driver and vendored relay retain their upstream GPL-2.0 license and
copyright notices; the lifecycle test is GPL-2.0-only. Their license text is in
`LICENSES/GPL-2.0.txt`. The standalone
`normalize-framerate.c` helper is MIT-licensed (`LICENSES/MIT.txt`). The
installer, configuration, and documentation are also MIT-licensed. See
`PROVENANCE.md` for the source pin, local changes, and validation scope.
