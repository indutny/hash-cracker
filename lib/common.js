'use strict';

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

function key(i) {
  let r = '';
  let t = i >>> 0;
  r += ALPHABET_FIRST[t % ALPHABET_FIRST.length];
  t = (t / ALPHABET_FIRST.length) | 0;
  while (r.length < 4) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}
exports.key = key;

function bigKey(i) {
  let r = '';
  let t = i >>> 0;
  r += ALPHABET_FIRST[t % ALPHABET_FIRST.length];
  t = (t / ALPHABET_FIRST.length) | 0;
  while (t > 0) {
    r += ALPHABET[t % ALPHABET.length];
    t = (t / ALPHABET.length) | 0;
  }
  return r;
}
exports.bigKey = bigKey;

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

function hashToStr(h) {
  let s = h.toString(16);
  while (s.length < 8)
    s = '0' + s;
  return s;
}
exports.hashToStr = hashToStr;

function splitTimings(timings) {
  let avg = 0;
  let stddev = 0;
  for (let i = 0; i < timings.length; i++) {
    const time = timings[i].time;
    avg += time;
    stddev += Math.pow(time, 2);
  }
  avg /= timings.length;
  stddev /= timings.length;
  stddev -= Math.pow(avg, 2);
  stddev = Math.sqrt(stddev);

  let min = Infinity;
  let max = 0;
  for (let i = 0; i < timings.length; i++) {
    const time = timings[i].time;
    min = Math.min(min, time);
    max = Math.max(max, time);
  }

  function select(centers, time) {
    const dist = centers.map((c) => {
      return Math.pow(c - time, 2);
    });

    let min = Infinity;
    let minJ = 0;
    for (let j = 0; j < dist.length; j++) {
      if (dist[j] > min)
        continue;

      min = dist[j];
      minJ = j;
    }

    return minJ;
  }

  const centers = [ min, max ];
  const means = centers.map(_ => ({ mean: 0, count: 0 }));
  for (;;) {
    for (let i = 0; i < timings.length; i++) {
      const time = timings[i].time;
      const centerIndex = select(centers, time);

      means[centerIndex].mean += time;
      means[centerIndex].count++;
    }

    let d = 0;
    for (let i = 0; i < centers.length; i++) {
      const newCenter = means[i].mean / means[i].count;
      d = Math.max(d, Math.abs(newCenter - centers[i]));
      centers[i] = newCenter;
    }

    if (d < 1)
      break;
  }
  centers.sort((a, b) => b - a);

  const split = centers.map(_ => []);
  for (let i = 0; i < timings.length; i++) {
    const centerIndex = select(centers, timings[i].time);

    split[centerIndex].push(timings[i]);
  }

  return { centers: centers, left: split[0], right: split[1] };
}
exports.splitTimings = splitTimings;

function toJSON(keys) {
  return '{' + keys.map(key => JSON.stringify(key) + ':0').join(',') + '}';
}
exports.toJSON = toJSON;

function amplifyJSON(list, postfix, amp) {
  const res = [];
  for (let i = 0; i < amp; i++)
    res.push(toJSON(list.concat(postfix)));
  return `[${res.join(',')}]`;
}
exports.amplifyJSON = amplifyJSON;

function getProbe(spoof, list, postfix, amp) {
  function computeSpoof() {
    return spoof.map((s) => {
      return toJSON(list.concat(s));
    }).join(',');
  }

  const r = [];
  // Re-spoof every 8192 iterations
  for (let i = 0; i < amp; i+= 8192)
    r.push(computeSpoof(), amplifyJSON(list, postfix, Math.min(8192, amp - i)));
  return '[' + r.join(',') + ']';
}
exports.getProbe = getProbe;
