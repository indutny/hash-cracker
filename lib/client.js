'use strict';

// Needs to be run with --hash_seed=1 and node compiled --without-snapshot
const SEED = 1;
const AMPLIFICATION = 4096 * 8;
const SPOOF_COUNT = 1024 + 512;
const KEY_COUNT = 1;
const EXTRACT_COUNT = 2048;

const PORT = (process.argv[2] || 8000) | 0;
const HOST = process.argv[3] || '127.0.0.1';

const async = require('async');
const path = require('path');
const spawn = require('child_process').spawn;
const util = require('util');

const BINARY = path.join(__dirname, '..', 'out/Release/client');

const common = require('./common');

function Client(port, host) {
  this.port = port;
  this.host = host;

  this.proc = null;
  this.pending = [];

  this.respawn();
}
module.exports = Client;

Client.prototype.respawn = function respawn() {
  this.proc = spawn(BINARY, [ this.port, this.host ], {
    stdio: [ 'pipe', 'pipe', 'inherit' ]
  });
  this.proc.on('exit', () => {
    this.proc = null;
    this.respawn();
  });

  this.proc.stdin.on('error', () => {});
  this.proc.stdout.on('error', () => {});

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

    const line = split[0];
    pending.callback(null, parseFloat(line));

    this._sendPending();
  });

  this._sendPending();
};

Client.prototype._sendPending = function _sendPending() {
  if (this.pending.length === 0 || this.pending[0].proc === this.proc)
    return;

  this.pending[0].proc = this.proc;
  this.proc.stdin.write(this.pending[0].input);
};

Client.prototype.request = function request(body, callback) {
  this.pending.push({
    proc: null,
    input: body + '\n',
    callback
  });
  this._sendPending();
};

Client.prototype.exit = function exit() {
  this.proc.kill();
};
