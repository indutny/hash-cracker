'use strict';

const http = require('http');

http.createServer((req, res) => {
  let chunks = '';
  req.on('data', chunk => chunks += chunk);
  req.once('end', () => {
    let body;
    try {
      body = JSON.parse(chunks);
    } catch (e) {
      console.error(e);
    }
    res.end();
  });
}).listen(8000, () => {
  console.log('Listening');
});
