// Only semantic state is uploaded; LCD animation is rendered by LVGL locally.
export class DeviceMonitor {
  constructor() {
    for (const [key, id] of Object.entries({address:'deviceAddress',status:'deviceStatus',metrics:'deviceMetrics',imuText:'deviceImu',button:'deviceConnect'})) this[key]=document.getElementById(id);
    this.address.value=location.protocol==='file:'?'':location.host;
    this.button.textContent='刷新设备状态'; this.button.onclick=()=>this.poll(true);
    this.seq=0; this.status.textContent='等待连接 · 网页同步状态，LCD 使用 LVGL 本地绘制';
    this.timer=setInterval(()=>this.poll(),250); this.poll();
  }
  base() {
    const value=this.address.value.trim();
    if(!value)throw new Error('请填写串口输出的板子地址');
    const url=new URL(/^https?:\/\//.test(value)?value:`http://${value}`);
    if(!['http:','https:'].includes(url.protocol))throw new Error('设备地址必须使用 HTTP');
    return url.origin;
  }
  pushFace(state) {
    if(document.hidden||this.sending)return;
    let origin;try{origin=this.base();}catch{return;}
    const now=performance.now();
    const {time,age,wallClockMs,ch,cm,cs,cw,cmo,cd,csec,...semantic}=state;
    const key=JSON.stringify(semantic);
    if(now-(this.lastFaceAt||-1000)<(key!==this.lastKey?40:250))return;
    const body=JSON.stringify({protocol:2,seq:++this.seq,...state});
    if(new TextEncoder().encode(body).length>=4096){this.status.textContent='同步失败：状态超过 4095 字节';return;}
    this.sending=true;
    fetch(`${origin}/api/face`,{method:'POST',headers:{'Content-Type':'application/json'},body,signal:AbortSignal.timeout(1500)})
      .then(async response=>{
        if(!response.ok)throw new Error(`HTTP ${response.status}: ${await response.text()}`);
        const ack=await response.json();if(!ack.ok)throw new Error('设备未接受状态');
        this.lastKey=key;this.lastFaceAt=now;
        this.status.textContent=`状态已接收 #${ack.client_seq} · LCD 已显示 #${ack.displayed_seq}`;
      })
      .catch(error=>{this.status.textContent=`同步失败：${error.message}`;this.lastFaceAt=now;})
      .finally(()=>{this.sending=false;});
  }
  async poll(manual=false) {
    if(this.polling||document.hidden||!this.address.value.trim())return;
    this.polling=true;
    try {
      const response=await fetch(`${this.base()}/api/state`,{signal:AbortSignal.timeout(2000)});
      if(!response.ok)throw new Error(`HTTP ${response.status}`);
      const state=await response.json(),frameMs=(state.render_us+state.flush_us)/1000;
      if(state.local?.event_seq && state.local.event_seq!==this.localSeq){
        this.localSeq=state.local.event_seq;
        if(state.local.remaining_ms>0)this.onLocalFace?.(state.local);
      }
      this.metrics.textContent=`${state.face} · 已显示 #${state.displayed_seq} · 绘制 ${(state.render_us/1000).toFixed(1)} ms + SPI ${(state.flush_us/1000).toFixed(1)} ms · 上限约 ${Math.min(30,1000/Math.max(1,frameMs)).toFixed(0)} fps · PSRAM ${(state.psram/1048576).toFixed(2)} MB · RSSI ${state.rssi} dBm`;
      const imu=state.imu||{};
      this.imuText.textContent=imu.ready?`IMU 加速度 [${imu.acc.map(x=>x.toFixed(2)).join(', ')}] m/s² · 角速度 [${imu.gyro.map(x=>x.toFixed(2)).join(', ')}] rad/s`:'IMU 未就绪';
    }catch(error){if(manual)this.status.textContent=error.message;}
    finally{this.polling=false;}
  }
}
