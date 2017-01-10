'use strict';

const async = require('async');
const spawn = require('child_process').spawn;

function measure(req, callback) {
  const p = spawn('./client', [
    '8000', '127.0.0.1',
    req
  ], {
    stdio: [ null, 'pipe', null ]
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
    hash ^= hash >> 6;
  }

  hash = (hash + (hash << 3)) | 0;
  hash ^= (hash >> 11);
  hash = (hash + (hash << 15)) | 0;

  return hash >>> 0;
}

const POPULATION_COUNT = 64;
const SURVIVOR_COUNT = 16;
const NEWBORN_COUNT = 8;
const KEY_COUNT = 2;
const KEY_SIZE = 1;

let top = { keys: [], time: 0 };

function merge(a, b) {
  const res = { keys: new Array(KEY_COUNT), time: 0 };
  for (let i = 0; i < res.keys.length; i++)
    res.keys[i] = Math.random() < 0.5 ? a.keys[i] : b.keys[i];

  return res;
}

const ALPHABET =
    '!#$%&\'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('');

let first = false;

function generate() {
  const keys = [];

  while (keys.length < KEY_COUNT) {
    let key = '';
    let t = (Math.random() * 0xffffffff) >>> 0;
    for (let j = 0; j < KEY_SIZE; j++) {
      key += ALPHABET[t % ALPHABET.length];
      t = (t / ALPHABET.length) | 0;
    }
    if (first && (hash(key, 1) & 255) !== 1)
      continue;

    keys.push(key);
  }

  first = false;

  return { keys: keys, time: 0 };
}

function mutate(population) {
  population.sort((a, b) => b.time - a.time);

  population = population.slice(0, SURVIVOR_COUNT);
  for (let i = 0; i < NEWBORN_COUNT; i++)
    population.push(generate());
  while (population.length < POPULATION_COUNT) {
    let i = (Math.random() * SURVIVOR_COUNT) | 0;
    let j;
    do
      j = (Math.random() * SURVIVOR_COUNT) | 0;
    while (i == j);

    population.push(merge(population[i], population[j]));
  }
  return population;
}

function getHeader(ent) {
  let r = 'GET / HTTP/1.1\r\n';
  for (let i = 0; i < ent.keys.length; i++)
    r += `${ent.keys[i]}:.\r\n`;
  r += '\r\n';
  return r;
}

function epoch(population, callback) {
  async.forEachSeries(population, (ent, callback) => {
    measure(getHeader(ent), (err, avg) => {
      process.stdout.write('.');
      if (err || isNaN(avg))
        return callback(null);

      ent.time = avg;

      if (top.time < avg) {
        top.time = avg;
        top.keys = ent.keys;
      }

      callback(null);
    });
  }, (err) => {
    population = mutate(population);
    top.time = (top.time + population[0].time) / 2;
    console.log('\nepoch end, global best %d, local best %d',
                top.time, population[0].time);
    printCollisions();
    callback(null, population);
  });
}

let population = [];
for (let i = 0; i < POPULATION_COUNT; i++) {
  population.push(generate());
}

let total = 0;

function printCollisions() {
  let check = new Map();
  let coll = 0;
  for (let i = 0; i < top.keys.length; i++) {
    const idx = hash(top.keys[i], 1) & 15;
    if (check.has(idx))
      coll++;
    else
      check.set(idx, true);
  }
  console.log('Actual collisions %d', coll);
}

epoch(population, function done(err, population) {
  epoch(population, done);
});
