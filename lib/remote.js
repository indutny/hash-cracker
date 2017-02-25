'use strict';

const async = require('async');
const spawn = require('child_process').spawn;

const common = require('./common');
const hash = common.hash;
const key = common.key;
const findPos = common.findPos;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const REPEAT = 30;
const EXTRACT_COUNT = 48;
const PARALLEL = 1;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

const keys = [];
for (let i = 0; i < KEY_COUNT; i++) {
  keys.push(key(i));
}

keys.sort((a, b) => hash(a) - hash(b));

function measure(probes, callback) {
  const p = spawn('./out/Release/client', [
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
        findPos(keys, probes[minI]),
        probes[maxI], hash(probes[maxI]).toString(16),
        (r.stddev[maxI] / r.avg[maxI]).toFixed(2),
        r.avg[maxI].toFixed(2),
        findPos(keys, probes[maxI]));

    results.push(probes[minI], probes[maxI]);

    if (findPos(keys, probes[minI]) > findPos(keys, probes[maxI]))
      hits++;
    console.log(keys.join(':') + '@' + results.join(':'));

    callback(null);
  });
}, () => {
  console.log('Hits %d', hits);
});
