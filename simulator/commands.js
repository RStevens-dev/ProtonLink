import {route} from './protocol.js';
const explicit={mute:["/volume/unmute","/volume/mute"],loop:["/music/loop/all","/music/loop/one"],shuffle:["/music/shuffle/off","/music/shuffle/on"],smoke:["/pack/smoke/off","/pack/smoke/on"],vibration:["/pack/vibration/off","/pack/vibration/on"],direction:["/pack/cyclotron/counterclockwise","/pack/cyclotron/clockwise"],cable:["/pack/cable/off","/pack/cable/on"]};
export function commandPath(s,a){
 if(explicit[a.type]){if(typeof a.value!=="boolean")throw Error("Choose an explicit setting");return explicit[a.type][+a.value]}
 if(["attenuate","lockout","restart"].includes(a.type))return "/pack/"+a.type;
 if(a.type==="track"&&(!Number.isInteger(a.value)||a.value<s.first||a.value>s.last))throw Error("Track outside equipment range");
 if(a.type==="power"&&s.pack===a.value)return null;
 return route(s,a);
}
export async function sendCommand(path,request=(...args)=>globalThis.fetch(...args)){const c=new AbortController(),t=setTimeout(()=>c.abort(),5000);try{const r=await request("/bridge"+path,{method:"PUT",headers:{"X-ProtonLink-Command":"1"},signal:c.signal});const x=await r.json();if(!r.ok)throw Error("HTTP "+r.status+" · "+(x.error||x.status||"Rejected"));if(x.status!=="success"&&x.success!==true)throw Error("Unrecognized acknowledgement");return x}finally{clearTimeout(t)}}
export function commandedField(a){return ({mute:"muted",loop:"loop",shuffle:"shuffle",smoke:"smoke",vibration:"vibration",direction:"direction",cable:"cable"})[a.type]||null}
