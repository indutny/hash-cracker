'use strict';

const http = require('http');

http.createServer((req, res) => {
  res.end();
}).listen(8000, () => {
  console.log('Listening');
});

setInterval(() => {
  console.log(process.memoryUsage());
}, 5000);
