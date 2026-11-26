import {test,expect} from '@playwright/test';
import {spawn} from 'node:child_process';
let server;
test.beforeAll(async()=>{server=spawn('python',['-m','http.server','8765'],{stdio:'ignore'});for(let i=0;i<40;i++){try{if((await fetch('http://127.0.0.1:8765/simulator/')).ok)return}catch{}await new Promise(r=>setTimeout(r,250))}throw Error('Server unavailable')});
test.afterAll(()=>server?.kill());
test('boot, unlock, industrial transport and lock',async({page})=>{const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.setViewportSize({width:1500,height:900});await page.goto('http://127.0.0.1:8765/simulator/');await expect(page.locator('#fieldCanvas')).toBeVisible();await expect(page.locator('#bootLayer')).toBeHidden();const r=await page.locator('#body').boundingBox();await page.mouse.move(r.x+r.width/2,r.y+r.height/2);await page.mouse.down();await page.waitForTimeout(2100);await page.mouse.up();await page.mouse.move(r.x+r.width*.8,r.y+r.height*.5);await page.mouse.down();await page.mouse.move(r.x+r.width*.2,r.y+r.height*.5,{steps:8});await page.mouse.up();await expect(page.locator('.quick-channel')).toHaveCount(3);await expect(page.locator('[data-quick="vent"]')).toBeVisible();await expect(page.locator('.quick-transport')).toBeVisible();await page.locator('.quick-keys [data-act="play"]').click();await expect(page.locator('.quick-keys [data-act="stop"]')).toBeVisible();await page.locator('#quickDots').click();await expect(page.locator('#fieldCanvas')).toBeVisible();await page.locator('#quickDots').click();await expect(page.locator('.quick-transport')).toBeVisible();expect(errors).toEqual([])});

test('field setup persists module visibility and favorites',async({page})=>{await page.setViewportSize({width:1500,height:900});await page.goto('http://127.0.0.1:8765/simulator/');await page.locator('[data-tab="field"]').click();await expect(page.locator('#moduleOrder input')).toHaveCount(4);await page.locator('#moduleOrder input').first().uncheck();await page.locator('#fieldSearch').fill('503');await expect(page.locator('#fieldTracks button')).toHaveCount(1);await page.locator('#fieldTracks button').click();await expect(page.locator('#fieldTracks button')).toHaveText('★');await page.reload();await page.locator('[data-tab="field"]').click();await expect(page.locator('#moduleOrder input').first()).not.toBeChecked()});

test('display preferences, startup and observed session panel',async({page})=>{await page.setViewportSize({width:1500,height:900});await page.goto('http://127.0.0.1:8765/simulator/');await expect(page.locator('#bootLayer')).toBeHidden();await page.locator('[data-tab="immersion"]').click();await page.locator('#finishPick').selectOption('tactical');await expect(page.locator('#watch')).toHaveAttribute('data-finish','tactical');await page.locator('#filterPick').check();await expect(page.locator('#packGraphic')).toContainText('N-FILTER');await page.locator('#replayBoot').click();await expect(page.locator('#bootLayer')).toBeVisible();await page.locator('#skipBoot').click();await expect(page.locator('#bootLayer')).toBeHidden();await expect(page.locator('#fieldStats')).toContainText('Observed pack on');await page.reload();await expect(page.locator('#watch')).toHaveAttribute('data-finish','tactical')});

test('live transport invokes native fetch with Window receiver',async({page})=>{await page.goto('http://127.0.0.1:8765/simulator/');await page.route('**/bridge/status',route=>route.fulfill({json:{pack:'Idle'}}));const result=await page.evaluate(async()=>{const native=window.fetch;window.fetch=function(...args){if(this!==window)throw new Error('Invalid fetch receiver');return native.apply(window,args)};try{const {ReadOnlyTransport}=await import('/simulator/live.js');return await new ReadOnlyTransport().get('status')}finally{window.fetch=native}});expect(result).toEqual({pack:'Idle'})});

test('legacy physical status renders in read-only connection',async({page})=>{await page.goto('http://127.0.0.1:8765/simulator/');await page.route('**/bridge/info',r=>r.fulfill({json:{mode:'read-only'}}));await page.route('**/bridge/status',r=>r.fulfill({json:{"mode":"Super Hero","modeID":1,"theme":"1984","themeID":2,"switch":"Ready","pack":"Powered","power":"1","safety":"Safety Off","wand":"Connected","wandPower":"Idle","wandMode":"Proton Stream","firing":"Idle","cable":"Connected","cyclotron":"Normal","cyclotronLid":true,"temperature":"Normal","musicPlaying":false,"musicPaused":false,"musicCurrent":500,"musicStart":500,"musicEnd":514,"volMaster":10,"volEffects":100,"volMusic":100,"battVoltage":5.26,"wandAmps":0,"apClients":1,"wsClients":0}}));await page.route('**/bridge/config/device',r=>r.fulfill({json:{songList:'Theme'}}));await page.route('**/bridge/openapi.json',r=>r.fulfill({status:404}));await page.locator('[data-tab="connection"]').click();await page.locator('#connectReal').click();await expect(page.locator('#liveRaw')).toContainText('Super Hero');await expect(page.locator('#packData')).toContainText('5.26 V');await expect(page.locator('#packData')).toContainText('Unknown');await expect(page.locator('#connectionMode')).toContainText('READ ONLY');await expect(page.locator('#physical button').first()).toBeDisabled()});

test('WebSocket reconnect updates equipment lights and stops on disconnect', async ({page}) => {
  const {readFile} = await import('node:fs/promises');
  const raw = JSON.parse(await readFile(new URL('./fixtures/legacy-gen1-status.json',import.meta.url)));
  const peers = [];
  await page.routeWebSocket('ws://127.0.0.1:8766/ws', socket => peers.push(socket));
  await page.route('**/bridge/**', route => {
    const path = new URL(route.request().url()).pathname;
    const json = path.endsWith('/info') ? {mode:'read-only',websocket:'ws://127.0.0.1:8766/ws'} :
      path.endsWith('/status') ? raw : path.endsWith('/config/pack') ?
      {prefsAvailable:true,ledCycLidCount:20} : {prefsAvailable:true};
    return route.fulfill({json});
  });
  await page.goto('http://127.0.0.1:8765/simulator/');
  await page.locator('[data-tab="connection"]').click();
  await page.locator('#connectReal').click();
  await expect(page.locator('.discovered-outer')).toHaveCount(20);
  await expect.poll(()=>peers.length).toBe(1);
  peers[0].close();
  await expect.poll(()=>peers.length).toBe(2);
  peers[1].send(JSON.stringify({...raw,pack:'Idle'}));
  await expect(page.locator('.discovered-outer').first()).toHaveAttribute('fill','#533033');
  await page.locator('#disconnectReal').click();
  await expect(page.locator('#connectionMode')).toHaveText('SIMULATION');
});
