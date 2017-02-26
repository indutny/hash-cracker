'use strict';

const async = require('async');
const spawn = require('child_process').spawn;

const common = require('./common');
const hash = common.hash;
const key = common.key;
const findPos = common.findPos;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const REPEAT = 100;
const EXTRACT_COUNT = 96;
const PARALLEL = 1;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

const keys = [];
let offset = (Math.random() * 0xfffffff) | 0;
for (let i = 0; i < KEY_COUNT; i++) {
  keys.push(key(offset++));
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

    const total = {
      avg: lines[0],
      stddev: lines[1]
    };
    const results = [];
    for (let i = 2; i < lines.length; i += 2) {
      results.push({
        i: (i / 2 - 1),
        avg: lines[i],
        stddev: lines[i + 1]
      });
    }
    results.sort((a, b) => a.stddev - b.stddev);
    callback(null, {
      total,
      results
    });
  });
}

console.log('number of tests %d', REPEAT * PROBES_COUNT);
console.log('[' + keys.map(k => hash(k).toString(16)).join(',') + ']');

let results = [];
let iter = [];
for (let i = 0; i < EXTRACT_COUNT; i++)
  iter.push(i);

const perPos = [];
for (let i = 0; i < keys.length + 1; i++)
  perPos.push({ count: 0, avg: 0, stddev: 0 });

let iteration = 0;
let hits = 0;
async.forEachLimit(iter, PARALLEL, (_, callback) => {
  const probes = [];
  for (let i = 0; i < PROBES_COUNT; i++)
    probes.push(key(offset++));

  measure(probes, (err, res) => {
    console.log('total', res.total);

    res.results.map((item) => {
      return {
        pos: findPos(keys, probes[item.i]),
        item
      };
    }).sort((a, b) => {
      return a.pos - b.pos;
    }).forEach((item) => {
      const p = perPos[item.pos];

      // Filter out outliers
      const dist = Math.abs(item.item.avg - res.total.avg);
      if (dist > res.total.stddev / 1.5)
        return;

      p.count++;
      p.avg += item.item.avg;
      p.stddev += item.item.stddev;
    });

    perPos.forEach((p, i) => {
      console.log('%d, %d, %d, %d', i, p.count, (p.avg / p.count) - res.total.avg, p.stddev / p.count);
    });
    return callback(null);

    pairs.sort((a, b) => {
      return b.wstd - a.wstd;
    });

    if (pairs.length === 0) {
      console.log('not conclusive results, skip');
      return callback(null);
    }

    const pair = pairs[0];
    const left = pair.left;
    const right = pair.right;

    console.log(
        '[%d] left: key=%s hsh=%s std=%d avg=%d pos=%d || ' +
            'right: key=%s hsh=%s std=%d avg=%d pos=%d',
        iteration++,
        probes[left], hash(probes[left]).toString(16),
        r.stddev[left].toFixed(2),
        r.avg[left].toFixed(2),
        findPos(keys, probes[left]),
        probes[right], hash(probes[right]).toString(16),
        r.stddev[right].toFixed(2),
        r.avg[right].toFixed(2),
        findPos(keys, probes[right]));

    results.push(probes[left], probes[right]);

    if (findPos(keys, probes[left]) > findPos(keys, probes[right]))
      hits++;
    console.log('Hits %d%%', (100 * (hits * 2) / results.length).toFixed(2));
    console.log(keys.join(':') + '@' + results.join(':'));

    callback(null);
  });
}, () => {
});
