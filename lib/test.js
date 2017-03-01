'use strict';

const SEED = 1;

const common = require('./common');
const hash = common.createHash(SEED);

const KEY_COUNT = common.KEY_COUNT;
const PROBES_COUNT = common.PROBES_COUNT;

let offset = (Math.random() * 0xffffffff) >>> 0;
function genKey() {
  return common.genKey(offset++);
}

const keys = [];
for (let i = 0; i < KEY_COUNT; i++)
  keys.push(genKey());
const keyHashes = keys.map(hash).sort((a, b) => a - b);

const prefix = [];
for (let i = 0; i < keys.length; i++)
  prefix.push(JSON.stringify(keys[i]) + ':0');

function genProbeJSON(prefix, probe) {
  return '{' + prefix.join(',') + ',' + JSON.stringify(probe) + ':0}';
}

function amplify(s, times) {
  let r = s;
  for (let i = 1; i < times; i++)
    r += ',' + s;
  return r;
}

function measure(json) {
  const start = process.hrtime();
  JSON.parse(json);
  const end = process.hrtime(start);

  return end[0] * 1e9 + end[1];
};

const spoof = [];
for (let i = 0; i < 1024 + 512; i++)
  spoof.push(genKey());
const spoofJSON = spoof.map(s => genProbeJSON(prefix, s)).join(',');

let low;
do
  low = genKey();
while (hash(low) > keyHashes[0]);
let high;
do
  high = genKey();
while (hash(high) < keyHashes[keyHashes.length - 1]);

const jLow = '[' + spoofJSON + ',' + amplify(genProbeJSON(prefix, low), 10) + ']';
const jHigh = '[' + spoofJSON + ',' + amplify(genProbeJSON(prefix, high), 10) + ']';

const timing = {
  high: { avg: 0, stddev: 0, count: 0 },
  low: { avg: 0, stddev: 0, count: 0 }
};

function addTiming(timing, time) {
  timing.avg += time;
  timing.stddev += Math.pow(time, 2);
  timing.count++;

  const stddev = (timing.stddev / timing.count) -
                 Math.pow(timing.avg / timing.count, 2);

  return {
    avg: timing.avg / timing.count,
    stddev: Math.sqrt(stddev)
  };
}

for (;;) {
  let lt;
  let ht;
  if (Math.random() < 0.5) {
    lt = measure(jLow);
    ht = measure(jHigh);
  } else {
    ht = measure(jLow);
    lt = measure(jHigh);
  }

  const h = addTiming(timing.high, ht);
  const l = addTiming(timing.low, lt);

  console.log('delta=%d hdev=%d ldev=%d',
              (l.avg - h.avg).toFixed(1),
              h.stddev.toFixed(1),
              l.stddev.toFixed(1));
}
