# User-specified requirements

## Current scope

- 2026-10-29, user asked why `v4l2-relayd` used approximately 0.8% RAM with
  no active camera usage. After the proposal to release/recycle the camera
  pipeline when idle instead of simply reducing the memory cap, user said
  **“do it”**: implement that idle-memory improvement.
- 2026-10-29, user explicitly approved installing `libgstreamer1.0-dev` and
  `libgstreamer-plugins-base1.0-dev`, followed by a tested, reversible patched
  relay for this webcam service only (approval-question reply).
- 2026-10-13, user requested a GitHub repository containing the webcam fix
  and clarified **“public”**. Existing public repository:
  <https://github.com/tsoernes/imx471-ubuntu-webcam-fix>.
- 2026-10-29 global instructions: ask for explicit approval before installing
  software/tools; keep user requirements separate from agent implementation
  decisions and maintain this durable specification.

## Agent implementation goals (not additional user requirements)

- Preserve camera discovery and on-demand reopening in Edge and GNOME Camera.
- Keep the frame-rate workaround and bounded frame queues.
- Avoid restarting Edge or desktop audio.
- Verify idle memory and repeated open/close behavior before claiming success.
- Prefer a narrowly scoped, reversible change over a new resident daemon.

## Open questions / limits

- No numeric idle-RAM acceptance target was specified by the user.
- Real Teams calls, suspend/resume and long-term stability require separate
  validation; do not infer them from local capture tests.

## Implementation status (agent record, not new requirements)

2026-10-29: the instance-specific isolated-input relay was installed and tested.
The original in-process release-only candidate was rejected after reopen stress
showed continued retention. The revised worker lifetime boundary held idle RSS
near 24 MiB across repeated native/Edge opens, without automatic service
restarts. See `PROVENANCE.md` for exact measurements and remaining limits.
