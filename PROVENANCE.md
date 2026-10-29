# Source and validation provenance

Packaged on 2026-10-13 from the working, reviewed machine-specific repair.
No camera images, recordings, browser identifiers, passwords, private logs,
or unrelated dotfiles are included.

## Sensor source

- Upstream: [ocewers/x1c14-camera-imx471](https://github.com/ocewers/x1c14-camera-imx471)
- Commit: `485baf27151d81ffcccd4012abbd551bbee18b6a`
- File: [`src/imx471.c`](https://github.com/ocewers/x1c14-camera-imx471/blob/485baf27151d81ffcccd4012abbd551bbee18b6a/src/imx471.c)
- Bundled at: `sensor-module/imx471.c`
- SHA-256: `e0d26fae361bd06363a92f0f972398d925816e4d4790df58dbccccc092c0972a`
- Fetched and compared byte-for-byte during packaging: no local source diff.
  Earlier repair notes called `avdd` a local adaptation; direct comparison
  establishes that the pinned upstream source already includes it.
- Upstream GPL-2.0 header and Intel/Kate Hsuan copyright notices preserved.

## Repair-owned components

- Fish installer and sensor-only DKMS metadata (2026-10-08).
- Loopback, bounded relay pipeline, and systemd limits (hardened 2026-10-13).
- WirePlumber source-selection rules; no USB webcam disabling.
- MIT-licensed C frame-interval normalizer and sandboxed watcher.
- The export adds `build-essential` and `v4l-utils` to installer dependencies,
  bundles the previously separate WirePlumber rule, and adds source-check CI.
  The installed running pipeline and helper source are unchanged.

## Verified environment and results

ThinkPad X1 Carbon Gen 14, Lenovo product `21V8S0A500`, Sony IMX471 ACPI
`TBE20A0`, Ubuntu 26.04.1, kernel `7.0.0-38-generic`, PipeWire
`1.6.2-1ubuntu1.2`, GNOME Snapshot 50.0, v4l2loopback 0.15.3.

- Sensor DKMS build and IPU7 binding verified on the stated kernel.
- Bounded relay: 534 frames over an 18-second capture; 176/176/174 frames
  over three six-second reopens.
- Real Microsoft Edge 154 isolated capture: 893 frames over 30.04 seconds,
  no black samples or track events, 59 unique sampled frame hashes.
- GNOME Camera preview visually confirmed working by the user after
  frame-interval normalization.
- Watcher installation verified active. An unreduced interval introduced
  with `v4l2-ctl --set-parm=30` was automatically reduced to `1/30` within
  two seconds; the actual 30 fps rate was unchanged.
- Bridge cgroup memory about 88 MiB at the final check, zero automatic
  restarts; watcher about 1.4 MiB.
- Packaging checks: C compilation with `-Wall -Wextra -Werror -fanalyzer`,
  Fish syntax, helper usage exit status, and systemd unit verification.

The full installer has not been rerun end-to-end: its component operations
were applied individually. Reboot, suspend/resume, long-term leak freedom,
and a real Teams screen-sharing meeting remain unverified. A concurrent
Edge capture while Snapshot owned the camera returned `NotReadableError`;
simultaneous sharing is not claimed.

## Upstream PipeWire defect

[PipeWire issue #5491](https://gitlab.freedesktop.org/pipewire/pipewire/-/issues/5491)
already describes equal-but-unreduced frame intervals being rejected by the
V4L2 comparator. It is closed upstream, but the installed distro package was
reproduced as affected. No duplicate issue was filed; no corrected release
version is claimed here. Remove the workaround only after retesting the
fixed-rate negotiation with an unreduced interval on the installed package.

## 2026-10-29 idle-memory lifecycle patch

### Relay source pin

- Ubuntu archive source, matching installed package `0.2.0-0ubuntu1`:
  <https://archive.ubuntu.com/ubuntu/pool/universe/v/v4l2-relayd/v4l2-relayd_0.2.0.orig.tar.gz>
- Archive SHA-256:
  `3f25808c4b9672893b83a8e1db2a7c2c9ac87226fa4ac006c1bad8870cd11294`
  (also matches Ubuntu's `.dsc` metadata).
- Unmodified `src/v4l2-relayd.c` SHA-256:
  `247e0f68d361c5d59410acf74b09fe88ccbbfe7c8ae1797776a8e5e29c588159`.
- Patched source: `relay/v4l2-relayd.c`. GPL version 2; Canonical's header
  is preserved. `relay/idle-release.patch` records the exact source changes.
- The older Launchpad `ubuntu/devel` Git branch was inspected but **not used**:
  it does not contain 0.2.0's updated v4l2loopback client-usage event constant.

### Diagnosis and implementation

The old idle relay had 258 MiB RSS, approximately 262 MiB cgroup memory, 93
threads, and millions of `memory.high` reclaim events. PipeWire's source was
suspended. Source inspection established that `input_pipeline_disable()` stopped
the input graph but retained its object and bus-watch reference. Exact ownership
of all retained pages was not measured, so allocator retention versus additional
upstream leaks is not fully attributed.

The first candidate destroyed that graph and its watch after a two-second idle
grace period and trimmed glibc pages. It passed capture tests but still retained
memory across repeated real-camera opens, so it was superseded before claiming
completion. The final candidate isolates libcamera/Mesa in a short-lived source
worker, transports raw frames through a bounded pipe, and immediately stops and
reaps the worker on final close. Its parent output graph stays discoverable;
its input graph is freed on close and rebuilt on demand. This gives camera
library allocations a process-lifetime boundary. Both processes share the
service cgroup and its limits; no active-capture service restart is scheduled.
The patch also balances the temporary appsrc reference and trims free parent
allocator pages. The instance-only service override uses a separately installed
binary and two malloc arenas. The existing
256 MiB/512 MiB soft/hard limits and frame-rate normalizer are unchanged.

`SPECIFICATION.md` records user requirements and approval for development
packages and the reversible instance-specific installation. Runtime logs and
camera luma statistics remain private outside the repository. No image bytes
or browser identifiers are published.

### Final isolated-mode validation

On 2026-10-29, installed `0.2.0-imx471-idle2` and verified installed binary
and both service overrides match the reviewed build/configuration. Original
configuration backups remain outside Git. Only the webcam relay was restarted;
the user's browser and desktop audio were not restarted by the repair commands.

- Five synthetic in-process cycles and five isolated-source cycles verified
  delivery, quick reopen, input-object finalization and worker reaping. Separate
  ASan/UBSan lifecycle runs passed (`detect_leaks=0`, so LeakSanitizer coverage
  is **not** claimed). Strict compiler warnings and GCC analyzer passed.
- Isolated real Microsoft Edge 155 captures delivered **240, 239 and 236**
  frames over **8.073, 8.068 and 8.107 seconds**, respectively, at 1280x720.
  All sampled frames were nonblack; no mute/unmute/ended track events occurred.
  Idle cgroup memory after these three captures was 26,722,304 / 26,685,440 /
  26,984,448 bytes, not the per-open growth of the rejected first candidate.
- Twelve additional native open/close tests each delivered 45 frames and exited
  successfully. Idle RSS stayed **24,676–24,728 KiB**; idle cgroup memory stayed
  **27,017,216–27,103,232 bytes**. The parent PID stayed unchanged, with zero
  automatic service restarts and zero `memory.high`/OOM events during testing.
- After deliberately restoring an unreduced loopback interval, the existing
  normalizer restored 30/1. A fixed-rate PipeWire capture of 60 frames at
  1280x720 YUY2 30/1 then exited successfully. The normalizer stayed active.
- At final idle the relay had five threads, no worker child, ~24 MiB RSS and
  ~26 MiB cgroup memory. The shared service cgroup peak was ~50 MiB in these
  tests; existing soft/hard limits remained 256/512 MiB.
- Compared with the original idle **264,104 KiB RSS** (~258 MiB), the tested
  parent idle RSS is about **24 MiB**, a roughly **91% reduction**.

These are repeated-open and short-run tests, not proof of indefinite leak-free
operation. Reboot, suspend/resume and real Teams meeting/screen-sharing behavior
remain separate verification tasks. No simultaneous Edge/Snapshot claim is made.
