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
