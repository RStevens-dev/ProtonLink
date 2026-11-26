// Independent local simulation. No networking or equipment access.
export const initial = () => ({online:true,locked:true,pack:false,playing:false,paused:false,track:501,first:500,last:520,master:75,effects:80,music:70,muted:false,loop:false,shuffle:false,theme:"1984",stream:"Proton",smoke:false,vibration:true,direction:true,level:1,wand:true,cable:true,cyclotron:"Normal",temperature:"Normal",firing:false});
export const streams=["Proton","Stasis","Slime","Meson","Spectral","Halloween","Christmas","Custom"];
export const themes=["1984","1989","2021","2024"];
const clamp=v=>Math.max(0,Math.min(100,v));
export function dispatch(s,a) {
 if(a.type==="lock") return {...s,locked:true};
 if(a.type==="unlock") return s.online?{...s,locked:false}:s;
 if(a.type==="scenario"){
  const k=a.value;
  if(k==="offline")return {...s,online:false,locked:true};
  if(!["standby","active","firing","warning","critical","venting"].includes(k))return s;
  return {...s,online:true,pack:k!=="standby",firing:k==="firing",cyclotron:({firing:"Active",warning:"Warning",critical:"Critical",venting:"Recovery"})[k]||"Normal",temperature:k==="venting"?"Venting":"Normal"};
 }
 // Physical pack controls are independent of the watch lock/link.
 if(a.type==="physical"){
  switch(a.control){
   case "power":return {...s,pack:!!a.value,firing:false,cyclotron:"Normal",temperature:"Normal"};
   case "music":return {...s,playing:!!a.value,paused:false};
   case "level":return Number.isInteger(a.value)&&a.value>=1&&a.value<=5?{...s,level:a.value}:s;
   case "wand":return {...s,wand:!!a.value,firing:a.value?s.firing:false};
   case "cable":return {...s,cable:!!a.value};
   case "firing":return s.pack&&s.wand?{...s,firing:!!a.value,cyclotron:a.value?"Active":"Normal"}:s;
   default:return s;
  }
 }
 if(s.locked||!s.online)return s;
 const normal=["Normal","Active"].includes(s.cyclotron)&&!s.firing&&s.temperature!=="Venting";
 switch(a.type){
 case "power":return a.confirmed&&normal?{...s,pack:!!a.value}:s;
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
 case "theme":return normal&&themes.includes(a.value)?{...s,theme:a.value}:s;
 case "stream":return normal&&streams.includes(a.value)?{...s,stream:a.value}:s;
 case "smoke":return normal&&(!a.value||a.confirmed)?{...s,smoke:!!a.value}:s;
 case "vibration":return {...s,vibration:!s.vibration};
 case "direction":return normal?{...s,direction:!s.direction}:s;
 case "vent":return a.confirmed&&s.pack&&normal?{...s,temperature:"Venting",cyclotron:"Recovery"}:s;
 default:return s;
 }
}
