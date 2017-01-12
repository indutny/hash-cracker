'use strict';

const async = require('async');
const spawn = require('child_process').spawn;
const net = require('net');

function measure(req, callback) {
  const p = spawn('./client', [
    '8000', '127.0.0.1',
    req
  ], {
    stdio: [ null, 'pipe', 'inherit' ]
  });

  let all = '';
  p.stdout.on('data', (chunk) => {
    all += chunk;
  });

  p.stdout.on('end', () => {
    const lines = all.split('\n').slice(0, -1).map((line) => line | 0);

    let avg = 0;
    for (let i = 0; i < lines.length; i++)
      avg += lines[i];
    avg /= lines.length;
    callback(null, avg);
  });
}

function hash(str, seed) {
  let hash = seed;

  for (let i = 0; i < str.length; i++) {
    hash = (hash + str.charCodeAt(i)) | 0;
    hash = (hash + (hash << 10)) | 0;
    hash ^= hash >>> 6;
  }

  hash = (hash + (hash << 3)) | 0;
  hash ^= (hash >>> 11);
  hash = (hash + (hash << 15)) | 0;

  return (hash >>> 0) & 0x3fffffff;
}

const ALPHABET =
    '!#$%&\'*+-.^_`|~abcdefghijklmnopqrstuvwxyz'.split('');
const TARGET_KEYS = [];

for (let i = 100; i < 118; i++) {
  TARGET_KEYS.push('x' + i);
}

function key(i) {
  let r = '';
  let t = i;
  while (r.length < 3) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}

function getKeys(seed) {
  let a = 0;
  let b = 0;
  let c = 0;

  let t = seed;
  for (let i = 0; i < 10; i++) {
    a |= (t & 1) << i;
    t >>>= 1;
    b |= (t & 1) << i;
    t >>>= 1;
    c |= (t & 1) << i;
    t >>>= 1;
  }

  if (seed % 100000 === 0) {
    console.log(seed, a, b, c);
    console.log(process.memoryUsage());
  }

  return [
    '1' + key(c),
    '2' + key(b),
    '3' + key(a)
  ];
}

function probe(seed) {
  const keys = getKeys(seed);

  const headers = {};
  for (let i = 0; i < keys.length; i++)
    headers[keys[i]] = null;

  process.nextTick(() => {
    probe(seed + 1);
  });
}

probe(0);
