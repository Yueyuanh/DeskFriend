// Run against REAL hardware: node scripts/verify_face_sync.mjs http://BOARD_IP
// Downloads outgoing diagnostic frames, never uploads pixel data.
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const board=process.argv[2],out=path.join(root,'.pio','face_qa');
fs.mkdirSync(out,{recursive:true});
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const server=http.createServer((req,res)=>{
  let url=new URL(req.url,'http://localhost');
  let name=url.pathname==='/'?'DeskFriend_index.html':url.pathname.slice(1);
  if(name==='device_monitor.mjs'||name.startsWith('vendor/')||name.startsWith('fonts/'))name='assets/'+name;
  const file=path.resolve(root,name);
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)){res.writeHead(404);res.end();return;}
  const ext=path.extname(file);res.setHeader('Content-Type',({'.html':'text/html','.mjs':'text/javascript','.js':'text/javascript','.ttf':'font/ttf'})[ext]||'application/octet-stream');res.end(fs.readFileSync(file));
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const profile=path.join(out,`chrome-profile-${process.pid}`),referenceUrl=`http://127.0.0.1:${server.address().port}/?test=1`;
const chromePath=process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe';
if(!fs.existsSync(chromePath)){server.close();throw new Error('Chrome not found; set CHROME_PATH to your Chrome executable');}
const chrome=spawn(chromePath,['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0',`--user-data-dir=${profile}`,'--enable-unsafe-swiftshader','--use-angle=swiftshader',referenceUrl],{windowsHide:true,stdio:'ignore'});
let ws;
try {
  let pages;
  for(let i=0;i<100;++i){try{const port=fs.readFileSync(path.join(profile,'DevToolsActivePort'),'utf8').split('\n')[0];pages=await(await fetch(`http://127.0.0.1:${port}/json`)).json();if(pages.some(x=>x.type==='page'&&x.url===referenceUrl))break;}catch{}await pause(100);}
  const page=pages?.find(x=>x.type==='page'&&x.url===referenceUrl);
  if(!page)throw new Error('Fresh reference browser did not start');
  ws=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;});
  let id=0;const pending=new Map(),errors=[];
  ws.onmessage=event=>{const data=JSON.parse(event.data);if(data.id){const p=pending.get(data.id);pending.delete(data.id);if(!p)return;if(data.error)p.reject(data.error);else p.resolve(data.result);}else if(data.method==='Runtime.exceptionThrown')errors.push(data.params);};
  const call=(method,params={})=>new Promise((resolve,reject)=>{const key=++id;pending.set(key,{resolve,reject});ws.send(JSON.stringify({id:key,method,params}));});
  await call('Runtime.enable');
  const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  for(let i=0;i<150;++i){if(await evaluate('!!window.deskFaceTest'))break;await pause(100);}
  if(!await evaluate('!!window.deskFaceTest'))throw new Error('Page did not initialize: '+JSON.stringify(errors));
  const faces=['idle','happy','curious','shy','pressed','greeting','eat','water','off','energy','night','thanks','miss','msg','cheer','sleepy','wow','sad','love','wink','think','scared','dance','dizzy','no','lowbat','charging','full','boot','offline','gsend','grecv','replyWait','replyDone'];
  const cases=faces.map(face=>({name:face,face,text:['gsend','grecv','replyWait','replyDone'].includes(face)?'浩拍了拍你':'',time:2.35,age:1.15,hold:face==='replyWait',holdProgress:.57}));
  for(const face of ['idle','happy','water','love','wink'])cases.push({name:face+'_caption',face,text:'你好，见到你啦',time:1.73,age:.16});
  for(const p of [0,.25,.5,1])cases.push({name:'hold_'+p,face:'pressed',time:1.73,age:1.1,hold:true,holdProgress:p});
  for(const t of [0,.03,.12,.25])cases.push({name:'subtitle_'+t,face:'greeting',text:'你好',time:1.73,age:t});
  for(const face of ['idle','curious','wow','boot'])cases.push({name:face+'_blink',face,time:1.73,age:.16,blink:true,blinkRemaining:.12});
  for(const h of [7,12,15,23])cases.push({name:'clock_'+h,face:'clock',time:2.35,age:1.15,wallClockMs:Date.UTC(2026,10,1,h-8,36,23,625),utcOffsetMinutes:480});
  for(const age of [.3,.95,1.5,4])cases.push({name:'chat_'+age,face:'chat',time:2.35,age,chat:{name:'小浩',pages:[['你好，今天一起','去散步吧！'],['第二页 English']],starts:[.9,3.5]},wallClockMs:Date.UTC(2026,10,1,7,36,23,625),utcOffsetMinutes:480});
  for(const anim of ['idle','wave','heart','cheer','jack','dance','bow','sleep','confused','cry','shrug','stretch','run'])cases.push({name:'stick_'+anim,face:'stick',time:2.35,age:1.15,stick:{anim,camera:false}});
  cases.push({name:'stick_camera',face:'stick',time:2.35,age:1.15,stick:{anim:'idle',camera:true,joints:[[32,16],[31,22],[28,23],[34,23],[30,34],[23,27],[38,20],[19,23],[41,14],[25,42],[35,42],[23,51],[38,50]],eyes:'closed',mouth:'open'}});
  fs.writeFileSync(path.join(out,'cases.json'),JSON.stringify(cases,null,2));
  const results=[];
  let seq=10000;
  for(const test of cases){
    const state={...test,protocol:2,seq:++seq,paused:true};delete state.name;
    const png=await evaluate(`window.deskFaceTest.draw(${JSON.stringify(state)})`);
    fs.writeFileSync(path.join(out,test.name+'_web.png'),Buffer.from(png.split(',')[1],'base64'));
    if(board){
      const response=await fetch(board+'/api/face',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(state),signal:AbortSignal.timeout(5000)});
      if(!response.ok)throw new Error(test.name+': '+response.status+' '+await response.text());
      let status;
      for(let i=0;i<120;++i){status=await(await fetch(board+'/api/state',{signal:AbortSignal.timeout(5000)})).json();if(status.displayed_seq===seq)break;await pause(20);}
      if(status.displayed_seq!==seq)throw new Error(test.name+': not displayed '+JSON.stringify(status));
      const screenshot=await fetch(board+'/api/screen.rgb565',{signal:AbortSignal.timeout(5000)});
      if(Number(screenshot.headers.get('X-Frame-Seq'))!==seq)throw new Error(test.name+': another browser changed the face during capture; close other DeskFriend tabs');
      const frame=await screenshot.arrayBuffer();
      if(frame.byteLength!==115200)throw new Error('Invalid frame '+frame.byteLength);
      fs.writeFileSync(path.join(out,test.name+'_lcd.rgb565'),Buffer.from(frame));
      results.push({name:test.name,state,render_us:status.render_us,flush_us:status.flush_us,heap:status.heap,psram:status.psram});
      fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(results,null,2));
      console.log(test.name,status.render_us,status.flush_us);
    }
  }
  fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(results,null,2));
  fs.writeFileSync(path.join(out,'browser_errors.json'),JSON.stringify(errors,null,2));
  if(errors.length)throw new Error('Browser exceptions: '+JSON.stringify(errors));
  console.log('Verified',cases.length,'reference states',board?'on real board':'browser only');
} finally {
  if(board)try{await fetch(board+'/api/face',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({protocol:2,seq:20000,face:'idle',time:0,age:0,paused:false}),signal:AbortSignal.timeout(2000)});}catch{}
  if(ws?.readyState===1){ws.send(JSON.stringify({id:999999,method:'Browser.close'}));await pause(100);ws.close();}
  chrome.kill();server.close();
}
