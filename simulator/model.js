// Independent local simulation. No networking or equipment access.
export const initial = () => ({online:true,locked:true,pack:false,playing:false,paused:false,track:501,first:500,last:520,master:75,effects:80,music:70,muted:false,loop:false,shuffle:false,theme:"1984",stream:"Proton",smoke:false,vibration:true,direction:true,level:1,wand:true,wandPower:false,barrel:false,lid:true,ionArm:false,ramping:false,mode:"Upgraded",vgMode:true,cable:true,cyclotron:"Normal",temperature:"Normal",firing:false});
export const streams=["Proton","Stasis","Slime","Meson","Spectral","Halloween","Christmas","Custom"];
export function canChangeStream(s){return s.pack&&!s.firing&&!s.ramping&&s.temperature!=="Venting"&&s.cable&&s.mode!=="Standard"&&s.vgMode&&["Normal","Active"].includes(s.cyclotron)}
export const themes=["1984","1989","2021","2024"];
const clamp=v=>Math.max(0,Math.min(100,v));
export function dispatch(s,a) {
 if(a.type==="lock") return {...s,locked:true};
 if(a.type==="unlock") return s.online?{...s,locked:false}:s;
 if(a.type==="scenario"){
  const k=a.value;
  if(k==="offline")return {...s,online:false,locked:true};
  if(!["standby","active","firing","warning","critical","venting"].includes(k))return s;
  return {...s,online:true,pack:k!=="standby",wandPower:k==="firing",barrel:k==="firing",ramping:false,firing:k==="firing"&&s.wand,cyclotron:({firing:"Normal",warning:"Warning",critical:"Critical",venting:"Recovery"})[k]||"Normal",temperature:k==="venting"?"Venting":"Normal"};
 }
 // Physical pack controls are independent of the watch lock/link.
 if(a.type==="physical"){
  switch(a.control){
   case "power":return {...s,ionArm:!!a.value,pack:!!a.value,ramping:false,wandPower:a.value?s.wandPower:false,firing:false,cyclotron:"Normal",temperature:"Normal"};
   case "music":return {...s,playing:!!a.value,paused:false};
   case "level":return Number.isInteger(a.value)&&a.value>=1&&a.value<=5?{...s,level:a.value}:s;
   case "wand":return {...s,wand:!!a.value,wandPower:a.value?s.wandPower:false,firing:a.value?s.firing:false};
   case "wandPower":return s.wand?{...s,wandPower:!!a.value,firing:a.value?s.firing:false}:s;
   case "barrel":return s.wand?{...s,barrel:!!a.value,firing:a.value?s.firing:false}:s;
   case "lid":return {...s,lid:!!a.value};
   case "ramping":return {...s,ramping:!!a.value,firing:false};
   case "mode":return !s.pack?{...s,mode:a.value?"Upgraded":"Standard"}:s;
   case "vgMode":return {...s,vgMode:!!a.value};
   case "cable":return {...s,cable:!!a.value,firing:a.value?s.firing:false};
   case "firing":return s.pack&&s.wand&&s.wandPower&&s.barrel&&s.cable&&!s.ramping&&s.temperature!=="Venting"?{...s,firing:!!a.value}:s;
   default:return s;
  }
 }
 if(s.locked||!s.online)return s;
 const normal=["Normal","Active"].includes(s.cyclotron)&&!s.firing&&s.temperature!=="Venting";
 switch(a.type){
 case "power":return a.confirmed&&normal&&!s.ramping&&s.pack!==!!a.value?{...s,pack:!!a.value,wandPower:a.value?s.wandPower:false,firing:false}:s;
 case "play":return {...s,playing:true,paused:false};
 case "stop":return {...s,playing:false,paused:false};
 case "pause":return s.playing?{...s,paused:!s.paused}:s;
 case "next":return {...s,track:s.track===s.last?s.first:s.track+1};
 case "prev":return {...s,track:s.track===s.first?s.last:s.track-1};
 case "track":return Number.isInteger(a.value)&&a.value>=s.first&&a.value<=s.last?{...s,track:a.value}:s;
 case "volume":return ["master","effects","music"].includes(a.channel)&&Number.isFinite(a.value)?{...s,[a.channel]:clamp(a.value)}:s;
 case "mute":return {...s,muted:!s.muted};
 case "loop":return {...s,loop:!s.loop};
 case "shuffle":return {...s,shuffle:!s.shuffle};
 case "theme":return normal&&!s.pack&&!s.wandPower&&!s.ramping&&themes.includes(a.value)?{...s,theme:a.value}:s;
 case "stream":return canChangeStream(s)&&streams.includes(a.value)?{...s,stream:a.value}:s;
 case "smoke":return normal&&(!a.value||a.confirmed)?{...s,smoke:!!a.value}:s;
 case "vibration":return {...s,vibration:!s.vibration};
 case "direction":return normal?{...s,direction:!s.direction}:s;
 case "vent":return a.confirmed&&s.pack&&normal?{...s,temperature:"Venting",cyclotron:"Recovery"}:s;
 default:return s;
 }
}
