// Diagnostic geometry is derived only from validated preferences.
export function hardwareProfile(p){const pack=p.pack?.prefsAvailable===true?p.pack:{},wand=p.wand?.prefsAvailable===true?p.wand:{};const n=(v,allowed)=>allowed.includes(v)?v:null;return {outer:n(pack.ledCycLidCount,[12,20,36,40]),inner:n(pack.ledCycCakeCount,[12,23,24,26,35,36]),cell:n(pack.ledPowercellCount,[13,15]),bargraph:n(wand.numBargraphSegments,[28,30]),cavity:Number.isInteger(pack.ledCycCavCount)&&pack.ledCycCavCount>=0&&pack.ledCycCavCount<=20?pack.ledCycCavCount:null}}
export function lightColor(pack, prefix, stream, fallback) {
 if (pack?.prefsAvailable !== true) return fallback;
 const hue = pack[prefix+"Hue"], sat = pack[prefix+"Sat"], lum = pack[prefix+"Lum"];
 const valid = (value,min,max) => Number.isInteger(value) && value >= min && value <= max;
 const custom = stream === "Custom" && valid(hue,2,254) && valid(sat,2,254);
 const color = custom ? "hsl("+hue*360/254+" "+sat*100/254+"% 50%)" : fallback;
 return valid(lum,20,100) ? "color-mix(in srgb, "+color+" "+lum+"%, black)" : color;
}
export function discoveredRing(profile,s,pack){const ring=(n,r,cls,color)=>Array.from({length:n||0},(_,i)=>{const a=2*Math.PI*i/n;return '<circle class="'+cls+'" cx="'+(145+r*Math.cos(a))+'" cy="'+(246+r*Math.sin(a))+'" r="2" fill="'+color+'"/>'}).join("");return s.lid?ring(profile.outer,59,"discovered-outer",s.pack?lightColor(pack,"ledCycLid",s.stream,"#ea6560"):"#533033"):ring(profile.inner,43,"discovered-inner",s.pack?lightColor(pack,"ledCycCake",s.stream,"#88b9ed"):"#35495b")}
