#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Servo pins - pin 13 shocks player 1, pin 12 shocks player 2
#define SERVO1_PIN 14
#define SERVO2_PIN 27

// How long to collect gesture votes after countdown
#define GAME_WINDOW 1500

// How long to wait before opening the gesture window (matches countdown)
#define COUNTDOWN_MS 4000

Servo servo1;
Servo servo2;
WebServer server(80);       // serves the website
WebSocketsServer webSocket(81); // live connection to phone

// Game state
bool gameStarted = false;
bool glove1Connected = false;
bool glove2Connected = false;
unsigned long gameStartTime = 0;

bool shockActive = false;
unsigned long shockStartTime = 0;
int shockServo = 0;

// Buffers to collect gesture votes from each glove
String votes1[20];
String votes2[20];
int voteCount1 = 0;
int voteCount2 = 0;

// Data packet received from gloves
typedef struct {
  int gloveId;
  char gesture[10];
} GloveData;

// Pick the most common gesture from all readings
String majorityVote(String* votes, int count) {
  if (count == 0) return "";
  int r = 0, p = 0, s = 0;
  for (int i = 0; i < count; i++) {
    if (votes[i] == "rock")          r++;
    else if (votes[i] == "paper")    p++;
    else if (votes[i] == "scissors") s++;
  }
  if (r == 0 && p == 0 && s == 0) return ""; // all readings were unknown, cancel
  if (r >= p && r >= s) return "rock";
  if (p >= r && p >= s) return "paper";
  return "scissors";
}

// Standard rock paper scissors rules
String determineWinner(String g1, String g2) {
  if (g1 == g2) return "draw";
  if ((g1 == "rock"     && g2 == "scissors") ||
      (g1 == "scissors" && g2 == "paper")    ||
      (g1 == "paper"    && g2 == "rock")) return "1";
  return "2";
}

// Rotate servo to press shock button then release
void triggerShock(int loser) {
  Servo& s = (loser == 1) ? servo1 : servo2;
  s.write(180);
  shockActive = true;
  shockStartTime = millis();
  shockServo = loser;
}

// Send a JSON message to the phone over WebSocket
void sendState(String state, String p1 = "", String p2 = "", String winner = "", String loser = "", int g1 = -1, int g2 = -1) {
  StaticJsonDocument<200> doc;
  doc["state"] = state;
  if (p1 != "")     doc["player1"] = p1;
  if (p2 != "")     doc["player2"] = p2;
  if (winner != "") doc["winner"]  = winner;
  if (loser != "")  doc["loser"]   = loser;
  if (g1 != -1)     doc["glove1"]  = (bool)g1;
  if (g2 != -1)     doc["glove2"]  = (bool)g2;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

// Reset everything back to waiting state
void resetGame() {
  gameStarted = false;
  voteCount1 = 0;
  voteCount2 = 0;
  gameStartTime = 0;
}

// Tell phone which gloves are connected
void broadcastGloveStatus() {
  sendState("ready", "", "", "", "", glove1Connected, glove2Connected);
}

// Called every time a glove sends a gesture packet
void onDataReceived(const esp_now_recv_info *info, const uint8_t *data, int len) {
  GloveData *d = (GloveData *)data;
  String gesture = String(d->gesture);

  // Mark glove as connected on first packet received
  if (d->gloveId == 1 && !glove1Connected) {
    glove1Connected = true;
    broadcastGloveStatus();
    Serial.println("Glove 1 connected");
  } else if (d->gloveId == 2 && !glove2Connected) {
    glove2Connected = true;
    broadcastGloveStatus();
    Serial.println("Glove 2 connected");
  }

  // Only collect votes when game window is open
  if (!gameStarted) return;

  unsigned long elapsed = millis() - gameStartTime;
  if (elapsed < GAME_WINDOW) {
    if (d->gloveId == 1 && voteCount1 < 20) {
      votes1[voteCount1++] = gesture;
    } else if (d->gloveId == 2 && voteCount2 < 20) {
      votes2[voteCount2++] = gesture;
    }
  }
}

// Called when phone sends a message over WebSocket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  // When phone connects, send current glove status
  if (type == WStype_CONNECTED) {
    broadcastGloveStatus();
  }
  if (type == WStype_TEXT) {
    String msg = String((char *)payload);

    // Phone pressed FIGHT
    if (msg.indexOf("start") >= 0 && !gameStarted) {
      resetGame();
      sendState("countdown");
      gameStartTime = millis() + COUNTDOWN_MS; // open window after countdown
      gameStarted = true;
      Serial.println("Game started");
    }

    // Phone pressed CANCEL
    if (msg.indexOf("cancel") >= 0) {
      resetGame();
      sendState("cancel");
      Serial.println("Game cancelled");
    }
  }
}

