'use strict';

const async = require('async');
const spawn = require('child_process').spawn;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const SEED = 1;
const REPEAT = 10;
const EXTRACT_COUNT = 96;
const PARALLEL = 10;

// 2093 hits the limit of transitions
const PROBES_COUNT = 2093;

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

const ALPHABET =
    '!#$%&\'*+-.^_`|~abcdefghijklmnopqrstuvwxyz'.split('');

function key(i) {
  let r = '';
  let t = i;
  while (r.length < 4) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}

const keys = [];
// 18 keys trigger hashmap
for (let i = 0; i < 17; i++) {
  keys.push(key(i));
}

keys.sort((a, b) => hash(a) - hash(b));

function measure(probes, callback) {
  const p = spawn('./client', [
    PORT, HOST,
    REPEAT,
    probes.join(':'),
    keys.join(':')
  ], {
    stdio: [ null, 'pipe', 'inherit' ]
  });

  let all = '';
  p.stdout.on('data', (chunk) => {
    all += chunk;
  });

  p.stdout.on('end', () => {
    const lines = all.split('\n').slice(0, -1).map((line) => parseFloat(line));

    let avg = new Array(lines.length / 2);
    let stddev = new Array(avg.length);
    for (let i = 0; i < lines.length; i += 2) {
      avg[i >>> 1] = lines[i];
      stddev[i >>> 1] = lines[i + 1];
    }
    callback(null, { avg: avg, stddev: stddev });
  });
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

console.log(`const char* keys[] = ${toCArray(keys)};`);

let offset = 0;
let results = [];
let iter = [];
for (let i = 0; i < EXTRACT_COUNT; i++)
  iter.push(i);

let iteration = 0;
let hits = 0;
async.forEachLimit(iter, PARALLEL, (_, callback) => {
  const probes = [];
  for (let j = 17; j < 17 + PROBES_COUNT; j++)
    probes.push(key(j + offset));
  offset += probes.length;

  measure(probes, (err, r) => {
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
        iteration++,
        probes[minI], hash(probes[minI]).toString(16),
        (r.stddev[minI] / r.avg[minI]).toFixed(2),
        r.avg[minI].toFixed(2),
        findPos(probes[minI]),
        probes[maxI], hash(probes[maxI]).toString(16),
        (r.stddev[maxI] / r.avg[maxI]).toFixed(2),
        r.avg[maxI].toFixed(2),
        findPos(probes[maxI]));

    results.push(probes[minI], probes[maxI]);

    if (findPos(probes[minI]) > findPos(probes[maxI]))
      hits++;
    console.log(keys.join(':') + '@' + results.join(':'));

    callback(null);
  });
}, () => {
  console.log('Hits %d', hits);
});
