/* ============================================================
   app.js — fetch ke backend, tampilkan JSON di UI.
   Memakai relative URL agar kompatibel dengan Codespaces port-forwarding.
   ============================================================ */

const API = (location.protocol === 'http:' || location.protocol === 'https:')
  ? ''                              // same-origin (server menyajikan frontend + API)
  : 'http://localhost:8080';        // fallback kalau dibuka via file://

const $ = (id) => document.getElementById(id);

const outEl    = $('output');
const statusEl = $('status');
const inputKey = $('input-key');

/* -------- Render helpers -------- */
function show(obj) {
  outEl.textContent = (typeof obj === 'string')
    ? obj
    : JSON.stringify(obj, null, 2);
  outEl.scrollTop = 0;
}

function setStatus(msg, kind = '') {
  statusEl.textContent = msg;
  statusEl.className = 'status ' + kind;
}

function shake(el) {
  el.classList.remove('shake');
  void el.offsetWidth;        // force reflow → restart animation
  el.classList.add('shake');
}

/* -------- Fetch wrapper -------- */
async function request(path, opts = {}) {
  const res = await fetch(API + path, opts);
  const text = await res.text();
  let body;
  try { body = JSON.parse(text); } catch { body = { raw: text }; }
  return { status: res.status, body, headers: res.headers };
}

/* -------- Handlers -------- */
async function generateKey() {
  const btn = $('btn-gen');
  const sp  = $('spinner-gen');
  sp.classList.remove('hidden');
  setStatus('generating key…');
  try {
    const { status, body } = await request('/auth/register', { method: 'POST' });
    show({ http_status: status, ...body });
    if (status === 200 && body.api_key) {
      inputKey.value = body.api_key;
      setStatus('✅ api key generated — auto-filled below', 'ok');
    } else {
      setStatus('❌ unexpected status ' + status, 'err');
      shake(btn);
    }
  } catch (err) {
    show({ error: String(err) });
    setStatus('❌ network error (server off?)', 'err');
    shake(btn);
  } finally {
    sp.classList.add('hidden');
  }
}

async function verifyKey() {
  const key = inputKey.value.trim();
  if (!key) { setStatus('⚠ paste a key first', 'err'); shake(inputKey); return; }

  const sp = $('spinner-test');
  sp.classList.remove('hidden');
  setStatus('verifying…');
  try {
    const { status, body } = await request('/auth/verify', {
      method: 'POST',
      headers: { 'X-API-Key': key }
    });
    show({ http_status: status, ...body });
    if (body.valid) {
      setStatus('✅ key valid', 'ok');
    } else {
      setStatus('❌ key invalid (401)', 'err');
      shake(inputKey);
    }
  } catch (err) {
    show({ error: String(err) });
    setStatus('❌ network error', 'err');
    shake(inputKey);
  } finally {
    sp.classList.add('hidden');
  }
}

async function fetchData() {
  const key = inputKey.value.trim();
  if (!key) { setStatus('⚠ paste a key first', 'err'); shake(inputKey); return; }

  const sp = $('spinner-test');
  sp.classList.remove('hidden');
  setStatus('fetching /data…');
  try {
    const { status, body } = await request('/data', {
      headers: { 'X-API-Key': key }
    });
    show({ http_status: status, ...body });

    if      (status === 200) setStatus('✅ 200 OK', 'ok');
    else if (status === 401) { setStatus('❌ 401 Unauthorized', 'err'); shake(inputKey); }
    else if (status === 429) { setStatus('⏱ 429 Rate limit (100/min)', 'err'); shake(inputKey); }
    else if (status === 500) setStatus('💥 500 Server error', 'err');
    else                     setStatus('⚠ status ' + status, 'err');
  } catch (err) {
    show({ error: String(err) });
    setStatus('❌ network error', 'err');
  } finally {
    sp.classList.add('hidden');
  }
}

async function copyKey() {
  const key = inputKey.value.trim();
  if (!key) { setStatus('⚠ nothing to copy', 'err'); return; }
  try {
    await navigator.clipboard.writeText(key);
    setStatus('📋 copied to clipboard', 'ok');
  } catch {
    // Fallback untuk browser tanpa clipboard API
    const ta = document.createElement('textarea');
    ta.value = key;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    try { document.execCommand('copy'); setStatus('📋 copied (fallback)', 'ok'); }
    catch { setStatus('❌ copy failed', 'err'); }
    document.body.removeChild(ta);
  }
}

/* -------- Boot -------- */
document.addEventListener('DOMContentLoaded', () => {
  $('btn-gen').addEventListener('click', generateKey);
  $('btn-verify').addEventListener('click', verifyKey);
  $('btn-data').addEventListener('click', fetchData);
  $('btn-copy').addEventListener('click', copyKey);

  // Enter di input = verify
  inputKey.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') verifyKey();
  });

  // Cek status server
  fetch(API + '/health')
    .then(r => r.json())
    .then(j => { if (j.status === 'ok') setStatus('🟢 server online', 'ok'); })
    .catch(() => setStatus('🔴 server offline', 'err'));

  /* -------- Custom cursor dengan lerp -------- */
  const cursor = $('cursor');
  let mx = window.innerWidth  / 2;
  let my = window.innerHeight / 2;
  let cx = mx, cy = my;

  window.addEventListener('mousemove', (e) => {
    mx = e.clientX; my = e.clientY;
  });
  window.addEventListener('touchmove', (e) => {
    if (e.touches[0]) { mx = e.touches[0].clientX; my = e.touches[0].clientY; }
  }, { passive: true });

  function tick() {
    cx += (mx - cx) * 0.22;
    cy += (my - cy) * 0.22;
    cursor.style.transform = `translate(${cx}px, ${cy}px) translate(-50%,-50%)`;
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
});
