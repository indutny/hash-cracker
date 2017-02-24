'use strict';

const keys = [
  "+!!!","$!!!","&!!!","b!!!","|!!!","'!!!","%!!!","!!!!","*!!!","-!!!","`!!!",
  ".!!!","a!!!","#!!!","^!!!","_!!!","~!!!"
];
const probes = [
  "#r!!","n%#!","we$!","+e#!",".m$!","ub%!","s#&!","i^&!","zt'!","k+*!","md+!",
  "cd*!","go+!","~e-!","tm.!",".$.!","%*_!","q&^!","mt_!","!i_!","*g|!","o`|!",
  "#g~!",".h~!","hpa!","`'b!","drb!","!gb!","mcd!","oqc!","|we!","wge!","taf!",
  "^of!","b.g!","$ug!","$$i!","w|i!","dxi!","dqj!","b%l!","o#k!","#sl!","oxl!",
  "-mm!","w_n!","qvn!","s_o!","zop!","$up!","nkq!","$vq!","ees!","+gs!","qat!",
  "$vs!","x'u!","_nu!","ecv!","kgv!","phx!","-kw!","c_y!","aiy!","elz!","`pz!",
  "p`!#","yi!#","~q##","t+$#","sv$#",".n%#","hj&#","~$'#","_-'#","ot'#","qe*#",
  "lk*#",".q-#","$e-#","kt.#","dw.#","|u^#","~^^#","#k_#","+_`#","kk`#","*h|#",
  "!y~#","r_~#","pla#","$#b#","i.c#","bpb#","kgd#","cvc#"
];

function convert(value) {
  const bits = value.charCodeAt(0) |
               (value.charCodeAt(1) << 8) |
               (value.charCodeAt(2) << 16) |
               (value.charCodeAt(3) << 24);
  let res = bits.toString(16);
  while (res.length < 8) {
    res = '0' + res;
  }
  return '0x' + res;
}

process.stdout.write(`static unsigned int key_count = ${keys.length};\n`);
process.stdout.write(`static unsigned int probe_count = ${probes.length};\n`);
process.stdout.write('static unsigned int dataset[] = {\n');
for (let i = 0; i < keys.length; i++) {
  process.stdout.write(convert(keys[i]) + ',');
  if (i % 8 === 7)
    process.stdout.write('\n');
}
process.stdout.write('\n');
for (let i = 0; i < probes.length; i++) {
  process.stdout.write(convert(probes[i]) + ',');
  if (i % 8 === 7)
    process.stdout.write('\n');
}
process.stdout.write('};\n');
