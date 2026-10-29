/* SPDX-License-Identifier: MIT
 * Optional real-hardware test. Uses an already installed Playwright module and
 * isolated real Edge, never the user's browser profile. Stores statistics only.
 * PLAYWRIGHT_MODULE may name the absolute path of an existing installation.
 */
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const http = require('node:http');
const fs = require('node:fs');
const assert = require('node:assert/strict');

const memoryPath = '/sys/fs/cgroup/system.slice/system-v4l2\\x2drelayd.slice/v4l2-relayd@builtin.service/memory.current';
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

(async () => {
  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('<!doctype html><title>Isolated camera regression test</title>');
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  let browser;
  try {
    browser = await chromium.launch({
      channel: 'msedge', headless: true,
      args: ['--use-fake-ui-for-media-stream', '--no-first-run'],
      // Only bypass the permission UI. NO fake camera or fake video device.
    });
    const context = await browser.newContext();
    await context.grantPermissions(['camera']);
    const page = await context.newPage();
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    const results = { browser: browser.version(), cycles: [] };
    for (let cycle = 1; cycle <= 3; cycle++) {
      const capture = await page.evaluate(async () => {
        const devices = await navigator.mediaDevices.enumerateDevices();
        const camera = devices.find(d => d.kind === 'videoinput' && d.label.includes('Built-in Sony IMX471'));
        if (!camera) throw new Error('Built-in Sony IMX471 was not discoverable');
        let stream, callback;
        const video = document.createElement('video');
        video.muted = true;
        document.body.append(video);
        const events = [];
        try {
          stream = await navigator.mediaDevices.getUserMedia({ audio: false, video: {
            deviceId: { exact: camera.deviceId }, width: { ideal: 1280 },
            height: { ideal: 720 }, frameRate: { ideal: 30 },
          }});
          const track = stream.getVideoTracks()[0];
          for (const event of ['mute', 'unmute', 'ended'])
            track.addEventListener(event, () => events.push(event));
          video.srcObject = stream;
          await video.play();
          let frames = 0;
          function frame() { frames++; callback = video.requestVideoFrameCallback(frame); }
          callback = video.requestVideoFrameCallback(frame);
          const canvas = document.createElement('canvas');
          canvas.width = 64; canvas.height = 36;
          const ctx = canvas.getContext('2d', { willReadFrequently: true });
          const lumas = [];
          const started = performance.now();
          for (let i = 0; i < 16; i++) {
            await new Promise(resolve => setTimeout(resolve, 500));
            ctx.drawImage(video, 0, 0, 64, 36);
            const rgba = ctx.getImageData(0, 0, 64, 36).data;
            let sum = 0;
            for (let p = 0; p < rgba.length; p += 4)
              sum += (rgba[p] + rgba[p + 1] + rgba[p + 2]) / 3;
            lumas.push(sum / (64 * 36));
          }
          return { frames, seconds: (performance.now() - started) / 1000,
            lumas, events, settings: { width: track.getSettings().width,
              height: track.getSettings().height, frameRate: track.getSettings().frameRate } };
        } finally {
          if (callback) video.cancelVideoFrameCallback(callback);
          if (stream) stream.getTracks().forEach(track => track.stop());
          video.srcObject = null;
          video.remove();
        }
      });
      await sleep(4500);
      const memoryAfterIdleBytes = Number(fs.readFileSync(memoryPath, 'utf8'));
      results.cycles.push({ cycle, ...capture, memoryAfterIdleBytes });
      assert(capture.frames >= 150, `cycle ${cycle}: insufficient live frames`);
      assert(capture.lumas.slice(4).every(luma => luma > 5), `cycle ${cycle}: black frame after warmup`);
      assert.equal(capture.events.length, 0, `cycle ${cycle}: unexpected track events`);
      assert(memoryAfterIdleBytes < 100 * 1024 * 1024, `cycle ${cycle}: idle RAM exceeded test threshold`);
    }
    console.log(JSON.stringify(results, null, 2));
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
