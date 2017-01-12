'use strict';

const csv = require('csv');

const binCount = process.argv[2] | 0;
const from = process.argv[3] | 0;
const to = process.argv[4] | 0;
const bins = new Array(binCount);
const sbins = new Array(binCount);

bins.fill(0);
sbins.fill(0);

const data = [];
let all = '';

const parser = csv.parse();

process.stdin.pipe(parser);

let max = 0;
let total = 0;
let matchAvg = 0;
let matchAvgCount = 0;

parser.on('data', ([i, avg, match]) => {
  i |= 0;
  avg = parseFloat(avg);
  match |= 0;
  match = i === 6;
  if (match) {
    matchAvg += avg;
    matchAvgCount++;
  }

  let bin = (((avg - from) / (to - from)) * binCount) | 0;
  if (bin < 0)
    return;
  if (bin >= binCount)
    return;
  bin = Math.min(bin, binCount - 1);
  total++;

  max = Math.max(max, ++bins[bin]);
  if (match)
    sbins[bin]++;
});

parser.on('end', () => {
  const tlen = to.toString().length + 14;

  bins.forEach((bin, i) => {
    const p = ((bin / total) * 100).toFixed(0);

    bin /= max;
    bin *= 80;

    let sbin = sbins[i];
    sbin /= max;
    sbin *= 80;

    const pc = bin === 0 ? 100 : ((sbin / bin) * 100).toFixed(0);

    let r = from + ((i * (to - from) / binCount) | 0);
    r += ` t=${p}% c=${pc}%`;
    while (r.length < tlen)
      r += ' ';
    r += ': ';

    let j;
    for (j = 0; j < sbin; j++){
      r += '#';
    }

    for (j; j < bin; j++){
      r += '-';
    }

    console.log(r);
  });

  matchAvg /= matchAvgCount;
  console.log(matchAvg);
});
