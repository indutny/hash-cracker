'use strict';

const async = require('async');

const SEED = 1;

const common = require('./common');
const Client = require('./client');
const hash = common.createHash(SEED);
const findPos = common.findPos;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const REPEAT = 1;
const EXTRACT_COUNT = Infinity;

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

const client = new Client(PORT, HOST);

let offset = (Math.random() * 0xffffffff) >>> 0;
function genKey() {
  return common.genKey(offset++);
}

const keys = [];
for (let i = 0; i < KEY_COUNT; i++)
  keys.push(genKey());
const keyHashes = keys.map(hash).sort((a, b) => a - b);

const postfix = [];
for (let i = 0; i < keys.length; i++)
  postfix.push(JSON.stringify(keys[i]) + ':0');

function genProbeJSON(postfix, probe) {
  return '{' + JSON.stringify(probe) + ':0,' + postfix.join(',') + '}';
}

function measure(postfix, probes, callback) {
  function repeat(body, callback) {
    let i = 0;
    const timings = [];

    function onDone(err) {
      if (err)
        return callback(err);

      callback(null, timings);
    }

    async.whilst(() => {
      return i++ < REPEAT;
    }, (callback) => {
      client.request(body, (err, time) => {
        if (err)
          return callback(err);
        timings.push(time);
        callback(null);
      });
    }, onDone);
  }

  async.map(probes, (probe, callback) => {
    const body = genProbeJSON(postfix, probe);
    repeat(body, callback);
  }, callback);
}

console.log('number of tests %d', REPEAT * PROBES_COUNT);
console.log(JSON.stringify(keys));
console.log('[' + keys.map(k => hash(k).toString(16)).join(',') + ']');

const probes = [];
for (let i = 0; i < PROBES_COUNT; i++)
  probes.push(genKey());

let results = [];

let iteration = 0;
let hits = 0;
async.whilst(() => {
  return iteration++ < EXTRACT_COUNT;
}, (callback) => {
  measure(postfix, probes, (err, timings) => {
    probes.forEach((probe, i) => {
      const probeTimings = timings[i];
      const pos = findPos(keyHashes, hash(probe));

      probeTimings.forEach((t) => {
        console.log(`${t},${pos}`);
      });
    });

    callback(null);
  });
}, () => {
  client.exit();
});
