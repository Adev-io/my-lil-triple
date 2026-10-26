# My lil Triple — design

VCV Rack 2 plugin by A_Development. Slug `MyLilTriple`, module `Triple`, version 2.0.0.
A polyphonic three-oscillator synth voice inspired by the behavior of FL Studio's 3x Osc, plus amp ADSR and a filter with its own ADSR.

## Provenance

Clean-room. Built from the published 3x Osc manual behavior only
(https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/3x%20Osc.htm).
No decompilation, no Image-Line code, samples or artwork. Same rule as the other My lil modules.

## Features

Per oscillator (x3):
- Wave: sine, triangle, square, saw, rounded saw, noise, custom
- Invert switch
- Phase offset (L and R start at different phases, stereo width)
- Coarse tune +-24 semitones, fine tune +-1 semitone
- Pan
- Mix level (osc 1 has none; osc 2/3 relative; summed level kept constant)

Global (from 3x Osc):
- AM OSC3 switch: osc 3 amplitude-modulates osc 1 + osc 2 (gain = 0.5 x (1 + osc 3); osc 3 is not heard, its pan and mix are unused)
- HQ switch: polyBLEP anti-aliasing on/off

Added (not in 3x Osc, where FL does it on the channel):
- Amp ADSR
- Filter: LP / HP / BP state-variable; cutoff, resonance, env amount, key track
- Filter ADSR (separate)

Ports: V/OCT (poly in), gate (poly in), cutoff CV, L/R audio out (poly).
Context menu: load / clear custom wave (single-cycle WAV). Panel stays simple.
UI: My lil house style (lime title band, role-colored jack rings, house-color lights, Rogan knobs, talking-face mascot driven by output level).

## Voice (x16, one per poly channel)

```
V/OCT + gate
  -> 3 osc (phase accumulator, polyBLEP when HQ) -> AM: osc3 x (osc1+osc2) when on
  -> per-osc invert, constant-power pan, mix (normalized)
  -> stereo sum -> SVF (LP/HP/BP) <- cutoff + filterADSR*amt + keytrack + CV
  -> x ampADSR -> L/R out
```

- Phase offset: each osc has one accumulator; L reads it, R reads it plus the offset.
- Noise: per-voice xorshift, unpitched.
- Custom wave: 2048-sample table, linear interpolation; missing/invalid file falls back to sine.
- ADSR: linear segments (Adsr.h reused from My lil Drums); retrigger starts from the current level (no click); gate low = release; voice idle when amp env finishes, so CPU follows held notes.
- SVF: TPT state-variable; cutoff clamped below Nyquist; resonance capped for stability.
- Poly channels = max(channels of V/OCT, gate); output channels the same.
- State: knobs are normal params; custom wave path stored in module JSON.

## Repo layout

Same as `my-lil-drums`: `core/` (DSP, no Rack dependency), `src/`, `tools/panel.py` -> `layout.hpp`, `res/`, `tests/`, `plugin.json`, `Makefile`.
Face: copy `core/FaceTap.h` + `src/TalkingFace.hpp` from `my-lil-acid-bass-vcv`.

## Tests (`tests/`, plain asserts, no framework)

1. V/OCT 0 gives 261.63 Hz within 0.1 cent (zero-crossing count).
2. Coarse +12 = one octave up; fine +1 = +1 semitone.
3. Inverted osc + identical osc = silence.
4. Saw with HQ on has less energy above 0.9 x Nyquist than HQ off.
5. ADSR hits expected levels at expected times; retrigger has no jump.
6. SVF LP at 1 kHz attenuates an 8 kHz sine > 20 dB; HP the inverse.
7. 16 voices render with no NaN/inf.
8. Pan hard left leaves R silent.

Then: build, install into Ari's Rack, listen via the `rack` MCP (needs Rack running with My lil MCP on :2600).

## Out of scope

Unison/detune stack, wavetable morphing, modulation matrix, MPE, oversampling. Add only on request.
