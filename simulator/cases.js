import {initial,dispatch} from './model.js';
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
export function runCases(){return cases.map(([name,fn])=>{try{fn();return {name,pass:true}}catch(e){return {name,pass:false,error:e.message}}})}
