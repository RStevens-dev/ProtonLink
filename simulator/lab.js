// Browser-only transport laboratory. No fetch, WebSocket or hardware connection.
import {status,exchange,route} from './protocol.js';
export const profiles={legacy:{label:"Gen 1 conservative",streams:["Proton","Stasis","Slime","Meson"],openapi:false},mock:{label:"Upstream mock capability set",streams:["Proton","Stasis","Slime","Meson","Spectral"],openapi:true},extended:{label:"All documented route candidates",streams:["Proton","Stasis","Slime","Meson","Spectral","Halloween","Christmas","Custom"],openapi:true}};
export const defaults=()=>({latency:0,loss:0,gateway:true,gatewayPower:true,fault:"none",profile:"mock"});
export function diff(a,b){return Object.keys({...a,...b}).filter(k=>JSON.stringify(a[k])!==JSON.stringify(b[k])).map(k=>({field:k,watch:a[k],pack:b[k]}))}
const event=(kind,direction,summary,body=null)=>({kind,direction,summary,body});
export function simulate(s,a,opts={},random=Math.random){
 const cfg={...defaults(),...opts},p=profiles[cfg.profile]||profiles.mock;
 if(a.type==="stream"&&!p.streams.includes(a.value))return {state:s,events:[event("LOCAL","Watch","Unsupported by selected simulated profile",a)],delivered:false};
 if(!cfg.gatewayPower||!cfg.gateway)return {state:s,events:[event("LOCAL","Watch","Equipment status unavailable; write blocked")],delivered:false};
 const result=exchange(s,a);
 if(!result.events.some(e=>e.summary.startsWith("PUT ")))return {...result,delivered:true};
 const path=route(s,a),request=event("HTTP","Watch → Attenuator","PUT "+path);
 if(!cfg.gateway)return {state:s,events:[request,event("FAULT","Attenuator","Simulated equipment link unavailable; no completion assumed")],delivered:false};
 if(cfg.fault==="request"||random()*100<cfg.loss)return {state:s,events:[request,event("FAULT","Transport","Simulated request dropped")],delivered:false};
 if(cfg.fault==="409")return {state:s,events:[request,event("HTTP","Attenuator → Watch","409 Conflict · injected", {status:"Injected conflict"})],delivered:true};
 if(cfg.fault==="response")return {state:result.state,events:[request,event("FAULT","Transport","Response and telemetry dropped; pack may have changed")],delivered:false};
 if(cfg.fault==="malformed")return {state:result.state,events:[request,event("FAULT","Transport","Malformed response and telemetry", "{invalid json")],delivered:false};
 return {...result,delivered:true};
}
export const configPreview=()=>({prefsAvailable:true,ledCycLidHue:8,ledCycLidSat:254,ledCycLidLum:100,ledPowercellHue:8,ledPowercellSat:254,ledPowercellLum:100});
export function readPreview(path,s,cfg){const p=profiles[cfg.profile]||profiles.mock;if(path==="/status")return cfg.gateway?status(s):{};if(path==="/config/device")return {...(cfg.profile==="legacy"?{}:{songList:"Track 1\nTrack 2\nTrack 3\nGhostbusters Theme"}),streamModes:p.streams.map((label,value)=>({label,value}))};if(path==="/config/pack")return cfg.gateway?{...configPreview(),esp32Pack:false,packPowered:s.pack||s.ramping,wandPowered:s.wandPower}:{prefsAvailable:false};if(path==="/openapi.json"&&p.openapi)return {openapi:"3.0.0",info:{title:"ProtonLink simulated subset",version:"0.1"},paths:{"/status":{get:{}}, "/pack/on":{put:{}}, "/music/startstop":{put:{}}}};return null}
export const routes=[["GET","/status"],["GET","/config/device"],["GET","/config/pack"],["GET","/openapi.json"],["PUT","/pack/on"],["PUT","/pack/off"],["PUT","/music/startstop"],["PUT","/music/pauseresume"],["PUT","/music/next"],["PUT","/music/prev"],["PUT","/music/select?track=501"],["PUT","/volume/master/set/75"],["PUT","/volume/effects/up"],["PUT","/volume/music/down"],["PUT","/pack/stream/proton"],["PUT","/pack/theme/1984"],["PUT","/pack/vent"],["PUT","/pack/smoke/on"],["POST","/config/pack/save (reference only)"]];
export function intentFor(path,s){const fixed={"/pack/on":{type:"power",value:true},"/pack/off":{type:"power",value:false},"/music/startstop":{type:s.playing?"stop":"play"},"/music/pauseresume":{type:"pause"},"/music/next":{type:"next"},"/music/prev":{type:"prev"},"/music/select?track=501":{type:"track",value:501},"/volume/master/set/75":{type:"volume",channel:"master",value:75},"/volume/effects/up":{type:"volume",channel:"effects",value:s.effects+5},"/volume/music/down":{type:"volume",channel:"music",value:s.music-5},"/pack/stream/proton":{type:"stream",value:"Proton"},"/pack/theme/1984":{type:"theme",value:"1984"},"/pack/vent":{type:"vent"},"/pack/smoke/on":{type:"smoke",value:true}};return fixed[path]||null}
