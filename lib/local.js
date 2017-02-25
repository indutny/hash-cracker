'use strict';

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot
const common = require('./common');
const hash = common.hash;
const key = common.key;
const findPos = common.findPos;

const REPEAT = 10;
const EXTRACT_COUNT = 64;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

function test(pre, list) {
  const o = {};
  o[pre] = null;
  for (let i = 0; i < list.length; i++)
    o[list[i]] = null;
  return o;
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
      findPos(keys, probes[minI]),
      probes[maxI], hash(probes[maxI]).toString(16),
      (r.stddev[maxI] / r.avg[maxI]).toFixed(2),
      r.avg[maxI].toFixed(2),
      findPos(keys, probes[maxI]));

  if (findPos(keys, probes[minI]) > findPos(keys, probes[maxI]))
    hits++;

  results.push(probes[minI], probes[maxI]);

  console.log(keys.join(':') + '@' + results.join(':'));
}
console.log('Hits %d', hits);
