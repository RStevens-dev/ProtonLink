import {route} from './protocol.js';
import {canChangeStream, streams, themes} from './model.js';
const explicit={mute:["/volume/unmute","/volume/mute"],loop:["/music/loop/all","/music/loop/one"],shuffle:["/music/shuffle/off","/music/shuffle/on"],smoke:["/pack/smoke/off","/pack/smoke/on"],vibration:["/pack/vibration/off","/pack/vibration/on"],direction:["/pack/cyclotron/counterclockwise","/pack/cyclotron/clockwise"],cable:["/pack/cable/off","/pack/cable/on"]};
export function commandPath(s,a){
 const normal = ["Normal","Active"].includes(s.cyclotron) && s.firing === false && s.temperature === "Normal";
 if (["power","vent"].includes(a.type) || (a.type === "smoke" && a.value)) {
   if (a.confirmed !== true) throw Error("Hold to confirm this command");
 }
 if (a.type === "power") {
   if (typeof a.value !== "boolean") throw Error("Choose power on or off");
   if (!normal) throw Error("Power blocked by firing, temperature or cyclotron state");
   // Legacy Super Hero firmware checks shutdown in its on/off handlers.
   const legacyPower = s.mode === "Super Hero" && s.ramping === null;
   if (s.ramping !== false && !legacyPower) throw Error("Power transition active or unknown");
 }
 if (a.type === "vent" && (!normal || !s.pack)) throw Error("Vent unavailable");
 if (a.type === "smoke" && a.value && !normal) throw Error("Smoke unavailable");
 if (a.type === "theme" && (!themes.includes(a.value) || !normal || s.pack || s.wandPower || s.ramping !== false)) throw Error("Theme change unavailable");
 if (a.type === "stream" && (!streams.includes(a.value) || s.ramping !== false || !canChangeStream(s))) throw Error("Stream change unavailable");
 if (a.type === "volume" && (!["master","music","effects"].includes(a.channel) || !Number.isInteger(a.value) || a.value < 0 || a.value > 100)) throw Error("Invalid volume");
 if (a.type === "track" && (!Number.isInteger(s.first) || !Number.isInteger(s.last) || s.first < 500 || s.last > 4595)) throw Error("Unknown track range");
 if(Object.hasOwn(explicit,a.type)){if(typeof a.value!=="boolean")throw Error("Choose an explicit setting");return explicit[a.type][+a.value]}
 if(["attenuate","lockout","restart"].includes(a.type))return "/pack/"+a.type;
 if(a.type==="track"&&(!Number.isInteger(a.value)||a.value<s.first||a.value>s.last))throw Error("Track outside equipment range");
 if(a.type==="power"&&s.pack===a.value)return null;
 return route(s,a);
}
export async function sendCommand(path,request=(...args)=>globalThis.fetch(...args)){const c=new AbortController(),t=setTimeout(()=>c.abort(),5000);try{const r=await request("/bridge"+path,{method:"PUT",headers:{"X-ProtonLink-Command":"1"},signal:c.signal});const x=await r.json();if(!r.ok)throw Error("HTTP "+r.status+" · "+(x.error||x.status||"Rejected"));if(x.status!=="success"&&x.success!==true)throw Error("Unrecognized acknowledgement");return x}finally{clearTimeout(t)}}
export function commandedField(a){return ({mute:"muted",loop:"loop",shuffle:"shuffle",smoke:"smoke",vibration:"vibration",direction:"direction",cable:"cable"})[a.type]||null}

export function outcome(before,after,a){if(!after?.online)return "ACKNOWLEDGED / LINK LOST";const field=commandedField(a);let expected=a.value,key=field;if(a.type==="power")key="pack";if(a.type==="theme")key="theme";if(a.type==="stream")key="stream";if(a.type==="track")key="track";if(a.type==="volume")key=a.channel;if(a.type==="play"){key="playing";expected=true;if(after.paused)return "ACKNOWLEDGED / NOT CONFIRMED"}if(a.type==="stop"){key="playing";expected=false}if(a.type==="pause"){key="paused";expected=!before.paused}if(a.type==="vent"){key="temperature";expected="Venting"}if(["next","prev"].includes(a.type))return after.track!==before.track?"CONFIRMED":"ACKNOWLEDGED / NOT CONFIRMED";if(!key||after[key]==null)return "ACKNOWLEDGED / UNOBSERVABLE";return after[key]===expected?"CONFIRMED":"ACKNOWLEDGED / NOT CONFIRMED"}
