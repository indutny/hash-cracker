#!/usr/bin/env node
'use strict';

function Source2Blob(name, input, output) {
  this.name = name.replace(/[^a-z]/ig, '_').toLowerCase();
  this.blobName = this.name.toUpperCase();

  this.input = input;
  this.output = output;
  this.pos = 0;

  this.input.once('data', (data) => {
    for (let i = 0; i < data.length; i++)
      this.printByte(data[i]);
  });
  this.input.once('end', () => this.postfix());

  this.prefix();
}

Source2Blob.prototype.prefix = function prefix() {
  this.output.write(`#ifndef BLOB_${this.blobName}_\n`);
  this.output.write(`#define BLOB_${this.blobName}_\n\n`);

  this.output.write(`static const char ${this.name}[] = {\n`);
};

Source2Blob.prototype.postfix = function postfix() {
  this.output.write('  0\n');
  this.output.write('};\n');
  this.output.write('\n');
  this.output.write(`#endif  /* BLOB_${this.blobName}_ */\n`);
};

Source2Blob.prototype.printByte = function printByte(b) {
  // Indent
  if (this.pos === 0)
    this.output.write(' ');

  let bs = b.toString(16);
  if (bs.length < 2)
    bs = '0' + bs;
  this.output.write(` 0x${bs},`);

  if (++this.pos === 13) {
    this.pos = 0;
    this.output.write('\n');
  }
};

let s;
if (process.argv.length <= 3) {
  s = new Source2Blob(process.argv[2], process.stdin, process.stdout);
} else {
  const fs = require('fs');
  const input = fs.createReadStream(process.argv[3]);
  const output = fs.createWriteStream(process.argv[4]);
  s = new Source2Blob(process.argv[2], input, output);
}
