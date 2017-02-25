'use strict';

// Change this if needed
exports.SEED = 1;

// 2093 hits the limit of transitions
exports.PROBES_COUNT = 2093;

// 18 keys trigger hashmap, we don't want this to happen
exports.KEY_COUNT = 17;

function hash(str) {
  let hash = exports.SEED;

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
exports.hash = hash;

const ALPHABET_FIRST =
    'abcdefghijklmnopqrstuvwxyz'.split('');
const ALPHABET =
    '0123456789abcdefghijklmnopqrstuvwxyz'.split('');

function key(i) {
  let r = '';
  let t = i;
  r += ALPHABET_FIRST[t % ALPHABET_FIRST.length];
  t = (t / ALPHABET_FIRST.length) | 0;
  while (r.length < 4) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}
exports.key = key;

exports.findPos = function findPos(keys, k) {
  const h = hash(k);

  let last = 0;
  let minPos = null;
  let maxPos = null;
  for (let i = 0; i < keys.length; i++) {
    let next = hash(keys[i]);
    if (last <= h && h < next)
      return i;
    last = next;
  }
  return keys.length;
};
