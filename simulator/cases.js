import {initial,dispatch} from './model.js';
import {exchange,route,connect,status} from './protocol.js';
import {simulate,diff,defaults,readPreview} from './lab.js';
import {volumeTarget,nextVolumeIntent,queueVolume,dueVolumes} from './volume.js';
const open=()=>dispatch(initial(),{type:'unlock'});
const check=(condition,message)=>{if(!condition)throw new Error(message)};
export const cases=[
 ['Locked watch blocks writes',()=>{let s=initial();for(const type of ['play','stop','power','smoke','volume','theme','vent'])check(dispatch(s,{type,value:true,confirmed:true,channel:'master'})===s,type+' bypassed lock')}],
 ['Pack physical controls work while watch locked',()=>{let s=dispatch(initial(),{type:'physical',control:'power',value:true});check(s.pack&&s.locked,'physical power or lock incorrect')}],
 ['Music start and stop update pack state',()=>{let s=dispatch(open(),{type:'play'});check(s.playing,'did not start');s=dispatch(s,{type:'stop'});check(!s.playing&&!s.paused,'did not stop')}],
 ['Repeated start is idempotent',()=>{let s=dispatch(open(),{type:'play'});check(JSON.stringify(dispatch(s,{type:'play'}))===JSON.stringify(s),'duplicate start changed state')}],
 ['Pause and resume',()=>{let s=dispatch(open(),{type:'play'});s=dispatch(s,{type:'pause'});check(s.paused,'not paused');check(!dispatch(s,{type:'pause'}).paused,'not resumed')}],
 ['Track IDs wrap at boundaries',()=>{let s={...open(),track:520};check(dispatch(s,{type:'next'}).track===500,'next boundary');check(dispatch({...s,track:500},{type:'prev'}).track===520,'previous boundary');check(dispatch(s,{type:'track',value:4})===s,'accepted invalid raw ID')}],
 ['Volume stays between 0 and 100',()=>{let s=open();check(dispatch(s,{type:'volume',channel:'master',value:120}).master===100,'upper bound');check(dispatch(s,{type:'volume',channel:'music',value:-5}).music===0,'lower bound')}],
 ['Power needs confirmation',()=>{let s=open();check(dispatch(s,{type:'power',value:true})===s,'unconfirmed power');check(dispatch(s,{type:'power',value:true,confirmed:true}).pack,'confirmed power')}],
 ['Critical warning blocks vent and stream change',()=>{let s=dispatch(open(),{type:'scenario',value:'critical'});check(dispatch(s,{type:'vent',confirmed:true})===s,'vent allowed');check(dispatch(s,{type:'stream',value:'Slime'})===s,'stream allowed')}],
 ['Normal powered pack accepts confirmed vent',()=>{let s=dispatch(open(),{type:'scenario',value:'active'});check(dispatch(s,{type:'vent',confirmed:true}).temperature==='Venting','vent failed')}],
 ['Disconnect locks watch and rejects commands',()=>{let s=dispatch(open(),{type:'scenario',value:'offline'});check(s.locked&&!s.online,'not locked');check(dispatch(s,{type:'play'})===s,'offline write');check(dispatch(s,{type:'unlock'})===s,'offline unlock')}],
 ['Physical changes persist through reconnect',()=>{let s=dispatch(open(),{type:'scenario',value:'offline'});s=dispatch(s,{type:'physical',control:'music',value:true});s=dispatch(s,{type:'scenario',value:'active'});check(s.online&&s.locked&&s.playing,'reconnect mismatch')}],
 ['Lock preserves playback',()=>{let s=dispatch(open(),{type:'play'});check(dispatch(s,{type:'lock'}).playing,'lock stopped music')}],
 ['Physical wand level is reflected in shared state',()=>{let s=dispatch(initial(),{type:'physical',control:'level',value:5});check(s.level===5&&s.locked,'level mismatch')}],
 ['Firing requires connected wand and powered pack',()=>{let s=initial();check(dispatch(s,{type:'physical',control:'firing',value:true})===s,'firing in standby');s=dispatch(s,{type:'physical',control:'power',value:true});check(dispatch(s,{type:'physical',control:'firing',value:true}).firing,'firing denied')}]
];
cases.push(
 ['Protocol uses documented music toggle and acknowledgement',()=>{const x=exchange(open(),{type:"play"});check(x.events.some(e=>e.summary==="PUT /music/startstop"),"missing route");check(x.events.some(e=>e.body?.status==="success"),"missing ack");check(x.events.some(e=>e.kind==="WS"&&e.body.musicPlaying===true),"missing telemetry")}],
 ['Protocol does not repeat music toggle',()=>{const s=exchange(open(),{type:"play"}).state;check(!exchange(s,{type:"play"}).events.some(e=>e.summary.startsWith("PUT")),"duplicate write")}],
 ['Protocol uses raw track ID',()=>{check(route(open(),{type:"track",value:501})==="/music/select?track=501","wrong ID")}],
 ['Protocol distinguishes incremental mixer routes',()=>{check(route(open(),{type:"volume",channel:"effects",value:85})==="/volume/effects/up","wrong effects route");check(route(open(),{type:"volume",channel:"master",value:85})==="/volume/master/set/85","wrong master route")}],
 ['Locked protocol sends no HTTP requests',()=>{check(exchange(initial(),{type:"play"}).events.every(e=>e.kind==="LOCAL"),"traffic while locked")}],
 ['Offline protocol does not invent response',()=>{const s={...initial(),online:false};check(!connect(s).some(e=>e.summary.includes("200 OK")||e.kind==="WS"),"false reply")}],
 ['Theme changes blocked when powered',()=>{const s={...open(),pack:true};check(exchange(s,{type:"theme",value:"1989"}).state===s,"theme changed")}]
);
cases.push(
 ["Dropped response can leave authoritative state changed",()=>{let s=open();let x=simulate(s,{type:"play"},{fault:"response"},()=>.5);check(x.state.playing&&!x.delivered,"missing divergence");check(diff(status(s),status(x.state)).some(d=>d.field==="musicPlaying"),"missing diff")}],
 ["Dropped request does not mutate pack",()=>{let s=open();check(simulate(s,{type:"play"},{loss:100},()=>.5).state===s,"request mutated pack")}],
 ["Injected conflict does not mutate pack",()=>{let s=open();check(simulate(s,{type:"play"},{fault:"409"},()=>.5).state===s,"conflict mutated pack")}],
 ["Profile filters unavailable stream",()=>{let s=open();check(simulate(s,{type:"stream",value:"Halloween"},{profile:"legacy"}).state===s,"unsupported stream")}],
 ["Legacy profile lacks OpenAPI",()=>{check(readPreview("/openapi.json",initial(),{profile:"legacy"})===null,"legacy exposed discovery")}],
 ["Disconnected gateway blocks command",()=>{let s=open();check(simulate(s,{type:"play"},{gateway:false}).state===s,"gateway offline write")}]
);
cases.push(
 ["Volume debounce coalesces repeated adjustments",()=>{let q=queueVolume({},"master",80,0);q=queueVolume(q,"master",90,400);check(!dueVolumes(q,600).length,"early send");check(dueVolumes(q,1000)[0]==="master"&&q.master.target===90,"latest target lost")}],
 ["Active drag suppresses send",()=>{const q=queueVolume({},"effects",30,0);check(!dueVolumes(q,1000,["effects"]).length,"sent while dragging")}],
 ["Volume channels debounce independently",()=>{let q=queueVolume({},"master",30,0);q=queueVolume(q,"music",50,500);check(dueVolumes(q,600).join()==="master","incorrect due channels")}],
 ["Master rounds to supported 5 percent steps",()=>{check(volumeTarget(83)===85&&volumeTarget(102)===100&&volumeTarget(-5)===0,"invalid target")}],
 ["Master sends one absolute write",()=>{const a=nextVolumeIntent("master",75,20);const x=exchange(open(),a);check(x.state.master===20&&x.events.filter(e=>e.summary.startsWith("PUT ")).length===1,"incorrect master write")}],
 ["Effects only advance one step per request",()=>{const x=exchange(open(),{type:"volume",channel:"effects",value:20});check(x.state.effects===75&&x.events.some(e=>e.summary==="PUT /volume/effects/down"),"unrealistic effects jump")}],
 ["Music reaches final target with minimum incremental writes",()=>{let s=open(),count=0;while(s.music!==90&&count<10){const a=nextVolumeIntent("music",s.music,90);s=exchange(s,a).state;count++}check(s.music===90&&count===4,"incorrect incremental count")}],
 ["Returning to same volume needs no write",()=>{check(nextVolumeIntent("master",75,75)===null&&nextVolumeIntent("effects",80,80)===null,"redundant write")}],
 ["Unaligned volume target cannot oscillate",()=>{check(nextVolumeIntent("music",72,70)===null,"overshoot")}]
);
export function runCases(){return cases.map(([name,fn])=>{try{fn();return {name,pass:true}}catch(e){return {name,pass:false,error:e.message}}})}
