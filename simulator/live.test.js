import test from 'node:test';
import assert from 'node:assert/strict';
import {discover,configured,deviceTracks} from './discovery.js';
import {commandPath,outcome,sendCommand} from './commands.js';
import {hardwareProfile} from './hardware.js';
test('legacy discovery requests preferences before reading',async()=>{const calls=[];const p=await discover({get:async k=>{calls.push(k);return {prefsAvailable:true,smokeEnabled:true,cyclotronDirection:1}}},()=>true,async()=>{});assert.deepEqual(calls,['settings/pack','config/pack','settings/wand','config/wand','settings/smoke','config/smoke']);assert.equal(configured(p).smoke,true);assert.equal(configured({smoke:{prefsAvailable:false,smokeEnabled:false}}).smoke,null)});
test('duplicate prefix retains positional track with warning',()=>{const x=deviceTracks({songList:'513 Saving Repeat\n513 thunderstruck'},513,514);assert.equal(x.names[514],'thunderstruck');assert.equal(x.warnings.length,1)});
test('explicit command and outcome are independent',async()=>{assert.equal(commandPath({}, {type:'smoke',value:false}),'/pack/smoke/off');assert.throws(()=>commandPath({}, {type:'smoke'}));assert.equal(outcome({}, {online:true,smoke:null},{type:'smoke',value:false}),'ACKNOWLEDGED / UNOBSERVABLE');assert.equal(outcome({}, {online:true,pack:true},{type:'power',value:true}),'CONFIRMED');await sendCommand('/pack/on',async(path,opts)=>{assert.equal(path,'/bridge/pack/on');assert.equal(opts.method,'PUT');return {ok:true,json:async()=>({status:'success'})}})});
test('hardware counts only from synchronized preferences',()=>{assert.deepEqual(hardwareProfile({pack:{prefsAvailable:true,ledCycLidCount:20,ledCycCakeCount:35,ledPowercellCount:15,ledCycCavCount:0},wand:{prefsAvailable:true,numBargraphSegments:28}}),{outer:20,inner:35,cell:15,bargraph:28,cavity:0});assert.equal(hardwareProfile({pack:{prefsAvailable:false,ledCycLidCount:20}}).outer,null)});

test('legacy pack preferences can arrive on the final poll while the pack is off', async () => {
  let elapsed = 0;
  const transport = {
    async get(path) {
      if (path.startsWith('settings/')) return {requested: true};
      if (path === 'config/pack' && elapsed >= 3000) {
        return {
          prefsAvailable: true,
          packPowered: false,
          wandPowered: false,
          packVibration: 4,
          ledCycLidCount: 20,
        };
      }
      return {prefsAvailable: false};
    },
  };
  const preferences = await discover(transport, () => true, async ms => { elapsed += ms; });
  assert.equal(hardwareProfile(preferences).outer, 20);
  assert.equal(configured(preferences).packVibration, 4);
  assert.equal(hardwareProfile(preferences).bargraph, null);
  assert.equal(configured(preferences).smoke, null);
});

import {subscribeTelemetry} from './telemetry.js';
import {lightColor} from './hardware.js';
import {initial} from './model.js';

test('socket retries, ignores retired frames, and cancels on disconnect', () => {
  const sockets = [], timers = new Map(), samples = [];
  const stop = subscribeTelemetry('ws://equipment/ws', sample => {samples.push(sample); return true}, {
    open: () => {const socket = {close() {this.onclose()}}; sockets.push(socket); return socket},
    later: (fn, ms) => {timers.set(fn, ms); return fn},
    cancel: fn => timers.delete(fn),
  });
  const first = sockets[0];
  first.onopen();
  first.onmessage({data:'bad'});
  first.close();
  const [[retry, delay]] = timers;
  assert.equal(delay, 500);
  timers.delete(retry);
  retry();
  first.onmessage({data:'{"old":true}'});
  sockets[1].onopen();
  sockets[1].onmessage({data:'{"new":true}'});
  stop();
  assert.deepEqual(samples, [{new:true}]);
  assert.equal(timers.size, 0);
});

test('live custom lighting validates hue, saturation and brightness', () => {
  const pack = {prefsAvailable:true,ledCycLidHue:127,ledCycLidSat:254,ledCycLidLum:40};
  assert.equal(lightColor(pack,'ledCycLid','Custom','red'),'color-mix(in srgb, hsl(180 100% 50%) 40%, black)');
  assert.equal(lightColor({...pack,prefsAvailable:false},'ledCycLid','Custom','red'),'red');
  assert.equal(lightColor(pack,'ledCycLid','Proton','red'),'color-mix(in srgb, red 40%, black)');
});

