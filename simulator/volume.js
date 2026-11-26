// GPStar archived pack Configuration.h uses 5% steps for all three channels.
// Master is absolute; music/effects only expose incremental up/down routes.
export const VOLUME_STEP=5;
export const VOLUME_DEBOUNCE_MS=600;
export const channels=["master","effects","music"];
export function volumeTarget(value){return Math.max(0,Math.min(100,Math.round(value/VOLUME_STEP)*VOLUME_STEP))}
export function nextVolumeIntent(channel,current,target, incrementalMaster=false){
 if(!channels.includes(channel)||!Number.isFinite(current)||!Number.isFinite(target))return null;
 const desired=volumeTarget(target);
 if(current===desired)return null;
 if(channel==="master"&&!incrementalMaster)return {type:"volume",channel,value:desired};
 const value=Math.max(0,Math.min(100,current+Math.sign(desired-current)*VOLUME_STEP));
 // An unaligned legacy reading can make the requested exact value unreachable.
 // Stop rather than overshoot and oscillate.
 if(Math.abs(value-desired)>=Math.abs(current-desired))return null;
 return {type:"volume",channel,value};
}
// Pure debounce bookkeeping: each channel has its own target and deadline.
export function queueVolume(queue,channel,value,now){
 if(!channels.includes(channel)||!Number.isFinite(value))return queue;
 return {...queue,[channel]:{target:volumeTarget(value),due:now+VOLUME_DEBOUNCE_MS}};
}
export function dueVolumes(queue,now,dragging=[]){return channels.filter(k=>queue[k]&&queue[k].due<=now&&!dragging.includes(k))}
