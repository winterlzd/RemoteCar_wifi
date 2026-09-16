#ifndef WEBPAGE_H
#define WEBPAGE_H

/*  Embedded gamepad HTML – stored in flash (PROGMEM)
 *  Served by WebServer on GET /                       */

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,
      maximum-scale=1,user-scalable=no">
<title>RC Car Controller</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{
  height:100%;overflow:hidden;
  touch-action:none;
  -webkit-user-select:none;user-select:none;
}
body{
  background:#0b0b1a;color:#e0e0e0;
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
  display:flex;flex-direction:column;
}

/* ---- header ---- */
.hdr{
  padding:10px 16px;
  background:rgba(0,0,0,.45);
  display:flex;align-items:center;gap:10px;
  font-size:13px;z-index:2;
  backdrop-filter:blur(8px);
}
.dot{
  width:9px;height:9px;border-radius:50%;
  background:#00e676;
  box-shadow:0 0 6px #00e676;
  transition:background .3s;
}
.dot.off{background:#ff5252;box-shadow:0 0 6px #ff5252}
.hdr .tag{margin-left:auto;opacity:.45;font-size:12px}

/* ---- joystick ---- */
.pad{
  flex:1;display:flex;align-items:center;justify-content:center;
  position:relative;
}
.base{
  width:min(68vmin,300px);height:min(68vmin,300px);
  border-radius:50%;
  background:radial-gradient(circle,rgba(255,255,255,.03) 0%,transparent 70%);
  border:2px solid rgba(255,255,255,.07);
  position:relative;
}
/* cross-hair */
.base::before,.base::after{
  content:'';position:absolute;
  background:rgba(255,255,255,.04);
}
.base::before{width:1px;height:100%;left:50%}
.base::after{width:100%;height:1px;top:50%}
/* ring guides */
.ring{
  position:absolute;border-radius:50%;
  border:1px dashed rgba(255,255,255,.05);
  top:50%;left:50%;transform:translate(-50%,-50%);
  pointer-events:none;
}
.ring.r1{width:60%;height:60%}
.ring.r2{width:30%;height:30%}
.knob{
  width:76px;height:76px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#00e6b8,#007a5e);
  position:absolute;top:50%;left:50%;
  transform:translate(-50%,-50%);
  box-shadow:0 0 28px rgba(0,230,184,.30);
  transition:box-shadow .15s;
  pointer-events:none;
  will-change:top,left;
}
.knob.active{box-shadow:0 0 38px rgba(0,230,184,.55)}

/* ---- footer ---- */
.ftr{
  background:rgba(0,0,0,.45);
  padding:10px 16px;
  display:flex;justify-content:space-between;align-items:center;
  z-index:2;backdrop-filter:blur(8px);
}
.info{
  display:flex;gap:20px;
  font-size:13px;font-family:'SF Mono',Menlo,Consolas,monospace;
}
.info .val{color:#00e676;min-width:40px;display:inline-block;text-align:right}
.info .val.neg{color:#ff8a80}

/* brake button */
.brake{
  background:linear-gradient(135deg,#e63946,#c1121f);
  color:#fff;border:none;
  padding:12px 32px;border-radius:10px;
  font-size:15px;font-weight:700;letter-spacing:1.5px;
  box-shadow:0 4px 16px rgba(230,57,70,.3);
  transition:transform .1s,box-shadow .1s;
}
.brake:active{
  transform:scale(.93);
  box-shadow:0 2px 8px rgba(230,57,70,.5);
}

/* mode buttons */
.modes{
  position:absolute;top:16px;right:16px;
  display:flex;flex-direction:column;gap:8px;z-index:3;
}
.mode-btn{
  background:rgba(255,255,255,.06);
  border:1px solid rgba(255,255,255,.1);
  color:#aaa;border-radius:8px;
  padding:8px 14px;font-size:12px;
  transition:all .2s;
}
.mode-btn.on{background:rgba(0,230,184,.15);border-color:#00e676;color:#00e676}

/* direction hint */
.dir{
  position:absolute;bottom:16px;left:50%;transform:translateX(-50%);
  font-size:11px;opacity:.3;z-index:3;letter-spacing:1px;
}
</style>
</head>
<body>

<div class="hdr">
  <div class="dot" id="dot"></div>
  <span id="status">已连接</span>
  <span class="tag">RC Car Controller</span>
</div>

<div class="pad">
  <div class="modes">
    <button class="mode-btn on" id="mNorm" onclick="setMode(1)">普通</button>
    <button class="mode-btn" id="mSlow" onclick="setMode(0.4)">慢速</button>
    <button class="mode-btn" id="mFast" onclick="setMode(1.5)">快速</button>
  </div>
  <div class="base" id="base">
    <div class="ring r1"></div>
    <div class="ring r2"></div>
    <div class="knob" id="knob"></div>
  </div>
  <div class="dir">↑ 前进 &nbsp; ↓ 后退 &nbsp; ← 左 &nbsp; → 右</div>
</div>

<div class="ftr">
  <div class="info">
    <div>L <span class="val" id="lv">0</span></div>
    <div>R <span class="val" id="rv">0</span></div>
  </div>
  <button class="brake" id="brakeBtn">BRAKE</button>
</div>

<script>
(function(){
  /* ---- elements ---- */
  var base = document.getElementById('base');
  var knob = document.getElementById('knob');
  var lv   = document.getElementById('lv');
  var rv   = document.getElementById('rv');
  var dot  = document.getElementById('dot');
  var sts  = document.getElementById('status');

  /* ---- state ---- */
  var MAX_R   = 90;   /* px, adjusted on resize */
  var active  = false;
  var cx = 0, cy = 0;
  var curL = 0, curR = 0, curBrake = 0;
  var scale = 1;
  var modeScale = 1;
  var ws = null, retryTimer = 0, lastSent = '';

  /* ---- sizing ---- */
  function recalc(){
    var r = base.getBoundingClientRect();
    cx = r.left + r.width / 2;
    cy = r.top  + r.height / 2;
    MAX_R = r.width / 2 - 38;  /* knob half = 38px */
  }
  window.addEventListener('resize', recalc);
  recalc();

  /* One persistent socket; only the latest state is sent. Never queue old moves. */
  function connection(ready){
    dot.className = ready ? 'dot' : 'dot off';
    sts.textContent = ready ? '已连接' : '连接断开';
  }
  function connect(){
    if(document.hidden) return;
    ws = new WebSocket('ws://' + location.hostname + ':81/');
    ws.onopen = function(){ connection(true); lastSent = ''; transmit(true); };
    ws.onclose = function(){ connection(false); ws = null; retryTimer = setTimeout(connect, 500); };
    ws.onerror = function(){ ws.close(); };
  }
  function transmit(force){
    if(!ws || ws.readyState !== WebSocket.OPEN || ws.bufferedAmount) return;
    var frame = curL + ',' + curR + ',' + curBrake;
    if(force || frame !== lastSent || active || curBrake){
      ws.send(frame);
      lastSent = frame;
    }
  }
  function send(l, r, b){
    curL = l; curR = r; curBrake = b;
    transmit(true);
  }
  connect();

  /* ---- joystick math ---- */
  function calc(dx, dy){
    var d = Math.sqrt(dx*dx + dy*dy);
    if(d > MAX_R){ dx=dx/d*MAX_R; dy=dy/d*MAX_R; d=MAX_R; }

    /* dead-zone */
    var fwd  = 0, turn = 0;
    if(d > 8){
      fwd  =  dy / MAX_R;   /* + = forward */
      turn =  dx / MAX_R;   /* + = right   */
    }

    var s = scale * modeScale;
    var l = Math.round((fwd - turn) * 255 * s);
    var r = Math.round((fwd + turn) * 255 * s);
    l = Math.max(-255, Math.min(255, l));
    r = Math.max(-255, Math.min(255, r));
    return {l:l, r:r, dx:dx, dy:dy};
  }

  /* ---- UI update ---- */
  function show(m){
    lv.textContent = m.l;  lv.className = m.l < 0 ? 'val neg' : 'val';
    rv.textContent = m.r;  rv.className = m.r < 0 ? 'val neg' : 'val';
    knob.style.left = 'calc(50% + ' + m.dx + 'px)';
    knob.style.top  = 'calc(50% - ' + m.dy + 'px)';  /* dy positive = up, CSS top positive = down */
  }
  function resetKnob(){
    knob.style.left = '50%'; knob.style.top = '50%';
    lv.textContent='0'; rv.textContent='0';
    lv.className='val'; rv.className='val';
  }

  /* ---- touch / mouse ---- */
  function ptr(e){ return e.touches ? e.touches[0] : e; }

  function onDown(e){
    e.preventDefault(); active = true;
    knob.classList.add('active');
    recalc();
    onMove(e);
  }
  function onMove(e){
    if(!active) return; e.preventDefault();
    var t = ptr(e);
    var m = calc(t.clientX - cx, cy - t.clientY);
    show(m);
    curL=m.l; curR=m.r; curBrake=0;
  }
  function onUp(){
    if(!active) return; active = false;
    knob.classList.remove('active');
    resetKnob();
    curL=0; curR=0;
    send(0, 0, 0);
  }

  base.addEventListener('touchstart',  onDown, {passive:false});
  document.addEventListener('touchmove', onMove, {passive:false});
  document.addEventListener('touchend',  onUp);
  document.addEventListener('touchcancel',onUp);

  base.addEventListener('mousedown', onDown);
  document.addEventListener('mousemove', onMove);
  document.addEventListener('mouseup',   onUp);

  /* ---- brake button ---- */
  var bb = document.getElementById('brakeBtn');
  var brakeHeld = false;
  function brakeOn(e){ 
    e.preventDefault(); 
    active = false;
    resetKnob();
    brakeHeld = true;
    send(0,0,1); 
  }
  function brakeOff(){
    if(!brakeHeld) return;
    brakeHeld = false;
    send(0,0,0); 
  }

  bb.addEventListener('touchstart', brakeOn, {passive:false});
  bb.addEventListener('touchend',   brakeOff);
  bb.addEventListener('touchcancel',brakeOff);
  bb.addEventListener('mousedown',  brakeOn);
  bb.addEventListener('mouseup',    brakeOff);
  bb.addEventListener('mouseleave', brakeOff);

  /* 20 Hz keepalive while driving or braking; latest state only. */
  setInterval(function(){
    transmit(false);
  }, 50);

  document.addEventListener('visibilitychange', function(){
    if(document.hidden){
      active=false; brakeHeld=false; resetKnob(); send(0,0,1);
      clearTimeout(retryTimer);
      if(ws) ws.close();
    } else if(!ws) connect();
  });

  /* ---- speed mode ---- */
  window.setMode = function(s){
    modeScale = s;
    var btns = document.querySelectorAll('.mode-btn');
    for(var i=0;i<btns.length;i++) btns[i].className='mode-btn';
    if(s>=1.4) document.getElementById('mFast').className='mode-btn on';
    else if(s<=0.5) document.getElementById('mSlow').className='mode-btn on';
    else document.getElementById('mNorm').className='mode-btn on';
  };

})();
</script>
</body>
</html>
)rawliteral";

#endif /* WEBPAGE_H */
