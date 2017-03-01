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

const postfix = [];
for (let i = 0; i < keys.length; i++)
  postfix.push(JSON.stringify(keys[i]) + ':0');

function genProbeJSON(postfix, probe) {
  return '{' + JSON.stringify(probe) + ':0,' + postfix.join(',') + '}';
}

function measure(json) {
  const start = process.hrtime();
  JSON.parse(json);
  const end = process.hrtime(start);

  return end[0] * 1e9 + end[1];
};

let lavg = 0;
let havg = 0;
let count = 0;
for (;;) {
  let low;
  do
    low = genKey();
  while (hash(low) > keyHashes[0]);
  let high;
  do
    high = genKey();
  while (hash(high) < keyHashes[keyHashes.length - 1]);
  const jLow = genProbeJSON(postfix, low);
  const jHigh = genProbeJSON(postfix, high);

  const jProbes = [];
  for (let i = 0; i < PROBES_COUNT; i++)
    jProbes.push(genProbeJSON(postfix, genKey()));
  jProbes.forEach(j => JSON.parse(j));

  if (Math.random() < 0.5) {
    lavg += measure(jLow);
    havg += measure(jHigh);
  } else {
    havg += measure(jHigh);
    lavg += measure(jLow);
  }
  count++;

  console.log('%d %d', (lavg / count).toFixed(1), (havg / count).toFixed(1));
}
