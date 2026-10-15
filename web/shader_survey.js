// Reaches Diddy Kong Racing's maps by code for record_shaders.sh (shader_plan.json loads it into the page):
// go(map, seconds, how) loads a map the way the game's own teleport to Future Fun Land does (begin_level_teleport:
// gNextMap, gLevelLoadType 4, gLevelLoadTimer), with the map's own vehicle from the level table, and then:
// - in a race, once the countdown is over, drives the player's kart on the keyboard toward the track's next
//   checkpoint (racer->nextCheckpoint in gTrackCheckpoints), with the gas held, so about a lap of the track is seen
//   with the race's HUD and camera as a player sees them;
// - with how "finish", marks the player's kart finished instead (racer->raceFinished): the AI drives it, the finish
//   camera follows it and the game goes on to its results;
// - with how "stand", leaves the player's kart at the start while the others race;
// - in a hub, where there is no race, drives round and round with the gas held.
// The AI can't simply drive the player's kart (playerIndex -1): DKR-R's display list check then rejects every frame
// ("counted display-list address=0x000000"), and the level, once left, leaks the game's own memory.
// The addresses are the US 1.0 decomp's (build/decomp/v77).
(() => {
  const word = (a) => n64r(a & ~3) >>> 0;
  const rb = (a) => (word(a) >>> ((3 - (a & 3)) * 8)) & 0xff;
  const wb = (a, v) => { const s = (3 - (a & 3)) * 8; n64w(a & ~3, ((word(a) & ~(0xff << s)) | ((v & 0xff) << s)) >>> 0); };
  const wh = (a, v) => { const s = (2 - (a & 2)) * 8; n64w(a & ~3, ((word(a) & ~(0xffff << s)) | ((v & 0xffff) << s)) >>> 0); };
  const f32 = (a) => new Float32Array(new Uint32Array([word(a)]).buffer)[0];
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const key = (type, code, k, keyCode) => window.dispatchEvent(new KeyboardEvent(type, { bubbles: true, code, key: k, keyCode, which: keyCode }));
  const gGameMode = 0x801234ec, gPlayableMapId = 0x801234f4, gIsLoading = 0x80123514, gLevelLoadType = 0x80123524,
    gNextMap = 0x80123525, gLevelLoadTimer = 0x800dd394, gRacersByPort = 0x8011aeec, gNumRacers = 0x8011aef0,
    gLevelDefaultVehicleID = 0x80123518, gGlobalLevelTable = 0x8012117c,
    gCurrentMenuId = 0x800df470;
  const kart = () => { const ports = word(gRacersByPort); return ports ? word(ports) : 0; }; // the player's Object
  const racer = () => { const object = kart(); return object ? word(object + 0x64) : 0; }; // its Object_Racer
  // A test that throws (RDRAM not there yet, before the game starts) counts as false.
  const until = async (test, ms) => {
    for (const end = Date.now() + ms; Date.now() < end; await sleep(100)) {
      try { if (test()) return true; } catch { /* not yet */ }
    }
    return false;
  };
  window.dkrState = () => ({ mode: word(gGameMode) | 0, map: word(gPlayableMapId) | 0, menu: word(gCurrentMenuId) | 0,
    loading: rb(gIsLoading), racers: word(gNumRacers) | 0 });
  // For the plan's start: true once the game has passed its logos (menu 1) to the title (menu 0).
  window.titleReached = async (ms) => (await until(() => dkrState().menu === 1, ms)) && until(() => dkrState().menu === 0, ms);

  const gTrackCheckpoints = 0x8011aecc, gNumberOfMainCheckpoints = 0x8011aed0;
  const keys = { gas: ['Space', ' ', 32], left: ['KeyA', 'a', 65], right: ['KeyD', 'd', 68], brake: ['ShiftLeft', 'Shift', 16] };
  const press = (name, down) => key(down ? 'keydown' : 'keyup', ...keys[name]);
  const wrap = (a) => Math.atan2(Math.sin(a), Math.cos(a));
  // Toward the next checkpoint (the one after it once close), the left or right key held while the kart's heading,
  // read from how it moves (x and z over the last ~0.3 s), is off by more than ~8 degrees. Which way the left key
  // turns is read first, from a moment of it held. A kart that hasn't moved for 2 s backs off with the brake and the
  // other key.
  const drive = async (seconds) => {
    const object = kart(), data = racer();
    const end = Date.now() + seconds * 1000;
    const where = () => [f32(object + 0xc), f32(object + 0x14)];
    const trail = [];
    const heading = () => {
      const [x, z] = where(), now = Date.now();
      trail.push([now, x, z]);
      while (trail.length > 2 && now - trail[1][0] > 300) trail.shift();
      const [, px, pz] = trail[0];
      return Math.hypot(x - px, z - pz) > 5 ? Math.atan2(x - px, z - pz) : null;
    };
    const headingAfter = async (ms) => { let h = null; for (const t = Date.now() + ms; Date.now() < t; await sleep(30)) h = heading() ?? h; return h; };
    press('gas', true);
    const before = await headingAfter(2000);
    press('left', true); await sleep(600); press('left', false);
    const after = await headingAfter(300);
    let leftTurns = (before !== null && after !== null && Math.sign(wrap(after - before))) || 1;
    // Turning the wrong way for every correction leaves the kart pointing straight away from its target, where it
    // stays: 3 s of that and the sign is the other one.
    let awaySince = null, flips = 0;
    let held = null, last = where(), lastMoved = Date.now(), steered = 0;
    const hold = (name) => { if (held !== name) { if (held) press(held, false); if (name) press(name, true); held = name; } };
    while (Date.now() < end) {
      const [x, z] = where(), count = word(gNumberOfMainCheckpoints) | 0;
      if (Math.hypot(x - last[0], z - last[1]) > 40) { last = [x, z]; lastMoved = Date.now(); }
      if (Date.now() - lastMoved > 2000) {
        hold(null); press('gas', false); press('brake', true); press(leftTurns > 0 ? 'right' : 'left', true);
        await sleep(1200);
        press('brake', false); press('left', false); press('right', false); press('gas', true);
        lastMoved = Date.now();
        trail.length = 0;
        continue;
      }
      const h = heading();
      if (count > 0 && h !== null) {
        let next = (rb(data + 0x192) << 24 >> 24) % count; // nextCheckpoint, an s8
        if (next < 0) next += count;
        let c = word(gTrackCheckpoints) + next * 0x3c;
        if (Math.hypot(f32(c + 0x10) - x, f32(c + 0x18) - z) < 150) c = word(gTrackCheckpoints) + ((next + 1) % count) * 0x3c;
        const off = wrap(Math.atan2(f32(c + 0x10) - x, f32(c + 0x18) - z) - h);
        if (Math.abs(off) < 2.5) awaySince = null;
        else if (awaySince === null) awaySince = Date.now();
        else if (Date.now() - awaySince > 3000) { leftTurns = -leftTurns; flips++; awaySince = null; }
        hold(Math.abs(off) < 0.14 ? null : (Math.sign(off) === leftTurns ? 'left' : 'right'));
        if (steered++ % 150 === 0) console.log(`drive: checkpoint ${next} lap ${rb(data + 0x193)} off ${off.toFixed(2)} left turns ${leftTurns}`);
      }
      await sleep(30);
    }
    hold(null);
    press('gas', false);
    return `left turns ${leftTurns} (${before?.toFixed(2)} to ${after?.toFixed(2)}, flipped ${flips} times), ${steered} steers, checkpoint ${rb(data + 0x192)} of ${word(gNumberOfMainCheckpoints) | 0}, lap ${rb(data + 0x193)}`;
  };

  // Round and round with the gas held, steering a while each way: a hub (Timber's Island is where the game starts).
  const roam = window.roam = async (seconds) => {
    key('keydown', 'Space', ' ', 32);
    for (let t = 0; t < seconds; t += 3) {
      const [code, k, kc] = (t / 3) % 4 < 2 ? ['KeyA', 'a', 65] : ['KeyD', 'd', 68];
      key('keydown', code, k, kc); await sleep(1200); key('keyup', code, k, kc); await sleep(1800);
    }
    key('keyup', 'Space', ' ', 32);
    return JSON.stringify(dkrState());
  };

  window.drive = drive;

  window.go = async (map, seconds, how = 'race') => {
    if ((word(gPlayableMapId) | 0) === map) return `map ${map}: already there`; // the teleport to the map it is on breaks it
    if (!await until(() => (word(gGameMode) | 0) === 0 && (word(gLevelLoadTimer) >>> 16) === 0 && !rb(gIsLoading), 20000)) {
      return `not in game: ${JSON.stringify(dkrState())}`;
    }
    await sleep(500);
    // The teleport keeps the vehicle of the map before; the map's own is the low half of byte 2 of its 6 bytes in the
    // level table (leveltable_vehicle_default). gLevelDefaultVehicleID is a Vehicle, 4 bytes.
    n64w(gLevelDefaultVehicleID, rb(word(gGlobalLevelTable) + map * 6 + 2) & 0xf);
    wb(gNextMap, map);
    wb(gLevelLoadType, 4);
    wh(gLevelLoadTimer, 2);
    if (!await until(() => (word(gPlayableMapId) | 0) === map && !rb(gIsLoading) && racer(), 15000)) {
      return `map ${map} never loaded: ${JSON.stringify(dkrState())}`;
    }
    await until(() => (word(gNumRacers) | 0) > 1, 3000); // a race's racers come a moment after the map
    const racers = word(gNumRacers) | 0;
    if (racers > 1) {
      await sleep(6000); // the countdown
      if (how === 'finish') {
        wb(racer() + 0x1d8, 1); // raceFinished
        await sleep((seconds - 6) * 1000);
      } else if (how === 'stand') {
        await sleep((seconds - 6) * 1000);
      } else {
        const driven = await drive(seconds - 6);
        console.log(`drive: ${driven}`);
      }
    } else {
      await roam(seconds);
    }
    return `map ${map}: ${racers} racers, ${JSON.stringify(dkrState())}, heap ${typeof HEAPU8 === 'undefined' ? '?' : HEAPU8.buffer.byteLength >>> 20} MB`;
  };
})();
