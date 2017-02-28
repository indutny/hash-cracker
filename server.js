'use strict';

const http = require('http');
const zlib = require('zlib');

http.createServer((req, res) => {
  let chunks = '';
  req.on('data', chunk => chunks += chunk);
  req.once('end', () => {
    let body;
    try {
      const buf = Buffer.from(chunks, 'base64');
      body = JSON.parse(zlib.inflateSync(buf).toString());
    } catch (e) {
    }
    res.end();
  });
}).listen(8000, () => {
  console.log('Listening');
});
