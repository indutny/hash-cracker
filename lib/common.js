'use strict';

// 2093 hits the limit of transitions
exports.PROBES_COUNT = 2093;

exports.SPOOF_SIZE = 1024 + 512;

// 18 keys trigger hashmap, we don't want this to happen
exports.KEY_COUNT = 17;

function createHash(seed) {
  return (str) => {
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
  };
}
exports.createHash = createHash;

const ALPHABET_FIRST =
    'abcdefghijklmnopqrstuvwxyz'.split('');
const ALPHABET =
    '0123456789abcdefghijklmnopqrstuvwxyz'.split('');

function genKey(i) {
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
exports.genKey = genKey;

function genBigKey(i) {
  let r = '';
  let t = i;
  r += ALPHABET_FIRST[t % ALPHABET_FIRST.length];
  t = (t / ALPHABET_FIRST.length) | 0;
  while (t > 0) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}
exports.genBigKey = genBigKey;

exports.findPos = function findPos(hashes, hash) {
  let last = 0;
  let minPos = null;
  let maxPos = null;
  for (let i = 0; i < hashes.length; i++) {
    let next = hashes[i];
    if (last <= hash && hash < next)
      return i;
    last = next;
  }
  return hashes.length;
};
