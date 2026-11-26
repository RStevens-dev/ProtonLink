// In-memory protocol emulator, never performs network I/O.
// Routes and acknowledgement format based on upstream Attenuator Webrouting.h,
// Webhandler.h and SharedLib/WebRouter/include/WebRouter.h.
import {dispatch,canChangeStream} from './model.js';
import {nextVolumeIntent} from './volume.js';
const modeNames={Proton:'Proton Stream',Stasis:'Stasis Stream',Slime:'Slime Tether',Meson:'Meson Collider',Spectral:'Spectral Stream',Halloween:'Halloween',Christmas:'Christmas',Custom:'Custom Spectral'};
const streamPaths={Proton:'proton',Stasis:'stasis',Slime:'slime',Meson:'meson',Spectral:'spectral',Halloween:'holiday/halloween',Christmas:'holiday/christmas',Custom:'spectral/custom'};
export function status(s){return {mode:s.mode,modeID:s.mode==="Standard"?2:1,vgMode:s.vgMode,ramping:s.ramping,switch:s.ionArm?"Ready":"Standby",wandPower:s.wandPower?"Powered":"Idle",safety:s.barrel?"Safety Off":"Safety On",cyclotronLid:s.lid,theme:s.theme,smoke:s.smoke,vibration:s.vibration,direction:s.direction,pack:s.pack?"Powered":"Idle",power:String(s.level),wand:s.wand?"Connected":"Not Connected",wandMode:modeNames[s.stream]||s.stream,firing:s.firing?"Firing":"Idle",cable:s.cable?"Connected":"Disconnected",cyclotron:s.cyclotron,temperature:s.temperature,musicPlaying:s.playing,musicPaused:s.paused,musicLooping:s.loop,musicShuffled:s.shuffle,musicCurrent:s.track,musicStart:s.first,musicEnd:s.last,volMuted:s.muted,volMaster:s.master,volEffects:s.effects,volMusic:s.music,canChangeStream:canChangeStream(s)}}
export function route(s,a){switch(a.type){
 case "power":return "/pack/"+(a.value?"on":"off");
 case "play":return !s.playing?"/music/startstop":s.paused?"/music/pauseresume":null;
 case "stop":return s.playing?"/music/startstop":null;
 case "pause":return s.playing?"/music/pauseresume":null;
 case "next":case "prev":return "/music/"+a.type;
 case "track":return "/music/select?track="+encodeURIComponent(a.value);
 case "loop":return "/music/loop/"+(s.loop?"all":"one");
 case "shuffle":return "/music/shuffle/"+(s.shuffle?"off":"on");
 case "mute":return "/volume/"+(s.muted?"unmute":"mute");
 case "volume":if(a.value===s[a.channel])return null;return a.channel==="master"?"/volume/master/set/"+Math.max(0,Math.min(100,a.value)):"/volume/"+a.channel+"/"+(a.value>s[a.channel]?"up":"down");
 case "theme":return "/pack/theme/"+a.value;
 case "stream":return streamPaths[a.value]?"/pack/stream/"+streamPaths[a.value]:null;
 case "smoke":return "/pack/smoke/"+(a.value?"on":"off");
 case "vibration":return "/pack/vibration/"+(s.vibration?"off":"on");
 case "direction":return "/pack/cyclotron/"+(s.direction?"counterclockwise":"clockwise");
 case "vent":return "/pack/vent";
 default:return null;
}}
const entry=(kind,direction,summary,body=null)=>({kind,direction,summary,body});
export const push=s=>entry("WS","Attenuator → Watch","/ws · text frame",status(s));
export function sync(s,synced=true){return [entry("HTTP","Watch → Attenuator","GET /status"),...(s.online?[entry("HTTP","Attenuator → Watch","200 OK · /status",synced?status(s):{})]:[entry("FAULT","Local transport","GET /status · simulated timeout")])]}
export function connect(s){return s.online?[...sync(s),entry("WS","Watch → Attenuator","GET /ws · Upgrade: websocket"),entry("WS","Attenuator → Watch","101 Switching Protocols"),push(s)]:sync(s)}
export function exchange(s,a){
 const events=[];
 if(s.locked||!s.online)return {state:s,events:[entry("LOCAL","Watch","Blocked: "+(s.locked?"Field Lock":"offline")+" · no request sent",a)]};
 if(["play","stop","pause"].includes(a.type))events.push(...sync(s));
 if(a.type==="theme"&&(s.pack||s.wandPower||s.ramping))return {state:s,events:[...events,entry("LOCAL","Watch","Theme change blocked while pack powered · no request sent",a)]};
 const path=route(s,a);
 if(!path)return {state:s,events:[...events,entry("LOCAL","Watch","No request required or unsupported intent",a)]};
 const effective=a.type==='volume'?nextVolumeIntent(a.channel,s[a.channel],a.value):a;
 if(!effective)return {state:s,events:[...events,entry('LOCAL','Watch','Volume already at target or no reachable step',a)]};
 const next=dispatch(s,effective);
 if(next===s)return {state:s,events:[...events,entry("LOCAL","Watch","Command rejected by local guard · no request sent",a)]};
 events.push(entry("HTTP","Watch → Attenuator","PUT "+path));
 const serial=serialCommand(s,effective);if(serial)events.push(entry("SERIAL","Attenuator → Pack",serial.name,serial.data??null));
 events.push(entry("HTTP","Attenuator → Watch","200 OK · "+path,{status:"success"}));
 if(a.type==="volume")events.push(entry("SERIAL","Pack → Attenuator","A_VOLUME_SYNC",[next.master,next.effects,next.music]));
 if(JSON.stringify(status(next))!==JSON.stringify(status(s)))events.push(push(next));
 events.push(...sync(next));
 return {state:next,events};
}

 // Symbolic serial messages, not captured wire bytes.
 export function serialCommand(s,a){
 const names={power:a.value?"A_TURN_PACK_ON":"A_TURN_PACK_OFF",play:s.paused?"A_MUSIC_PAUSE_RESUME":"A_MUSIC_START_STOP",stop:"A_MUSIC_START_STOP",pause:"A_MUSIC_PAUSE_RESUME",next:"A_MUSIC_NEXT_TRACK",prev:"A_MUSIC_PREV_TRACK",track:"A_MUSIC_PLAY_TRACK",loop:"A_MUSIC_TRACK_LOOP_TOGGLE",shuffle:"A_MUSIC_TRACK_SHUFFLE_TOGGLE",mute:"A_TOGGLE_MUTE",direction:"A_CYCLOTRON_DIRECTION_TOGGLE",vent:"A_MANUAL_OVERHEAT",stream:"A_SET_STREAM_MODE",theme:({1984:"A_YEAR_1984",1989:"A_YEAR_1989",2021:"A_YEAR_AFTERLIFE",2024:"A_YEAR_FROZEN_EMPIRE"})[a.value]};
 if(a.type==="volume"){if(a.channel==="master")return {name:"A_VOLUME_SET",data:a.value};return {name:"A_VOLUME_"+(a.channel==="effects"?"SOUND_EFFECTS":"MUSIC")+"_"+(a.value>s[a.channel]?"INCREASE":"DECREASE")}}
 if(!names[a.type])return null;
 const data=a.type==="track"?a.value:a.type==="loop"?(s.loop?1:2):a.type==="shuffle"?(s.shuffle?1:2):a.type==="mute"?(s.muted?1:2):a.type==="direction"?(s.direction?1:2):undefined;
 return {name:names[a.type],data};
 }