test('live command validation uses fresh state, confirmations and bounded inputs', () => {
  const state = initial();
  for(const intent of [{type:'power',value:true},{type:'vent',confirmed:true},{type:'track',value:499},
    {type:'volume',channel:'music',value:101},{type:'theme',value:'unknown'},{type:'smoke',value:true}]) {
    assert.throws(()=>commandPath(state,intent));
  }
  assert.throws(()=>commandPath({...state,ramping:null},{type:'power',value:true,confirmed:true}));
  assert.equal(commandPath(state,{type:'power',value:true,confirmed:true}),'/pack/on');
});

test('legacy Super Hero power uses explicit on/off routes and preserves transition guards', () => {
  const state = {...initial(), mode:'Super Hero', ramping:null};
  const power = {type:'power',value:true,confirmed:true};
  assert.equal(commandPath(state,power),'/pack/on');
  assert.equal(commandPath({...state,pack:true},{...power,value:false}),'/pack/off');
  assert.equal(commandPath({...state,pack:true},power),null);
  for (const change of [{ramping:true},{firing:true},{temperature:'Venting'},{cyclotron:'Critical'}]) {
    assert.throws(()=>commandPath({...state,...change},power));
  }
  assert.throws(()=>commandPath(state,{...power,confirmed:false}));
});

test('legacy master steps and stream eligibility use observed state', () => {
  const s={...initial(),pack:true,mode:'Super Hero',ramping:null,vgMode:null,master:40};
  assert.equal(commandPath(s,{type:'volume',channel:'master',value:45,incremental:true}),'/volume/master/up');
  assert.equal(commandPath(s,{type:'volume',channel:'master',value:35,incremental:true}),'/volume/master/down');
  assert.equal(commandPath(s,{type:'stream',value:'Stasis'}),'/pack/stream/stasis');
  for(const change of [{ramping:true},{vgMode:false},{firing:true},{pack:false}]) {
    assert.throws(()=>commandPath({...s,...change},{type:'stream',value:'Stasis'}));
  }
});

import {nextVolumeIntent} from './volume.js';
test('live master slider replans each step from the reported value', () => {
  for (const [current,target,expected] of [[30,45,35],[35,45,40],[40,45,45],[50,45,45]]) {
    assert.equal(nextVolumeIntent('master',current,target,true).value,expected);
  }
  assert.equal(nextVolumeIntent('master',45,45,true),null);
});

test('legacy year changes require idle equipment',()=>{
 const idle={...initial(),mode:'Super Hero',ramping:null,pack:false,wandPower:false};
 const year={type:'theme',value:'1989'};
 assert.equal(commandPath(idle,year),'/pack/theme/1989');
 for(const change of [{pack:true},{wandPower:true},{ramping:true},{firing:true}])assert.throws(()=>commandPath({...idle,...change},year));
});

import {decodeStatus,capabilitySummary} from './live.js';
import {readFileSync} from 'node:fs';
test('legacy and extended status preserve unknown and authoritative fields',()=>{
 const fixture=JSON.parse(readFileSync(new URL('../tests/fixtures/legacy-gen1-status.json',import.meta.url),'utf8'));
 const legacy=decodeStatus(fixture);
 assert.equal(legacy.crossedStreams,null);
 assert.equal(legacy.canChangeStream,null);
 const extended=decodeStatus({...fixture,crossedStreams:true,lockout:true,canChangeStream:false,packTempC:32,packTempF:89.6,temperature:'Future state'});
 assert.equal(extended.crossedStreams,true);
 assert.equal(extended.lockout,true);
 assert.equal(extended.packTempC,32);
 assert.equal(extended.packTempF,89.6);
 assert.equal(extended.temperature,'Unknown');
 assert.throws(()=>commandPath(extended,{type:'vent',confirmed:true}));
 assert.throws(()=>commandPath({...legacy,canChangeStream:false},{type:'stream',value:'Stasis'}));
 assert.equal(commandPath({...legacy,canChangeStream:true},{type:'stream',value:'Stasis'}),'/pack/stream/stasis');
 assert.equal(capabilitySummary({audioCorrupt:true,audioOutdated:false},null).audioCorrupt,true);
});
