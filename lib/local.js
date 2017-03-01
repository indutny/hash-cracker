'use strict';

const util = require('util');

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot
const SEED = 1;

const common = require('./common');
const hash = common.createHash(SEED);
const key = common.key;
const findPos = common.findPos;

const REPEAT = 10;
const EXTRACT_COUNT = 128;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

let offset = (Math.random() * 0xffffffff) >>> 0;
function genKey() {
  return common.genKey(offset++);
}

function test(pre, list) {
  const o = {};
  o[pre] = null;
  for (let i = 0; i < list.length; i++)
    o[list[i]] = null;
  return o;
}

const keys = [];
// 18 keys trigger hashmap
for (let i = 0; i < KEY_COUNT; i++)
  keys.push(genKey());

keys.sort((a, b) => hash(a) - hash(b));
console.log(hash(keys[0]));

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

console.error('number of tests %d', REPEAT * PROBES_COUNT);
console.error('[' + keys.map(k => hash(k).toString(16)).join(',') + ']');

let results = [];
let hits = 0;
for (let i = 0; i < EXTRACT_COUNT; i++) {
  const probes = [];
  for (let j = 0; j < PROBES_COUNT; j++)
    probes.push(genKey());

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

  const keyHashes = keys.map(hash);
  const minHash = hash(probes[minI]);
  const maxHash = hash(probes[maxI]);

  console.error(
    '[%d] l: %s std=%d avg=%d pos=%d || r: %s std=%d avg=%d pos=%d',
    i,
    probes[minI],
    (r.stddev[minI] / r.avg[minI]).toFixed(2),
    r.avg[minI].toFixed(1),
    findPos(keyHashes, minHash),
    probes[maxI],
    (r.stddev[maxI] / r.avg[maxI]).toFixed(2),
    r.avg[maxI].toFixed(1),
    findPos(keyHashes, maxHash));

  if (findPos(keyHashes, minHash) > findPos(keyHashes, maxHash))
    hits++;

  results.push(probes[minI], probes[maxI]);
  console.log('Hits %d%%', (100 * (hits * 2) / results.length).toFixed(2));
}
console.error('');
console.error('Hit=%d for seed=%s', hits, SEED.toString(16));
console.log(keys.join(':') + '@' + results.join(':'));