// Website HTML stored in flash memory to save RAM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
  <html>
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>VOLT CLASH ⚡</title>
  <style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background:#060a1a;
    color:#fff;
    font-family:'Arial Black',Impact,sans-serif;
    min-height:100vh;
    display:flex;
    align-items:center;
    justify-content:center;
    text-align:center;
  }
  .screen { display:none; padding:20px; width:100%; max-width:420px; }
  .screen.active { display:block; }
  h1 { font-size:clamp(1.7rem,7vw,3rem); color:#7df9ff; letter-spacing:3px; margin-bottom:2px; }
  .fool { font-size:.55rem; letter-spacing:4px; color:#f0e040; margin-bottom:2px; }
  .sub { font-size:.6rem; letter-spacing:4px; color:#a78bfa; margin-bottom:22px; }
  .gloves { display:flex; gap:10px; justify-content:center; margin-bottom:14px; }
  .glove { flex:1; max-width:140px; background:#0d1230; border:2px solid #1e2a4a; border-radius:6px; padding:10px 6px; transition:border-color .3s; }
  .glove.on { border-color:#4dff91; box-shadow:0 0 10px #4dff9144; }
  .glove.off { border-color:#ff3c3c; }
  .glove .glabel { font-size:.5rem; letter-spacing:3px; color:#888; margin-bottom:4px; }
  .glove .gind { font-size:1.3rem; }
  .ws-status { font-size:.6rem; letter-spacing:3px; color:#888; margin-bottom:14px; }
  .ws-status.ok,#conn.ok { color:#7df9ff; }
  .ws-status.err,#conn.err { color:#ff3c3c; }
  .score { font-size:.7rem; letter-spacing:3px; color:#fff; margin-bottom:20px; }
  .score .s1 { color:#7df9ff; }
  .score .s2 { color:#a78bfa; }
  button {
    padding:14px 40px;
    font-family:inherit;
    font-size:1.1rem;
    letter-spacing:2px;
    background:#7df9ff;
    color:#000;
    border:none;
    border-radius:4px;
    cursor:pointer;
  }
  button:disabled { opacity:.3; cursor:not-allowed; }
  button.sec { background:#ff3c3c; color:#fff; font-size:.85rem; padding:10px 28px; margin-top:28px; }
  button.purple { background:#a78bfa; color:#000; font-size:1rem; padding:12px 34px; }
  .word { font-size:clamp(2.4rem,11vw,5rem); letter-spacing:3px; color:#7df9ff; margin-bottom:8px; }
  .word.waiting { font-size:clamp(1rem,4vw,1.6rem); color:#a78bfa; letter-spacing:4px; }
  .cdemo { font-size:clamp(3rem,14vw,5.5rem); margin-bottom:28px; }
  .players { display:flex; gap:10px; justify-content:center; margin:16px 0; }
  .card { flex:1; background:#0d1230; border:2px solid #1e2a4a; border-radius:8px; padding:12px 6px; }
  .card.winner { border-color:#f0e040; }
  .card.loser { border-color:#ff3c3c; }
  .card .clabel { font-size:.5rem; letter-spacing:4px; color:#888; margin-bottom:4px; }
  .card .cemo { font-size:clamp(2.5rem,11vw,4.5rem); }
  .card .cname { font-size:.6rem; letter-spacing:3px; color:#556; margin-top:4px; text-transform:uppercase; }
  .result-text { font-size:clamp(1.2rem,5.5vw,2rem); letter-spacing:2px; margin-bottom:2px; }
  .meme-text { font-size:.58rem; letter-spacing:2px; color:#a78bfa; margin-bottom:10px; }
  .result-text.p1 { color:#7df9ff; }
  .result-text.p2 { color:#a78bfa; }
  .result-text.tie { color:#aaa; }
  @keyframes zap {
    0%,100%{transform:translateX(0) scale(1);}
    10%{transform:translateX(-10px) scale(1.06) rotate(-2deg);}
    25%{transform:translateX(10px) scale(1.06) rotate(2deg);}
    40%{transform:translateX(-6px);}
    60%{transform:translateX(6px);}
    80%{transform:translateX(-3px);}
  }
  .card.shocking { animation:zap .4s ease 3; border-color:#7df9ff; box-shadow:0 0 30px #7df9ff; }
  #conn { position:fixed; bottom:10px; right:10px; font-size:.5rem; letter-spacing:2px; color:#333; }
  </style>
  </head>
  <body>

  <div id="s-connect" class="screen active">
    <div class="fool">SILLYHACK2026</div>
    <h1>VOLT CLASH</h1>
    <div style="font-size:3rem;margin-bottom:4px">🧙⚡</div>
    <div class="sub">ZAP · OR · BE · ZAPPED</div>
    <div class="gloves">
      <div class="glove" id="g1"><div class="glabel">WIZARD 1</div><div class="gind" id="gi1">⏳</div></div>
      <div class="glove" id="g2"><div class="glabel">WIZARD 2</div><div class="gind" id="gi2">⏳</div></div>
    </div>
    <div class="ws-status" id="ws-status">Connecting to Arena...</div>
    <div class="score">W1: <span class="s1" id="sc1">0</span> | W2: <span class="s2" id="sc2">0</span></div>
    <button id="btn-start" disabled>CLASH!</button>
  </div>

  <div id="s-countdown" class="screen">
    <div class="word" id="cd-word"></div>
    <div class="cdemo" id="cd-emoji"></div>
    <button class="sec" id="btn-cancel">RETREAT</button>
  </div>

  <div id="s-result" class="screen">
    <div class="score" style="margin-bottom:10px">W1: <span class="s1" id="sr1">0</span> | W2: <span class="s2" id="sr2">0</span></div>
    <div class="players">
      <div class="card" id="card1">
        <div class="clabel">WIZARD 1</div>
        <div class="cemo" id="emo1">✊</div>
        <div class="cname" id="name1">rock</div>
      </div>
      <div class="card" id="card2">
        <div class="clabel">WIZARD 2</div>
        <div class="cemo" id="emo2">✊</div>
        <div class="cname" id="name2">rock</div>
      </div>
    </div>
    <div class="result-text" id="result-text"></div>
    <div class="meme-text" id="meme-text"></div>
    <button class="purple" id="btn-again">REMATCH</button>
  </div>

  <div id="conn">● disconnected</div>

  <script>
  const WS_URL='ws://192.168.4.1:81';
  const EMOJI={rock:'✊',paper:'✋',scissors:'✌️ '};
  const STEPS=[
    {w:'ROCK...',e:'✊'},
    {w:'PAPER...',e:'✋'},
    {w:'SCISSORS...',e:'✌️ '},
    {w:'ZAAAAP!!!',e:'⚡'}
  ];
  const WIN_M=['HE PROTECC HE ATACC','ELECTRO WIZARD FR','GOAT OF THE ARENA','UNTOUCHABLE LEGEND'];
  const LOSE_M=['GET ZAPPED 💀','SKILL ISSUE BRO','RATIO+SHOCKED','COPE+SEETHE'];

  let ws=null,timer=null,scores={1:0,2:0},gloves={1:false,2:false},wsOk=false,actx=null;

  const $=id=>document.getElementById(id);
  const screens={connect:$('s-connect'),countdown:$('s-countdown'),result:$('s-result')};

  function show(name){
    Object.values(screens).forEach(s=>s.classList.remove('active'));
    screens[name].classList.add('active');
  }

  function shock(id){
    const c=$(id);
    c.classList.remove('shocking');
    void c.offsetWidth;
    c.classList.add('shocking');
  }

  function beep(freq,dur,gain){
    try{
      if(!actx)actx=new(window.AudioContext||window.webkitAudioContext)();
      const o=actx.createOscillator(),g=actx.createGain(),t=actx.currentTime;
      o.connect(g);g.connect(actx.destination);
      o.type='square';o.frequency.value=freq;
      g.gain.setValueAtTime(gain,t);
      g.gain.exponentialRampToValueAtTime(0.001,t+dur);
      o.start(t);o.stop(t+dur);
    }catch(e){}
  }

  function rnd(a){return a[Math.floor(Math.random()*a.length)];}
  function updateScores(){
    $('sc1').textContent=scores[1];$('sc2').textContent=scores[2];
    $('sr1').textContent=scores[1];$('sr2').textContent=scores[2];
  }
  function updateGloves(){
    [1,2].forEach(i=>{
      const on=gloves[i];
      $('g'+i).className='glove'+(on?' on':' off');
      $('gi'+i).textContent=on?'⚡':'⏳';
    });
    updateFightBtn();
  }

  function updateFightBtn(){
    $('btn-start').disabled=!(wsOk&&gloves[1]&&gloves[2]);
  }

  function setStatus(state,text){
    const ok=state==='ok',er=state==='err';
    const s=$('ws-status'),p=$('conn');
    s.textContent=text;s.className='ws-status'+(ok?' ok':er?' err':'');
    p.textContent=ok?'● on':er?'● err':'● off';p.className=ok?'ok':er?'err':'';
    wsOk=ok;updateFightBtn();
  }

  function connect(){
    if(ws)try{ws.close();}catch(e){}
    ws=new WebSocket(WS_URL);
    ws.onopen=()=>setStatus('ok','Online ⚡');
    ws.onclose=()=>{setStatus('err','Disconnected');setTimeout(connect,3000);};
    ws.onerror=()=>setStatus('err','No arena');
    ws.onmessage=e=>{try{handle(JSON.parse(e.data));}catch(_){}};
  }

  function handle(msg){
    if(msg.state==='ready'){
      if(msg.glove1!==undefined)gloves[1]=!!msg.glove1;
      if(msg.glove2!==undefined)gloves[2]=!!msg.glove2;
      updateGloves();
    }else if(msg.state==='countdown'){
      startCountdown();
    }else if(msg.state==='result'){
      showResult(msg);
    }else if(msg.state==='shock'){
      shock('card'+msg.loser);
    }else if(msg.state==='cancel'){
      clearTimeout(timer);show('connect');
    }
  }

  function send(obj){if(ws&&ws.readyState===WebSocket.OPEN)ws.send(JSON.stringify(obj));}

  $('btn-start').addEventListener('click',()=>{send({action:'start'});startCountdown();});
  $('btn-cancel').addEventListener('click',()=>{clearTimeout(timer);send({action:'cancel'});show('connect');});
  $('btn-again').addEventListener('click',()=>show('connect'));

  function startCountdown(){clearTimeout(timer);show('countdown');step(0);}

  function step(i){
    if(i>=STEPS.length){
      const w=$('cd-word');
      w.textContent='AWAITING RESULT...';
      w.className='word waiting';
      $('cd-emoji').textContent='⌛';
      return;
    }
    const s=STEPS[i];
    $('cd-word').className='word';
    $('cd-word').textContent=s.w;
    $('cd-emoji').textContent=s.e;
    if(s.w==='ZAAAAP!!!'){beep(1200,0.3,0.5);beep(600,0.7,0.6);}
    else{beep(440,0.12,0.25);}
    timer=setTimeout(()=>step(i+1),s.w==='ZAAAAP!!!'?1200:800);
  }

  function showResult(msg){
    clearTimeout(timer);
    const p1=(msg.player1||'').toLowerCase();
    const p2=(msg.player2||'').toLowerCase();
    $('emo1').textContent=EMOJI[p1]||'❓';
    $('emo2').textContent=EMOJI[p2]||'❓';
    $('name1').textContent=p1||'?';
    $('name2').textContent=p2||'?';
    $('card1').className=$('card2').className='card';
    const rt=$('result-text'),mt=$('meme-text');
    if(msg.winner==='1'){
      $('card1').classList.add('winner');$('card2').classList.add('loser');
      rt.textContent='WIZARD 1 WINS!';rt.className='result-text p1';
      mt.textContent='W1: '+rnd(WIN_M)+' | W2: '+rnd(LOSE_M);
      scores[1]++;
    }else if(msg.winner==='2'){
      $('card2').classList.add('winner');$('card1').classList.add('loser');
      rt.textContent='WIZARD 2 WINS!';rt.className='result-text p2';
      mt.textContent='W1: '+rnd(LOSE_M)+' | W2: '+rnd(WIN_M);
      scores[2]++;
    }else{
      rt.textContent='DOUBLE ZAP!';rt.className='result-text tie';
      mt.textContent='MUTUAL DESTRUCTION. CLOWNS.';
      shock('card1');shock('card2');
    }
    updateScores();
    show('result');
  }

  connect();
  </script>
  </body>
  </html>

)rawliteral";

void setup() {
  Serial.begin(115200);

  // Servos start at 0 (released position)
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(0);
  servo2.write(0);

  // Start WiFi hotspot
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ShockGlove", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Start ESP-NOW to receive from gloves
  esp_now_init();
  esp_now_register_recv_cb(onDataReceived);

  // Serve website when phone opens 192.168.4.1
  server.on("/", []() {
    server.send_P(200, "text/html", index_html);
  });
  server.begin();

  // Start WebSocket for live game updates
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Server ready!");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (shockActive && millis() - shockStartTime >= 300) {
    if (shockServo == 1) servo1.write(0);
    else servo2.write(0);
    shockActive = false;
  }

  if (gameStarted) {
    unsigned long now = millis();
    unsigned long windowEnd = gameStartTime + GAME_WINDOW;

    if (now > windowEnd) {
      String g1 = majorityVote(votes1, voteCount1);
      String g2 = majorityVote(votes2, voteCount2);

      if (g1 == "" || g2 == "") {
        sendState("cancel");
        Serial.println("Timeout — glove missing");
      } else {
        String winner = determineWinner(g1, g2);
        sendState("result", g1, g2, winner);
        if (winner != "draw") {
          String loser = (winner == "1") ? "2" : "1";
          sendState("shock", "", "", "", loser);
          triggerShock(loser.toInt());
        }
      }
      resetGame();
    }
  }
}