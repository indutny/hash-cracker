'use strict';

const SEED = 1;

const common = require('./common');
const hash = common.createHash(SEED);

const KEY_COUNT = 17;
const AMPLIFICATION = 20;

let offset = 0;
function genKey() {
  return common.genKey(offset++);
}

const keys = [];
for (let i = 0; i < KEY_COUNT; i++)
  keys.push(genKey());
keys.sort((a, b) => hash(a) - hash(b));
const keyHashes = keys.map(hash).sort((a, b) => a - b);

const postfix = [];
for (let i = 0; i < keys.length; i++)
  postfix.push(JSON.stringify(keys[i]) + ':0');

function genJSON(spoof, postfix, probe) {
  let r = '[' +
      spoof.map(spoofKey => `{"@":0,${JSON.stringify(spoofKey)}:0}`).join(',');
  for (let i = 0; i < AMPLIFICATION; i++)
    r += ',{"@":0,' + JSON.stringify(probe) + ':0,' + postfix.join(',') + '}';
  return r + ']';
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

let low;
do
  low = genKey();
while (hash(low) > keyHashes[0]);
let high;
do
  high = genKey();
while (hash(high) < keyHashes[keyHashes.length - 1]);

const jLow = genJSON(spoof, postfix, low);
const jHigh = genJSON(spoof, postfix, high);

const timing = {
  high: { avg: 0, stddev: 0, count: 0 },
  low: { avg: 0, stddev: 0, count: 0 },
  rhigh: { avg: 0, stddev: 0, count: 0 },
  rlow: { avg: 0, stddev: 0, count: 0 }
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
  let rht;
  let rlt;

  if (Math.random() < 0.5) {
    ht = measure(jHigh);
    lt = measure(jLow);

    rlt = measure(jLow);
    rht = measure(jHigh);
  } else {
    rlt = measure(jLow);
    rht = measure(jHigh);

    ht = measure(jHigh);
    lt = measure(jLow);
  }

  const h = addTiming(timing.high, ht);
  const l = addTiming(timing.low, lt);
  const rh = addTiming(timing.rhigh, rht);
  const rl = addTiming(timing.rlow, rlt);

  if (timing.high.count % 100 !== 0)
    continue;
  console.log('delta=%d reference=%d hdev=%d ldev=%d count=%d',
              (l.avg - h.avg).toFixed(1),
              (rl.avg - rh.avg).toFixed(1),
              h.stddev.toFixed(1),
              l.stddev.toFixed(1),
              timing.high.count);
}
