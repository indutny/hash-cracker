'use strict';

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot

const SEED = 1;
const REPEAT = 20;
const EXTRACT_COUNT = 48;
const KEY_COUNT = 17;

// 2093 hits the limit of transitions
const PROBES_COUNT = 2093;

function test(pre, list) {
  const o = {};
  o[pre] = null;
  for (let i = 0; i < list.length; i++)
    o[list[i]] = null;
  return o;
}

function hash(str) {
  let hash = SEED;

  for (let i = 0; i < str.length; i++) {
    hash = (hash + str.charCodeAt(i)) | 0;
    hash = (hash + (hash << 10)) | 0;
    hash ^= hash >>> 6;
  }

  hash = (hash + (hash << 3)) | 0;
  hash ^= (hash >>> 11);
  hash = (hash + (hash << 15)) | 0;

  return (hash >>> 0) & 0x3fffffff;
}

const ALPHABET_FIRST =
    'abcdefghijklmnopqrstuvwxyz'.split('');
const ALPHABET =
    '0123456789abcdefghijklmnopqrstuvwxyz'.split('');

function key(i) {
  let r = '';
  let t = i;
  r += ALPHABET_FIRST[t % ALPHABET_FIRST.length];
  t = (t / ALPHABET_FIRST.length) | 0;
  while (r.length < 4) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}

const keys = [];
// 18 keys trigger hashmap
for (let i = 0; i < KEY_COUNT; i++) {
  keys.push(key(i));
}

keys.sort((a, b) => hash(a) - hash(b));

function measure(list) {
  let avg = new Array(list.length);
  let stddev = new Array(list.length);

  avg.fill(0);
  stddev.fill(0);

  for (let i = 0; i < REPEAT * list.length; i++) {
    const idx = i % list.length;
    const pre = list[idx];

    const start = process.hrtime();
    test(pre, keys);
    const end = process.hrtime(start);

    let delta = end[0] * 1e9 + end[1];

    avg[idx] += delta;
    stddev[idx] += Math.pow(delta, 2);
  }

  for (let i = 0; i < list.length; i++) {
    avg[i] /= REPEAT;
    stddev[i] /= REPEAT;
    stddev[i] -= Math.pow(avg[i], 2);
    stddev[i] = Math.sqrt(stddev[i]);
  }

  return { avg: avg, stddev: stddev };
}

console.log('number of tests %d', REPEAT * PROBES_COUNT);
console.log('[' + keys.map(k => hash(k).toString(16)).join(',') + ']');

function findPos(k) {
  const h = hash(k);

  let last = 0;
  let minPos = null;
  let maxPos = null;
  for (let i = 0; i < keys.length; i++) {
    let next = hash(keys[i]);
    if (last <= h && h < next)
      return i;
    last = next;
  }
  return keys.length;
}

function toCArray(arr) {
  return '{' + arr.map(e => JSON.stringify(e)).join(',') + '}';
}

let offset = 0;
let results = [];
let hits = 0;
for (let i = 0; i < EXTRACT_COUNT; i++) {
  const probes = [];
  for (let j = KEY_COUNT; j < KEY_COUNT + PROBES_COUNT; j++)
    probes.push(key(j + offset));
  offset += probes.length;

  const r = measure(probes);

  let min = Infinity;
  let minI = 0;
  let max = 0;
  let maxI = 0;
  for (let j = 0; j < r.avg.length; j++) {
    const avg = r.avg[j];
    const wstd = Math.exp(-r.stddev[j] / avg);

    // Totally adhoc way to choose min/max with lowest stddev
    let hscore = avg * wstd;
    let lscore = avg / wstd;

    if (lscore < min) {
      min = lscore;
      minI = j;
    }

    if (hscore > max) {
      max = hscore;
      maxI = j;
    }
  }

  console.log(
      '[%d] min: key=%s hsh=%s std=%d avg=%d pos=%d || ' +
          'max: key=%s hsh=%s std=%d avg=%d pos=%d',
      i,
      probes[minI], hash(probes[minI]).toString(16),
      (r.stddev[minI] / r.avg[minI]).toFixed(2),
      r.avg[minI].toFixed(2),
      findPos(probes[minI]),
      probes[maxI], hash(probes[maxI]).toString(16),
      (r.stddev[maxI] / r.avg[maxI]).toFixed(2),
      r.avg[maxI].toFixed(2),
      findPos(probes[maxI]));

  if (findPos(probes[minI]) > findPos(probes[maxI]))
    hits++;

  results.push(probes[minI], probes[maxI]);

  console.log(keys.join(':') + '@' + results.join(':'));
}
console.log(hits);
