import {decodeStatus,capabilitySummary,ReadOnlyTransport} from './live.js';
import {normalizeVisual,equipmentState,illumination,activeFace,scanRate,eventChanges,emptyStats,accumulate,hardwareMarkup} from './immersion.js';
import {normalizeField,gesturePage,selectTracks,readiness,reduceAudio,equipmentSnapshot,equipmentPlan} from './field.js';
import {fresh,linked,classify,commandGuard,backup,parseBackup,scenarios,runScenario} from './reliability.js';
import {isQuick,toggleQuick,unlockPage,idlePage,allowed as quickAllowed,intent as quickIntent,quickHeader,quickBody,quickTransport} from './quick.js';
import {fromDevice,trackTitle,importTracks,fromFilenames,escapeHTML} from './library.js';
import {initial,dispatch,canChangeStream} from './model.js';
import {exchange,route,connect,status,sync} from './protocol.js';
import {simulate,diff,defaults,readPreview} from './lab.js';
import {volumeTarget,nextVolumeIntent,queueVolume,dueVolumes} from './volume.js';
import {faces,DEFAULT_FACE,resolveFace,normalizeColor,hsv,radarContacts,needleWobble,wheelPick} from './faces.js';
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
 ['Firing requires connected wand and powered pack',()=>{let s=initial();check(dispatch(s,{type:'physical',control:'firing',value:true})===s,'firing in standby');s=dispatch(s,{type:'physical',control:'power',value:true});s=dispatch(s,{type:'physical',control:'wandPower',value:true});s=dispatch(s,{type:'physical',control:'barrel',value:true});check(dispatch(s,{type:'physical',control:'firing',value:true}).firing,'firing denied')}]
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
cases.push(
 ["Lid detection independent of ribbon and wand",()=>{const s=dispatch(initial(),{type:"physical",control:"lid",value:false});check(!s.lid&&s.cable&&s.wand&&!status(s).cyclotronLid,"lid conflated")}],
 ["Ribbon removal independent of hose and lid",()=>{const s=dispatch(initial(),{type:"physical",control:"cable",value:false});check(!s.cable&&s.lid&&s.wand,"ribbon conflated")}],
 ["Wand disconnect independent of ribbon and lid",()=>{const s=dispatch(initial(),{type:"physical",control:"wand",value:false});check(!s.wand&&s.cable&&s.lid,"wand conflated")}],
 ["Stream requires active pack and eligible mode",()=>{const s=open();check(!canChangeStream(s)&&canChangeStream({...s,pack:true})&&!canChangeStream({...s,pack:true,mode:"Standard"})&&!canChangeStream({...s,pack:true,ramping:true})&&!canChangeStream({...s,pack:true,cable:false}),"stream guard mismatch")}],
 ["Theme blocked with wand running",()=>{const s={...open(),wandPower:true};check(exchange(s,{type:"theme",value:"1989"}).state===s,"theme changed")}],
 ["Unsynced gateway HTTP returns empty status",()=>{check(JSON.stringify(sync(initial(),false)[1].body)==="{}","incorrect unsynced body")}],
 ["Volume serial command and confirmation are distinct from ACK",()=>{const x=exchange(open(),{type:"volume",channel:"master",value:90});const i=x.events.findIndex(e=>e.summary==="A_VOLUME_SET"),j=x.events.findIndex(e=>e.body?.status==="success"),k=x.events.findIndex(e=>e.summary==="A_VOLUME_SYNC");check(i>=0&&i<j&&j<k&&x.events[k].body[0]===90,"serial ordering")}],
 ["No fictional direct wand firing command",()=>{check(route(open(),{type:"firing",value:true})===null,"unexpected API")}]
);
cases.push(
 ["Approved 11-face collection defaults to PKE",()=>{check(faces.length===11&&DEFAULT_FACE==="pke"&&resolveFace("obsolete")==="pke","face default")}],
 ["Marquee color conversion and bounds",()=>{check(hsv({h:0,s:100,v:100})==="#ff0000"&&hsv({h:0,s:0,v:100})==="#ffffff"&&normalizeColor({v:0}).v===10,"color conversion")}],
 ["Radar contacts stay in assigned quadrants",()=>{for(let t=0;t<120;t+=.25){const n=radarContacts(initial(),t);check(n[0].x<185&&n[0].y<215&&n[1].x>185&&n[1].y<215&&n[2].x<185&&n[2].y>215&&n[3].x>185&&n[3].y>215,"quadrant crossed")}}],
 ["Offline radar contacts are gray",()=>{check(radarContacts({...initial(),online:false},0).every(n=>n.color==="#526059"),"stale status")}],
 ["Higher power increases needle wobble envelope",()=>{check(needleWobble(0,1)===0&&Math.max(...Array.from({length:200},(_,i)=>Math.abs(needleWobble(5,i/20))))>Math.max(...Array.from({length:200},(_,i)=>Math.abs(needleWobble(1,i/20)))),"wobble")}],
 ["Color wheel center is unsaturated",()=>{check(wheelPick(90,90,180).s===0&&wheelPick(173,90,180).s===100,"wheel mapping")}]
);
cases.push(
 ["GPStar names preserve blank track positions",()=>{const m=fromDevice({songList:"A\n\nC"},500,520);check(m[500]==="A"&&m[501]===""&&m[502]==="C","track shift")}],
 ["Missing names fall back without altering playback IDs",()=>{check(fromDevice({},500,520)===null&&trackTitle({}, {},502,500)==="Track 3","fallback")}],
 ["Local metadata overrides synchronized names",()=>{check(trackTitle({500:"Local"},{500:"Remote"},500,500)==="Local","precedence")}],
 ["CSV explicit IDs and quoted commas",()=>{const m=importTracks('id,title\n502,"A, B"', "x.csv",500,520);check(m[502]==="A, B","CSV mapping")}],
 ["M3U imports EXTINF and filename fallback",()=>{const m=importTracks("#EXTM3U\n#EXTINF:20,Theme\nsong.wav\nother.mp3","x.m3u",500,520);check(m[500]==="Theme"&&m[501]==="other","M3U mapping")}],
 ["Numbered audio filenames preserve real IDs",()=>{const m=fromFilenames(["503 Theme.wav","500 Intro.wav"],500,520);check(m[503]==="Theme"&&m[500]==="Intro","filename mapping")}],
 ["Out of range and duplicate imports are rejected",()=>{for(const data of ['[{"id":521,"title":"A"}]','[{"id":500,"title":"A"},{"id":500,"title":"B"}]']){let failed=false;try{importTracks(data,"x.json",500,520)}catch{failed=true}check(failed,"invalid accepted")}}],
 ["Imported names cannot inject watch HTML",()=>{check(escapeHTML('<img onerror="x">')==="&lt;img onerror=&quot;x&quot;&gt;","HTML escape")}]
);
cases.push(
 ["Quick monitor toggle never unlocks Field Lock",()=>{check(toggleQuick("quick")==="monitor"&&toggleQuick("monitor")==="quick"&&toggleQuick("field")==="field","navigation")}],
 ["Quick mode remains opt-in and returns from submenu inactivity",()=>{check(unlockPage("field")==="home"&&unlockPage("quick")==="quick"&&idlePage("quick","music")==="quick"&&idlePage("quick","monitor")===null&&idlePage("field","music")==="field","idle")}],
 ["Quick commands are blocked while locked or disconnected",()=>{check(!quickAllowed(initial(),"on")&&!quickAllowed({...open(),online:false},"on")&&quickAllowed(open(),"on"),"guards")}],
 ["Quick commands map to existing API intentions",()=>{check(quickIntent("on").value===true&&quickIntent("off").value===false&&quickIntent("vent").type==="vent","intent")}],
 ["Quick stop stays available during vent cycle",()=>{const s={...open(),pack:true,temperature:"Venting"};check(quickAllowed(s,"off")&&!quickAllowed(s,"vent")&&!quickAllowed(s,"on"),"vent guard")}],
 ["Quick markup contains three faders and menu and monitor entry",()=>{const s=open(),body=quickBody(s,["master","effects","music"],{},false),head=quickHeader(s,"12:00",4);check((body.match(/data-fader=/g)||[]).length===3&&head.includes('data-act="go:home"')&&head.includes('id="quickDots"'),"markup")}]
);
cases.push(
 ["Industrial transport displays named track and actual ID",()=>{const s=open(),h=quickTransport(s,false,"A & B");check(h.includes(String(s.track))&&h.includes("A &amp; B")&&h.includes('data-act="go:quicklibrary"'),"track readout")}],
 ["Industrial transport start and pause follow playback",()=>{const s=open(),a=quickTransport(s,false,"A"),b=quickTransport({...s,playing:true,paused:true},false,"B");check(a.includes('data-act="play"')&&a.includes('data-act="pause" disabled')&&b.includes('data-act="stop"')&&b.includes("RESUME"),"transport state")}],
 ["Industrial transport uses existing music routes",()=>{const s=open();check(route(s,{type:"play"})==="/music/startstop"&&route({...s,playing:true},{type:"pause"})==="/music/pauseresume"&&route(s,{type:"track",value:503})==="/music/select?track=503","route mapping")}],
 ["Industrial transport blocks writes while offline",()=>{const h=quickTransport({...open(),online:false},false,"A");check((h.match(/ disabled/g)||[]).length===4,"offline guard")}]
);
cases.push(
 ["Telemetry expires after 15 seconds",()=>{check(fresh(1000,16000)&&!fresh(1000,16001),"freshness")}],
 ["Stale telemetry blocks commands",()=>{check(commandGuard(open(),defaults(),{type:"play"},1000,20000)==="Telemetry stale","guard")}],
 ["Dropped response classified uncertain",()=>{check(classify(simulate(open(),{type:"play"},{fault:"response"}))==="UNCERTAIN","classification")}],
 ["Configuration backup round trip",()=>{const x=parseBackup(JSON.stringify(backup("pke",{mainMode:"quick"},{local:{500:"Theme"},synced:{},auto:true},{})));check(x.trackStore.local[500]==="Theme"&&x.preferences.mainMode==="quick","backup")}],
 ...scenarios.map(spec=>["Scenario: "+spec.name,()=>{const r=runScenario(spec);check(r.pass,r.error)}])
);
cases.push(
 ["Quick module order and hidden modules are preserved",()=>{const f=normalizeField({order:["transport","music"],hidden:["effects"]});const h=quickBody(open(),["master","effects","music"],{},false,"A",f);check(h.indexOf("quick-transport")<h.indexOf('data-channel="music"')&&!h.includes('data-channel="effects"'),"layout")}],
 ["Horizontal gestures stay in allowed navigation pages",()=>{check(gesturePage("quick",-100,5)==="monitor"&&gesturePage("field",-100,0)==="field"&&gesturePage("music",10,100)==="music","gesture")}],
 ["Track search preserves authoritative IDs",()=>{check(selectTracks(500,505,id=>id===503?"Theme":"Other","theme")[0]===503,"search")}],
 ["Favorite and playlist filters preserve track range",()=>{const f=normalizeField({favorites:[500,500,9000],playlists:[{name:"Set",ids:[503,501]}]});check(f.favorites.length===1&&f.playlists[0].ids[0]===503&&selectTracks(500,502,String,"",f.playlists[0].ids).join()==="501","library")}],
 ["Audio reduction rounds to supported increments",()=>{check(reduceAudio([75,80,70]).join()==="40,40,35","audio")}],
 ["Equipment preset never enables smoke or powers equipment",()=>{const s=open(),p=equipmentSnapshot("Test",{...s,smoke:true,pack:true});check(!equipmentPlan(s,p,false).some(a=>a.type==="power"||a.type==="smoke"),"preset guard")}],
 ["Startup check flags missing cable and stale telemetry",()=>{check(readiness({...open(),cable:false},true,false).filter(x=>!x[1]).length===2,"readiness")}],
 ["Pack profiles store separate track names",()=>{const f=normalizeField({packs:[{name:"A",tracks:{local:[{id:500,title:"A"}]}},{name:"B",tracks:{local:[{id:500,title:"B"}]}}]});check(f.packs[0].tracks.local[0].title!==f.packs[1].tracks.local[0].title,"profiles")}]
);
cases.push(
 ["Visual preferences preserve default PKE behavior",()=>{const v=normalizeVisual();check(v.finish==="weathered"&&!v.dynamic&&activeFace("pke",open(),v)==="pke","defaults")}],
 ["Adaptive illumination follows alarm and vent telemetry",()=>{check(illumination({...open(),cable:false})==="#ff6758"&&equipmentState({...open(),temperature:"Venting"})==="venting","illumination")}],
 ["Dynamic faces obey alarm priority",()=>{const v=normalizeVisual({dynamic:true,firingFace:"analog",alarmFace:"mission"});check(activeFace("pke",{...open(),firing:true},v)==="analog"&&activeFace("pke",{...open(),firing:true,cable:false},v)==="mission","priority")}],
 ["PKE sweep accelerates with equipment condition",()=>{check(scanRate({...open(),pack:true,firing:true})>scanRate(open())&&scanRate({...open(),online:false})===0,"scan")}],
 ["Event overlays only reflect observed changes",()=>{const s=open();check(eventChanges(s,{...s,pack:true,track:503}).join()==="PACK ACTIVATED,AUDIO TRACK 503"&&eventChanges(s,{...s,online:false,pack:true}).join()==="LINK LOST","events")}],
 ["Session statistics separate observed and unknown time",()=>{const s={...open(),pack:true,firing:true,level:3};let n=accumulate(emptyStats(),s,{...s,temperature:"Venting"},1000);n=accumulate(n,s,{...s,online:false},2000);check(n.packMs===1000&&n.firingMs===1000&&n.vents===1&&n.levels[2]===1000&&n.unknownMs===2000,"stats")}],
 ["Hardware previews only draw selected optional components",()=>{check(!hardwareMarkup(open(),{}).includes("N-FILTER")&&hardwareMarkup(open(),{filter:true}).includes("N-FILTER"),"hardware")}]
);
cases.push(
 ["Live status mapping uses reported equipment state",()=>{const raw=status({...open(),pack:true,level:4,track:503});const x=decodeStatus(raw);check(x.pack&&x.level===4&&x.track===503&&x.locked,"mapping")}],
 ["Empty and malformed status rejected",()=>{for(const x of [{},null,{...status(open()),volMaster:900}]){let rejected=false;try{decodeStatus(x)}catch{rejected=true}check(rejected,"invalid accepted")}}],
 ["Live capabilities do not invent OpenAPI",()=>{check(capabilitySummary({buildDate:"test"},null).openapi===null,"capability")}]
);
cases.push(
 ["Legacy Gen 1 physical sample accepts optional omissions",()=>{const x=decodeStatus({"mode":"Super Hero","modeID":1,"theme":"1984","themeID":2,"switch":"Ready","pack":"Powered","power":"1","safety":"Safety Off","wand":"Connected","wandPower":"Idle","wandMode":"Proton Stream","firing":"Idle","cable":"Connected","cyclotron":"Normal","cyclotronLid":true,"temperature":"Normal","musicPlaying":false,"musicPaused":false,"musicCurrent":500,"musicStart":500,"musicEnd":514,"volMaster":10,"volEffects":100,"volMusic":100,"battVoltage":5.26,"wandAmps":0,"apClients":1,"wsClients":0});check(x.mode==="Super Hero"&&x.pack&&x.track===500&&x.last===514&&x.master===10&&x.battVoltage===5.26&&x.muted===null&&x.direction===null&&x.ramping===null&&x.smoke===null,"legacy mapping")}],
 ["Reported false remains distinct from unreported",()=>{const x=decodeStatus({...{"mode":"Super Hero","modeID":1,"theme":"1984","themeID":2,"switch":"Ready","pack":"Powered","power":"1","safety":"Safety Off","wand":"Connected","wandPower":"Idle","wandMode":"Proton Stream","firing":"Idle","cable":"Connected","cyclotron":"Normal","cyclotronLid":true,"temperature":"Normal","musicPlaying":false,"musicPaused":false,"musicCurrent":500,"musicStart":500,"musicEnd":514,"volMaster":10,"volEffects":100,"volMusic":100,"battVoltage":5.26,"wandAmps":0,"apClients":1,"wsClients":0},volMuted:false});check(x.muted===false&&x.loop===null,"unknown distinction")}]
);
export function runCases(){return cases.map(([name,fn])=>{try{fn();return {name,pass:true}}catch(e){return {name,pass:false,error:e.message}}})}
