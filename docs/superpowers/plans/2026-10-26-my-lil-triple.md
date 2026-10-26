# My lil Triple Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `My lil Triple`, a 16-voice polyphonic VCV Rack 2 synth voice modelled on the published behavior of FL Studio's 3x Osc, plus amp ADSR and a filter with its own ADSR.

**Architecture:** All DSP lives in header-only `core/` (no Rack dependency) and is covered by plain-assert tests built with g++. `src/` is a thin Rack shell: a module that reads knobs every 16 samples into a `TripleCfg`, runs one `TripleVoice` per poly channel, and a widget laid out from one table in `tools/panel.py` (panel SVG and `layout.hpp` come from the same source).

**Tech Stack:** C++11, Rack SDK 2.6.x at `C:\dev\Rack-SDK`, MSYS2 mingw64 via `tools/mk.ps1`, Python 3.11 + fontTools for the panel, GitHub Actions copied from my-lil-drums.

**Spec:** `docs/superpowers/specs/2026-10-26-my-lil-triple-design.md`. Repo: `C:\Users\Arikl\dev\active\my-lil-triple` (git already initialised, one commit).

**Ground rules**
- Clean-room: no Image-Line code, samples or artwork; behavior comes from the public 3x Osc manual only.
- Commits are plain prose with **no** `Co-Authored-By` or AI attribution lines (Ari's global rule).
- Run commands from the repo root in PowerShell. `pwsh -NoProfile -File tools/mk.ps1 <target>` runs make in MSYS2 against the Rack SDK; its exit code is make's.
- The core code and tests below were compiled and run green (46 checks) and the plugin DLL was built against the SDK before this plan was written. If a check fails for you, the file differs from the plan: diff it first.

## Spec changes found while planning (applied in Task 1)

| Spec said | Plan does | Why |
|---|---|---|
| ADSR "exponential-ish" | linear segments, `Adsr.h` reused unchanged from my-lil-drums | already tested there, retrigger and zero-length safe |
| Phase offset: two accumulators | one accumulator per osc; L reads it, R reads it + offset | same result, half the state |
| Context menu: "stereo/phase options" | dropped; phase offset is a panel knob | nothing left for a menu to hold |
| AM formula unspecified | gain = 0.5 x (1 + osc 3), osc 3 not heard, its pan and mix unused | true AM keeps the carriers; the manual gives no formula |
| Rounded saw unspecified | ramp over 90% of the cycle, 10% cosine fall | manual gives no formula; ours has no jump at reset |

## File structure

| File | Responsibility |
|---|---|
| `core/Adsr.h` | gate ADSR (copied from my-lil-drums) |
| `core/FaceTap.h` | mascot mouth buffer (copied) |
| `core/Sample.h` | WAV parser (copied) |
| `core/Osc.h` | seven waves, polyBLEP, noise, custom-table read |
| `core/WaveTable.h` | Sample -> one normalised custom cycle |
| `core/Svf.h` | TPT state-variable filter LP/HP/BP |
| `core/TripleVoice.h` | `OscCfg`, `TripleCfg`, `TripleVoice`: 3 osc -> AM/mix/pan -> filter -> amp |
| `tests/test_*.h`, `tests/test.cpp` | plain-assert checks, one header per core file |
| `tools/panel.py` | panel SVG + `src/layout.hpp` from one table |
| `src/plugin.*`, `src/Triple.hpp`, `src/Triple.cpp`, `src/TalkingFace.hpp` | Rack shell |
| `plugin.json`, `Makefile`, `.github/workflows/build.yml`, `README.md` | packaging |

---

### Task 1: Scaffold, shared files, spec amendments

**Files:**
- Create: `Makefile`, `plugin.json`, `.gitignore`, `LICENSE`, `tools/mk.ps1`, `tests/Makefile`, `tests/check.h`, `tests/test_adsr.h`, `tests/test.cpp`, `core/Adsr.h`, `core/FaceTap.h`, `core/Sample.h`
- Modify: `docs/superpowers/specs/2026-10-26-my-lil-triple-design.md`

- [ ] **Step 1: Copy the files that are identical to my-lil-drums**

```powershell
$d = 'C:\Users\Arikl\dev\active\my-lil-drums'; $t = 'C:\Users\Arikl\dev\active\my-lil-triple'
New-Item -ItemType Directory -Force "$t\core","$t\tests","$t\tools","$t\src","$t\res","$t\.github\workflows" | Out-Null
Copy-Item "$d\Makefile","$d\LICENSE","$d\.gitignore" $t
Copy-Item "$d\tools\mk.ps1" "$t\tools"
Copy-Item "$d\core\Adsr.h","$d\core\FaceTap.h","$d\core\Sample.h" "$t\core"
Copy-Item "$d\tests\Makefile","$d\tests\check.h","$d\tests\test_adsr.h" "$t\tests"
Copy-Item "$d\src\TalkingFace.hpp" "$t\src"
Copy-Item "$d\.github\workflows\build.yml" "$t\.github\workflows"
```

- [ ] **Step 2: Write `plugin.json`**

```json
{
  "slug": "MyLilTriple",
  "name": "My lil Triple",
  "version": "2.0.0",
  "license": "MIT",
  "brand": "A_Development",
  "author": "Ari Klopfer",
  "authorEmail": "arik@adevelopment.dev",
  "authorUrl": "https://adevelopment.dev",
  "pluginUrl": "https://github.com/Adev-io/my-lil-triple",
  "manualUrl": "https://github.com/Adev-io/my-lil-triple#readme",
  "sourceUrl": "https://github.com/Adev-io/my-lil-triple",
  "donateUrl": "",
  "changelogUrl": "https://github.com/Adev-io/my-lil-triple/releases",
  "modules": [
    { "slug": "Triple", "name": "Triple", "description": "Polyphonic three-oscillator synth voice: seven waves per osc, AM, HQ, amp ADSR, filter with its own ADSR", "tags": ["Oscillator", "Synth voice", "Polyphonic"] }
  ]
}
```

- [ ] **Step 3: Write `tests/test.cpp` with only the ADSR test for now**

```cpp
// test.cpp: checks the core with plain asserts. Run: make test
// Author: Ari Klopfer / A_Development. MIT
#include "check.h"
#include "test_adsr.h"

int main()
{
    testAdsr();
    std::printf ("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
```

- [ ] **Step 4: Point the CI workflow at this plugin**

In `.github/workflows/build.yml` change the artifact name `MyLilDrums-${{ matrix.name }}` to `MyLilTriple-${{ matrix.name }}`.

```powershell
(Get-Content .github\workflows\build.yml) -replace 'MyLilDrums-', 'MyLilTriple-' | Set-Content .github\workflows\build.yml
Select-String -Path .github\workflows\build.yml -Pattern 'MyLil'
```
Expected: one match, `MyLilTriple-${{ matrix.name }}`.

- [ ] **Step 5: Run the test target**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: nine `ok` lines for the ADSR and the last line `passed: 0 failure(s)`; exit code 0.

- [ ] **Step 6: Amend the spec to match the table above**

Apply these exact replacements in `docs/superpowers/specs/2026-10-26-my-lil-triple-design.md`:

| old | new |
|---|---|
| `- AM OSC3 switch: osc 3 amplitude-modulates osc 1 + osc 2` | `- AM OSC3 switch: osc 3 amplitude-modulates osc 1 + osc 2 (gain = 0.5 x (1 + osc 3); osc 3 is not heard, its pan and mix are unused)` |
| `Context menu: load custom wave (single-cycle WAV), stereo/phase options. Panel stays simple.` | `Context menu: load / clear custom wave (single-cycle WAV). Panel stays simple.` |
| `- Phase offset: each osc has two accumulators (L at 0, R at offset).` | `- Phase offset: each osc has one accumulator; L reads it, R reads it plus the offset.` |
| `- ADSR: exponential-ish segments; retrigger` | `- ADSR: linear segments (Adsr.h reused from My lil Drums); retrigger` |

- [ ] **Step 7: Commit**

```powershell
git add -A
git commit -m "Scaffold the plugin: build files, ADSR, WAV parser, test harness; amend the spec"
```

---

### Task 2: Oscillators and custom wave table

**Files:**
- Create: `core/Osc.h`, `core/WaveTable.h`, `tests/test_osc.h`
- Modify: `tests/test.cpp`

- [ ] **Step 1: Write the failing test `tests/test_osc.h`**

```cpp
// test_osc.h: wave shapes, polyBLEP, noise, custom table.
#pragma once
#include "check.h"
#include "Osc.h"
#include "WaveTable.h"

// frequency of a sine from its first to last rising zero crossing (sub-sample interpolated)
inline double measureHz (const std::vector<float>& x, double sr)
{
    double first = 0, last = 0;
    int n = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.f && x[i] >= 0.f)
        {
            const double t = (double) (i - 1) + (0.0 - x[i - 1]) / (x[i] - x[i - 1]);
            if (n++ == 0) first = t;
            last = t;
        }
    return n > 1 ? (n - 1) * sr / (last - first) : 0.0;
}

// magnitude of one frequency in x
inline double goertzel (const std::vector<float>& x, double hz, double sr)
{
    const double c = 2.0 * std::cos (6.283185307179586 * hz / sr);
    double s0 = 0, s1 = 0, s2 = 0;
    for (float v : x) { s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / x.size();
}

inline std::vector<float> renderOsc (int wave, double hz, double sr, int n, bool hq, const std::vector<float>* tbl = nullptr)
{
    std::vector<float> out (n);
    uint32_t rng = 1;
    for (int i = 0; i < n; ++i)
    {
        double p = i * hz / sr;
        p -= std::floor (p);   // exact phase, so the spectrum has no smear
        out[i] = oscSample (wave, (float) p, (float) (hz / sr), hq, rng, tbl);
    }
    return out;
}

inline void testOsc()
{
    uint32_t rng = 1;
    std::vector<float> sine = renderOsc (W_SINE, 1000.0, 48000.0, 48000, true);
    check (near (measureHz (sine, 48000.0), 1000.0, 0.01), "sine runs at the asked frequency", measureHz (sine, 48000.0));
    check (near (oscSample (W_TRI, 0.f, 0.01f, true, rng, nullptr), 0.0, 1e-6), "triangle starts at 0");
    check (near (oscSample (W_TRI, 0.25f, 0.01f, true, rng, nullptr), 1.0, 1e-6), "triangle peaks at a quarter cycle");

    bool inRange = true;
    for (int w : { W_SINE, W_TRI, W_SQUARE, W_SAW, W_RSAW })
        for (float v : renderOsc (w, 1234.0, 48000.0, 4800, true))
            if (std::fabs (v) > 1.05f) inRange = false;
    check (inRange, "every wave stays inside +-1.05 with HQ on");

    // HQ: a 7 kHz saw's 4th harmonic (28 kHz) folds to 20 kHz; polyBLEP must cut that alias well below the raw saw's
    const double raw = goertzel (renderOsc (W_SAW, 7000.0, 48000.0, 48000, false), 20000.0, 48000.0);
    const double blep = goertzel (renderOsc (W_SAW, 7000.0, 48000.0, 48000, true), 20000.0, 48000.0);
    check (blep < 0.7 * raw, "HQ cuts a saw's folded harmonic at 20 kHz", blep / raw);
    const double sraw = goertzel (renderOsc (W_SQUARE, 7000.0, 48000.0, 48000, false), 20000.0, 48000.0);
    const double sblep = goertzel (renderOsc (W_SQUARE, 7000.0, 48000.0, 48000, true), 20000.0, 48000.0);
    check (sblep < 0.7 * sraw, "HQ cuts a square's folded harmonic at 20 kHz", sblep / sraw);

    std::vector<float> rs = renderOsc (W_RSAW, 100.0, 48000.0, 480, true);
    float step = 0.f;
    for (size_t i = 1; i < rs.size(); ++i) step = std::max (step, std::fabs (rs[i] - rs[i - 1]));
    check (step < 0.1f, "rounded saw has no jump at its reset (a raw saw steps by 2)", step);

    double mean = 0, lo = 1, hi = -1;
    for (int i = 0; i < 100000; ++i) { const float v = noise (rng); mean += v; lo = std::min<double> (lo, v); hi = std::max<double> (hi, v); }
    check (std::fabs (mean / 100000) < 0.02 && lo >= -1.0 && hi <= 1.0 && hi - lo > 1.9, "noise is centred, full range, inside +-1", mean / 100000);

    check (near (oscSample (W_CUSTOM, 0.3f, 0.01f, true, rng, nullptr), std::sin (PI2 * 0.3f), 1e-6), "custom with no table falls back to sine");
    std::vector<float> ramp (TABLE_N);
    for (int i = 0; i < TABLE_N; ++i) ramp[i] = (float) i / TABLE_N;
    check (near (oscSample (W_CUSTOM, 0.25f, 0.01f, true, rng, &ramp), 0.25, 1e-4), "custom reads its table", oscSample (W_CUSTOM, 0.25f, 0.01f, true, rng, &ramp));
    std::vector<float> cyc (TABLE_N);
    for (int i = 0; i < TABLE_N; ++i) cyc[i] = std::sin (PI2 * i / TABLE_N);
    check (near (oscSample (W_CUSTOM, 0.99999f, 0.01f, true, rng, &cyc), 0.0, 1e-3), "custom interpolates across the wrap", oscSample (W_CUSTOM, 0.99999f, 0.01f, true, rng, &cyc));
    check (near (oscSample (W_CUSTOM, 0.3f, 0.01f, true, rng, &cyc), std::sin (PI2 * 0.3f), 1e-3), "custom table of a sine reads as a sine");

    Sample s;
    s.l.assign (100, 0.f);
    for (int i = 0; i < 100; ++i) s.l[i] = 0.1f * i;
    std::vector<float> t;
    check (makeTable (s, t) && (int) t.size() == TABLE_N, "a sample becomes a TABLE_N cycle");
    float peak = 0.f;
    for (float v : t) peak = std::max (peak, std::fabs (v));
    check (near (peak, 1.0, 1e-5), "the table is normalised to peak 1", peak);
    Sample silent;
    silent.l.assign (100, 0.f);
    check (! makeTable (silent, t) && ! makeTable (Sample(), t), "silent and empty samples are refused");
}
```

- [ ] **Step 2: Register it in `tests/test.cpp`**

Add `#include "test_osc.h"` after the `test_adsr.h` include and `testOsc();` after `testAdsr();`.

- [ ] **Step 3: Run it and watch it fail**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: FAIL to compile, `fatal error: Osc.h: No such file or directory`; non-zero exit.

- [ ] **Step 4: Write `core/Osc.h`**

```cpp
// Osc.h: one oscillator sample from a phase, for the seven 3x Osc style waves. No Rack dependency.
// HQ = polyBLEP on saw and square (3x Osc's anti-alias switch); off gives the raw, aliased shape.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

enum Wave { W_SINE, W_TRI, W_SQUARE, W_SAW, W_RSAW, W_NOISE, W_CUSTOM, W_N };

constexpr float PI2 = 6.2831853f;
constexpr int TABLE_N = 2048;   // samples in one custom-wave cycle

inline float frac (float x) { return x - std::floor (x); }

// two-sample polynomial band-limited step; t = phase 0..1, dt = phase step per sample
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float noise (uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;   // xorshift32
    return (int32_t) s * (1.f / 2147483648.f);
}

// ph 0..1 is the wave's phase, dt the phase step per sample (for polyBLEP), tbl the custom cycle (TABLE_N floats) or null.
// Rounded saw: a ramp with its reset smoothed into a 10% cosine fall (the manual gives no formula; this is our reading).
inline float oscSample (int w, float ph, float dt, bool hq, uint32_t& rng, const std::vector<float>* tbl)
{
    switch (w)
    {
        case W_TRI: { const float p = frac (ph + 0.25f); return 1.f - 2.f * std::fabs (2.f * p - 1.f); }
        case W_SQUARE:
        {
            float s = ph < 0.5f ? 1.f : -1.f;
            if (hq) s += polyBlep (ph, dt) - polyBlep (frac (ph + 0.5f), dt);
            return s;
        }
        case W_SAW:
        {
            float s = 2.f * ph - 1.f;
            if (hq) s -= polyBlep (ph, dt);
            return s;
        }
        case W_RSAW:
        {
            if (ph < 0.9f) return -1.f + 2.f * ph / 0.9f;
            return std::cos (3.14159265f * (ph - 0.9f) / 0.1f);   // 1 -> -1
        }
        case W_NOISE: return noise (rng);
        case W_CUSTOM:
        {
            if (! tbl || (int) tbl->size() != TABLE_N) break;   // no wave loaded: sine
            const float x = ph * TABLE_N;
            const int i = std::min ((int) x, TABLE_N - 1);
            const float f = x - i;
            const float a = (*tbl)[i], b = (*tbl)[(i + 1) % TABLE_N];
            return a + (b - a) * f;
        }
        default: break;
    }
    return std::sin (PI2 * ph);
}
```

- [ ] **Step 5: Write `core/WaveTable.h`**

```cpp
// WaveTable.h: turns a loaded Sample into one custom-wave cycle. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Osc.h"
#include "Sample.h"

// Resamples the left channel to TABLE_N points (linear) and normalises to peak 1. False for an empty or silent sample.
inline bool makeTable (const Sample& s, std::vector<float>& out)
{
    const int n = s.size();
    if (n < 2) return false;
    out.assign (TABLE_N, 0.f);
    float peak = 0.f;
    for (int i = 0; i < TABLE_N; ++i)
    {
        const float x = (float) i * n / TABLE_N;
        const int a = std::min ((int) x, n - 1), b = (a + 1) % n;   // the last point wraps to the first: it is a cycle
        out[i] = s.l[a] + (s.l[b] - s.l[a]) * (x - a);
        peak = std::max (peak, std::fabs (out[i]));
    }
    if (peak < 1e-6f) return false;
    for (float& v : out) v /= peak;
    return true;
}
```

- [ ] **Step 6: Run and watch it pass**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: all `ok`, including `HQ cuts a saw's folded harmonic at 20 kHz  (0.2746)` (ratio is well under 0.7) and last line `passed: 0 failure(s)`.

- [ ] **Step 7: Commit**

```powershell
git add core/Osc.h core/WaveTable.h tests
git commit -m "Add the seven oscillator waves with polyBLEP and the custom wave table"
```

---

### Task 3: State-variable filter

**Files:**
- Create: `core/Svf.h`, `tests/test_svf.h`
- Modify: `tests/test.cpp`

- [ ] **Step 1: Write the failing test `tests/test_svf.h`**

```cpp
// test_svf.h: the state-variable filter.
#pragma once
#include "check.h"
#include "Svf.h"

// peak of the filter's output for a sine at hz, after it has settled
inline float svfGain (int type, float fc, float res, float hz)
{
    Svf s;
    const float sr = 48000.f;
    const SvfCoef k = svfCoef (fc, res, sr);
    float peak = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = s.process ((float) std::sin (6.283185307179586 * hz * i / sr), k, type);
        if (i > 24000) peak = std::max (peak, std::fabs (y));
    }
    return peak;
}

inline void testSvf()
{
    check (svfGain (F_LP, 1000.f, 0.f, 8000.f) < 0.1f, "low-pass at 1 kHz cuts 8 kHz by > 20 dB", svfGain (F_LP, 1000.f, 0.f, 8000.f));
    check (svfGain (F_LP, 1000.f, 0.f, 100.f) > 0.95f, "low-pass at 1 kHz passes 100 Hz", svfGain (F_LP, 1000.f, 0.f, 100.f));
    check (svfGain (F_HP, 8000.f, 0.f, 1000.f) < 0.1f, "high-pass at 8 kHz cuts 1 kHz by > 20 dB", svfGain (F_HP, 8000.f, 0.f, 1000.f));
    check (svfGain (F_HP, 1000.f, 0.f, 10000.f) > 0.95f, "high-pass at 1 kHz passes 10 kHz", svfGain (F_HP, 1000.f, 0.f, 10000.f));
    check (near (svfGain (F_BP, 1000.f, 0.f, 1000.f), 0.7071, 0.05), "band-pass peaks at its centre", svfGain (F_BP, 1000.f, 0.f, 1000.f));
    check (svfGain (F_BP, 1000.f, 0.f, 8000.f) < 0.2f, "band-pass rejects far above its centre", svfGain (F_BP, 1000.f, 0.f, 8000.f));
    check (svfGain (F_LP, 1000.f, 1.f, 1000.f) > 5.f, "full resonance rings at the cutoff", svfGain (F_LP, 1000.f, 1.f, 1000.f));

    Svf s;
    float y = 0.f;
    for (int i = 0; i < 2000; ++i) y = s.process (i & 1 ? 1.f : -1.f, svfCoef (1e9f, 1.f, 48000.f), F_LP);
    check (std::isfinite (y), "extreme cutoff and full resonance stay finite", y);
    for (int i = 0; i < 2000; ++i) y = s.process (i & 1 ? 1.f : -1.f, svfCoef (-5.f, 0.f, 48000.f), F_HP);
    check (std::isfinite (y), "negative cutoff stays finite", y);
}
```

- [ ] **Step 2: Register it in `tests/test.cpp`**

Add `#include "test_svf.h"` and `testSvf();` after the osc lines.

- [ ] **Step 3: Run and watch it fail**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: compile error `Svf.h: No such file or directory`.

- [ ] **Step 4: Write `core/Svf.h`**

```cpp
// Svf.h: TPT state-variable filter (12 dB/oct LP / HP / BP), one channel. No Rack dependency.
// The coefficients are shared by both channels of a voice, so they are computed once.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>

enum FilterType { F_LP, F_HP, F_BP, F_N };

struct SvfCoef { float g, R; };

// fc in Hz (clamped to 20 Hz .. 0.45 sr), res 0..1: 0 = Butterworth, 1 = Q 20 (stable, near self-oscillation)
inline SvfCoef svfCoef (float fc, float res, float sr)
{
    fc = std::max (20.f, std::min (fc, 0.45f * sr));
    const float Q = 0.7071f + 19.3f * res * res;
    return { std::tan (3.14159265f * fc / sr), 1.f / (2.f * Q) };
}

struct Svf
{
    float s1 = 0.f, s2 = 0.f;

    float process (float x, const SvfCoef& k, int type)
    {
        const float hp = (x - (2.f * k.R + k.g) * s1 - s2) / (1.f + 2.f * k.R * k.g + k.g * k.g);
        const float bp = k.g * hp + s1;
        s1 = k.g * hp + bp;
        const float lp = k.g * bp + s2;
        s2 = k.g * bp + lp;
        return type == F_HP ? hp : type == F_BP ? bp : lp;
    }
};
```

- [ ] **Step 5: Run and watch it pass**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: all `ok` (low-pass 8 kHz gain about 0.012, band-pass centre 0.7071, full resonance about 20) and `passed: 0 failure(s)`.

- [ ] **Step 6: Commit**

```powershell
git add core/Svf.h tests
git commit -m "Add the TPT state-variable filter (LP, HP, BP)"
```

---

### Task 4: The voice

**Files:**
- Create: `core/TripleVoice.h`, `tests/test_voice.h`
- Modify: `tests/test.cpp`

- [ ] **Step 1: Write the failing test `tests/test_voice.h`**

```cpp
// test_voice.h: the voice end to end.
#pragma once
#include "check.h"
#include "TripleVoice.h"
#include "test_osc.h"

// one clean sine on osc 1 (the other two silent), filter open
inline TripleCfg sineCfg()
{
    TripleCfg c;
    for (int i = 0; i < 3; ++i) c.osc[i].wave = W_SINE;
    c.osc[1].mix = 0.f;
    c.osc[2].mix = 0.f;
    return c;
}

inline void renderVoice (const TripleCfg& c, float voct, int n, std::vector<float>& l, std::vector<float>& r)
{
    TripleVoice v;
    l.assign (n, 0.f);
    r.assign (n, 0.f);
    for (int i = 0; i < n; ++i) v.process (c, voct, true, 0.f, 48000.f, l[i], r[i]);
}

inline double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
inline double tailHz (const std::vector<float>& x) { return measureHz (std::vector<float> (x.begin() + 4800, x.end()), 48000.0); }
inline double peakOf (const std::vector<float>& x, int from = 4800)
{
    double p = 0;
    for (size_t i = from; i < x.size(); ++i) p = std::max<double> (p, std::fabs (x[i]));
    return p;
}
inline double rmsOf (const std::vector<float>& x, int from = 4800)
{
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (x.size() - from));
}
inline double rmsDiff (const std::vector<float>& x, int from = 4800)
{
    double s = 0;
    for (size_t i = from + 1; i < x.size(); ++i) s += (double) (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
    return std::sqrt (s / (x.size() - from));
}

inline void testVoice()
{
    std::vector<float> l, r;
    TripleCfg c = sineCfg();
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 261.6256)) < 0.1, "0 V is C4, 261.63 Hz, within 0.1 cent", tailHz (l));
    renderVoice (c, 1.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 523.2511)) < 0.1, "1 V is one octave up", tailHz (l));

    c.osc[0].coarse = 12.f;
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 523.2511)) < 0.1, "coarse +12 is one octave up", tailHz (l));
    c.osc[0].coarse = 0.f;
    c.osc[0].fine = 1.f;
    renderVoice (c, 0.f, 96000, l, r);
    check (std::fabs (cents (tailHz (l), 261.6256 * std::pow (2.0, 1.0 / 12))) < 0.1, "fine +1 is one semitone up", tailHz (l));

    // inverted copy cancels
    c = sineCfg();
    c.osc[1].mix = 1.f;
    c.osc[1].invert = true;
    renderVoice (c, 0.f, 24000, l, r);
    check (peakOf (l) < 1e-4 && peakOf (r) < 1e-4, "an inverted copy of the same osc is silence", peakOf (l));

    // three identical oscs keep the level of one
    c = sineCfg();
    renderVoice (c, 0.f, 24000, l, r);
    const double one = peakOf (l);
    c.osc[1].mix = 1.f; c.osc[2].mix = 1.f;
    renderVoice (c, 0.f, 24000, l, r);
    check (near (peakOf (l), one, one * 0.02), "relative mixes keep one overall level", peakOf (l) / one);

    // hard-left pan
    c = sineCfg();
    c.osc[0].pan = -1.f;
    renderVoice (c, 0.f, 24000, l, r);
    check (peakOf (r) < 1e-6 && peakOf (l) > 0.5, "pan hard left leaves R silent", peakOf (r));

    // phase offset: R starts a quarter cycle ahead, so at the first sample R is far from L
    c = sineCfg();
    c.osc[0].phase = 0.25f;
    c.ampA = 0.0001f;
    renderVoice (c, 0.f, 24000, l, r);
    double diff = 0;
    for (int i = 4800; i < 4900; ++i) diff = std::max<double> (diff, std::fabs (l[i] - r[i]));
    check (diff > 0.3, "phase offset puts R out of step with L", diff);

    // AM: osc 3 as a 65 Hz square gates osc 1 on and off, so the level drops to about 1/sqrt(2)
    c = sineCfg();
    c.osc[2].wave = W_SQUARE;
    c.osc[2].coarse = -24.f;
    c.osc[2].mix = 0.f;
    renderVoice (c, 0.f, 96000, l, r);
    const double plain = rmsOf (l);
    c.am = true;
    renderVoice (c, 0.f, 96000, l, r);
    check (near (rmsOf (l) / plain, 0.7071, 0.1), "AM OSC3 gates osc 1 and 2", rmsOf (l) / plain);

    // filter envelope opens a 200 Hz low-pass on a saw; keytrack lifts it with pitch
    c = sineCfg();
    c.osc[0].wave = W_SAW;
    c.cutoff = 200.f;
    renderVoice (c, 0.f, 48000, l, r);
    const double closed = rmsDiff (l);
    c.envAmt = 1.f;
    renderVoice (c, 0.f, 48000, l, r);
    check (rmsDiff (l) > 3.0 * closed, "filter envelope amount brightens the saw", rmsDiff (l) / closed);
    c.envAmt = 0.f;
    renderVoice (c, 2.f, 48000, l, r);
    const double lowTrack = rmsDiff (l);
    c.keytrack = 1.f;
    renderVoice (c, 2.f, 48000, l, r);
    check (rmsDiff (l) > 2.0 * lowTrack, "keytrack opens the filter for high notes", rmsDiff (l) / lowTrack);

    // release: gate off lets the voice fade and go idle
    c = sineCfg();
    c.ampR = 0.05f;
    TripleVoice v;
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 4800; ++i) v.process (c, 0.f, true, 0.f, 48000.f, a, b);
    check (v.active(), "voice is active while the gate is high");
    for (int i = 0; i < 3300; ++i) v.process (c, 0.f, false, 0.f, 48000.f, a, b);
    check (! v.active() && a == 0.f, "voice goes idle after the release");

    // 16 voices, every wave, both HQ settings, harsh filter: nothing blows up
    bool finite = true;
    double worst = 0;
    for (int n = 0; n < 16; ++n)
    {
        TripleCfg h;
        for (int i = 0; i < 3; ++i) { h.osc[i].wave = (n + i) % W_N; h.osc[i].mix = 1.f; h.osc[i].pan = (n % 3 - 1) * 0.9f; h.osc[i].phase = 0.1f * i; }
        h.hq = n & 1;
        h.am = (n % 5) == 0;
        h.ftype = n % F_N;
        h.res = 1.f;
        h.cutoff = 20.f * std::pow (1000.f, n / 15.f);
        h.envAmt = (n % 2) ? 1.f : -1.f;
        h.keytrack = 1.f;
        TripleVoice w;
        for (int i = 0; i < 4800; ++i)
        {
            w.process (h, (n - 8) * 0.4f, i < 3000, 0.f, 48000.f, a, b);
            if (! std::isfinite (a) || ! std::isfinite (b)) finite = false;
            worst = std::max<double> (worst, std::max (std::fabs (a), std::fabs (b)));
        }
    }
    check (finite && worst < 200.0, "16 harsh voices stay finite and bounded", worst);
}
```

- [ ] **Step 2: Make `tests/test.cpp` its final form**

Replace the whole file with:

```cpp
// test.cpp: checks the core with plain asserts. Run: make test
// Author: Ari Klopfer / A_Development. MIT
#include "check.h"
#include "test_adsr.h"
#include "test_osc.h"
#include "test_svf.h"
#include "test_voice.h"

int main()
{
    testAdsr();
    testOsc();
    testSvf();
    testVoice();
    std::printf ("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
```

- [ ] **Step 3: Run and watch it fail**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: compile error `TripleVoice.h: No such file or directory`.

- [ ] **Step 4: Write `core/TripleVoice.h`**

```cpp
// TripleVoice.h: one voice of My lil Triple: three oscillators -> stereo mix -> filter -> amp. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Adsr.h"
#include "Osc.h"
#include "Svf.h"

struct OscCfg
{
    int wave = W_SAW;
    bool invert = false;
    float phase = 0.f;                       // R channel starts this far (0..1 of a cycle) ahead of L
    float coarse = 0.f, fine = 0.f;          // semitones: +-24, +-1
    float pan = 0.f;                         // -1 left .. 1 right, constant power
    float mix = 1.f;                         // 0..1, relative to osc 1 (osc 1 is always 1)
};

struct TripleCfg
{
    OscCfg osc[3];
    bool am = false;                         // osc 3 amplitude-modulates osc 1 + 2 instead of being heard
    bool hq = true;                          // polyBLEP anti-aliasing
    float ampA = 0.001f, ampD = 0.2f, ampS = 1.f, ampR = 0.1f;
    int ftype = F_LP;
    float cutoff = 20000.f, res = 0.f;       // Hz, 0..1
    float envAmt = 0.f;                      // -1..1 = -/+ 6 octaves of filter envelope
    float keytrack = 0.f;                    // 0..1: cutoff follows pitch
    float fltA = 0.001f, fltD = 0.2f, fltS = 1.f, fltR = 0.1f;
    const std::vector<float>* table = nullptr;   // custom wave, TABLE_N floats, or null
};

struct TripleVoice
{
    float ph[3] = {};
    uint32_t rng = 0x9E3779B9u;
    Adsr amp, flt;
    Svf fl, fr;
    bool gateWas = false;

    bool active() const { return amp.active(); }

    // voct = volts, 0 = C4. cutCv = octaves added to the cutoff. l, r are about +-1 before the module's 5 V scale.
    void process (const TripleCfg& c, float voct, bool gate, float cutCv, float sr, float& l, float& r)
    {
        amp.a = c.ampA; amp.d = c.ampD; amp.s = c.ampS; amp.r = c.ampR;   // before gate(): the release rate reads r
        flt.a = c.fltA; flt.d = c.fltD; flt.s = c.fltS; flt.r = c.fltR;
        if (gate && ! gateWas)
        {
            if (! amp.active()) { fl = Svf(); fr = Svf(); }
            ph[0] = ph[1] = ph[2] = 0.f;
            amp.gate (true);
            flt.gate (true);
        }
        else if (! gate && gateWas)
        {
            amp.gate (false);
            flt.gate (false);
        }
        gateWas = gate;
        l = r = 0.f;
        if (! amp.active()) return;

        const float dt = 1.f / sr, a = amp.next (dt), fe = flt.next (dt);
        float oL[3], oR[3], gl[3], gr[3];
        for (int i = 0; i < 3; ++i)
        {
            const OscCfg& o = c.osc[i];
            const float inc = 261.6256f * std::exp2 (voct + (o.coarse + o.fine) / 12.f) * dt;
            const float bdt = std::min (inc, 0.45f);
            oL[i] = oscSample (o.wave, ph[i], bdt, c.hq, rng, c.table);
            oR[i] = oscSample (o.wave, frac (ph[i] + o.phase), bdt, c.hq, rng, c.table);
            if (o.invert) { oL[i] = -oL[i]; oR[i] = -oR[i]; }
            const float t = (o.pan + 1.f) * 0.78539816f;   // constant power: centre is -3 dB, hard left has R exactly 0
            gl[i] = std::cos (t);
            gr[i] = std::sin (t);
            ph[i] = frac (ph[i] + inc);
        }

        const float w0 = 1.f, w1 = c.osc[1].mix, w2 = c.osc[2].mix;
        float sl, sr2;
        if (c.am)
        {
            const float n = 1.f / (w0 + w1);   // osc 3's pan and mix are not used: it only modulates
            sl = (oL[0] * gl[0] * w0 + oL[1] * gl[1] * w1) * n * 0.5f * (1.f + oL[2]);
            sr2 = (oR[0] * gr[0] * w0 + oR[1] * gr[1] * w1) * n * 0.5f * (1.f + oR[2]);
        }
        else
        {
            const float n = 1.f / (w0 + w1 + w2);   // relative mixes: the sum keeps one level
            sl = (oL[0] * gl[0] * w0 + oL[1] * gl[1] * w1 + oL[2] * gl[2] * w2) * n;
            sr2 = (oR[0] * gr[0] * w0 + oR[1] * gr[1] * w1 + oR[2] * gr[2] * w2) * n;
        }

        const float fc = c.cutoff * std::exp2 (c.envAmt * 6.f * fe + c.keytrack * voct + cutCv);
        const SvfCoef k = svfCoef (fc, c.res, sr);
        l = fl.process (sl, k, c.ftype) * a;
        r = fr.process (sr2, k, c.ftype) * a;
    }
};
```

- [ ] **Step 5: Run and watch it pass**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: every check `ok` and `passed: 0 failure(s)`. Spot values: `0 V is C4 ... (261.626)`, `AM OSC3 gates osc 1 and 2  (0.706...)`, `16 harsh voices stay finite and bounded  (9.6...)`.

- [ ] **Step 6: Commit**

```powershell
git add core/TripleVoice.h tests
git commit -m "Add the voice: three oscillators, AM, mix and pan, filter, two envelopes"
```

---

### Task 5: Panel generator

**Files:**
- Create: `tools/panel.py`, then generated `res/Triple.svg`, `src/layout.hpp`

- [ ] **Step 1: Write `tools/panel.py`**

```python
#!/usr/bin/env python3
"""panel.py: writes res/Triple.svg and src/layout.hpp from one layout table.

Labels become paths (DejaVu Sans from the Rack install, or the font named by RACK_FONT), so the SVG has no <text>.
The widgets read the same table through layout.hpp, so art and components cannot drift.
House style as in My lil Drums / Looper / Acid Bass. Run from anywhere: py -3.11 tools/panel.py   (needs fontTools).
Author: Ari Klopfer / A_Development. MIT
"""
import math
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

H = 128.5
W = 5.08 * 40
BAND = 8.6
INK, INK2 = '#16161a', '#202027'
LIME, MAGENTA, BLUE, WHITE, DIM = '#c8ff3a', '#ff3d9a', '#5ac8e6', '#f2f2f4', '#8d8d96'
FONT = os.environ.get('RACK_FONT', r'C:\Program Files\VCV\Rack2Free\res\fonts\DejaVuSans.ttf')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RADIUS = {'knob': 6.7, 'button': 3.1, 'seq': 4.0, 'play': 4.0, 'cv': 4.0, 'out': 4.0}
LABEL_SIZE = {'knob': 2.4, 'button': 2.0, 'seq': 2.4, 'play': 2.4, 'cv': 2.4, 'out': 2.2}
RING_COLOR = {'seq': BLUE, 'play': LIME, 'cv': MAGENTA}
PAD_R = 4.8
RING_R = RADIUS['cv'] + 0.7

font = TTFont(FONT)
glyphs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font['head'].unitsPerEm


def width(s, size):
    return sum(glyphs[cmap[ord(c)]].width for c in s) * size / upm


def text(s, x, y, size, fill, anchor='middle', stroke=0.0):
    """One <path> for the string: baseline at y, em size in mm, optional stroke for a bolder face."""
    k = size / upm
    w = width(s, size)
    pen_x = x - w / 2 if anchor == 'middle' else x - w if anchor == 'end' else x
    d = []
    for c in s:
        g = glyphs[cmap[ord(c)]]
        pen = SVGPathPen(glyphs)
        g.draw(TransformPen(pen, (k, 0, 0, -k, pen_x, y)))
        d.append(pen.getCommands())
        pen_x += g.width * k
    bold = ' stroke="%s" stroke-width="%g" stroke-linejoin="round"' % (fill, stroke) if stroke else ''
    return '<path fill="%s"%s d="%s"/>' % (fill, bold, ' '.join(d))


TITLE, TITLE_X, TITLE_Y, TITLE_SIZE = 'TRIPLE', 11.2, 6.3, 4.3
MASCOT_X = W - 14.46
MASCOT_R = BAND * 0.36


def mascot(cx, cy, r):
    e = r * 0.37
    return ''.join([
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx, cy, r, MAGENTA),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx - e, cy - r * 0.32, r * 0.145, INK),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx + e, cy - r * 0.32, r * 0.145, INK),
        '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g" stroke-linecap="round"/>'
        % (cx - r * 0.58, cy + r * 0.45, cx + r * 0.58, cy + r * 0.45, INK, r * 0.16),
    ])


# ---- components: (name, kind, x, y, label). Twelve columns, three rows, jacks below.
CX = [14.0 + 16.0 * i for i in range(12)]
ROW_A, ROW_B, ROW_C, JACK_Y = 26.0, 50.0, 74.0, 108.0
OSC_ROWS = [ROW_A, ROW_B, ROW_C]
OSC_PARTS = [('WAVE', 'knob'), ('INV', 'button'), ('PHASE', 'knob'), ('COARSE', 'knob'), ('FINE', 'knob'), ('PAN', 'knob'), ('MIX', 'knob')]

LAYOUT = []
for o in range(3):
    for j, (label, kind) in enumerate(OSC_PARTS):
        if o == 0 and label == 'MIX':
            continue   # osc 1 has no mix: it is the reference level
        LAYOUT.append(('OSC%d_%d' % (o, j), kind, CX[j], OSC_ROWS[o], label))
LAYOUT += [
    ('AM_PARAM', 'button', CX[6], ROW_A, 'AM OSC3'), ('HQ_PARAM', 'button', CX[7], ROW_A, 'HQ'),
    ('A_ATK_PARAM', 'knob', CX[8], ROW_A, 'ATK'), ('A_DEC_PARAM', 'knob', CX[9], ROW_A, 'DEC'),
    ('A_SUS_PARAM', 'knob', CX[10], ROW_A, 'SUS'), ('A_REL_PARAM', 'knob', CX[11], ROW_A, 'REL'),
    ('FTYPE_PARAM', 'knob', CX[7], ROW_B, 'TYPE'), ('CUTOFF_PARAM', 'knob', CX[8], ROW_B, 'CUTOFF'),
    ('RES_PARAM', 'knob', CX[9], ROW_B, 'RES'), ('ENVAMT_PARAM', 'knob', CX[10], ROW_B, 'ENV'),
    ('KEYTRK_PARAM', 'knob', CX[11], ROW_B, 'KEY'),
    ('F_ATK_PARAM', 'knob', CX[8], ROW_C, 'ATK'), ('F_DEC_PARAM', 'knob', CX[9], ROW_C, 'DEC'),
    ('F_SUS_PARAM', 'knob', CX[10], ROW_C, 'SUS'), ('F_REL_PARAM', 'knob', CX[11], ROW_C, 'REL'),
    ('VOCT_INPUT', 'play', CX[0], JACK_Y, 'V/OCT'), ('GATE_INPUT', 'play', CX[1], JACK_Y, 'GATE'),
    ('CUTCV_INPUT', 'cv', CX[2], JACK_Y, 'CUTOFF'),
    ('OUT_L_OUTPUT', 'out', CX[10], JACK_Y, 'OUT L'), ('OUT_R_OUTPUT', 'out', CX[11], JACK_Y, 'OUT R'),
]
CAPTIONS = [('OSC 1', CX[0], ROW_A), ('OSC 2', CX[0], ROW_B), ('OSC 3', CX[0], ROW_C),
            ('AMP ENV', CX[8], ROW_A), ('FILTER', CX[8], ROW_B), ('FILTER ENV', CX[8], ROW_C)]


def reach(kind):
    return RING_R if kind in RING_COLOR else PAD_R if kind == 'out' else RADIUS[kind]


def label_base(kind, y):
    return y + reach(kind) + (2.6 if kind != 'knob' else 3.2)


def problems():
    """Parts must not overlap each other, their labels, the band, the screws or the panel edge. Must be empty."""
    bad = []
    parts = [(n, x, y, reach(k)) for n, k, x, y, _ in LAYOUT]
    for j, (a, ax, ay, ar) in enumerate(parts):
        if ax - ar < 0 or ax + ar > W or ay - ar < BAND or ay + ar > H:
            bad.append('%s leaves the panel or enters the band' % a)
        for b, bx, by, br in parts[j + 1:]:
            if math.hypot(ax - bx, ay - by) < ar + br:
                bad.append('%s touches %s' % (a, b))
        for sx in (5.08, W - 10.16):
            for sy in (0.0, H - 5.08):
                if sx < ax + ar and ax - ar < sx + 5.08 and sy < ay + ar and ay - ar < sy + 5.08:
                    bad.append('%s under a screw' % a)
    for n, k, x, y, lab in LAYOUT:   # a label must clear every other part
        w, base, size = width(lab, LABEL_SIZE[k]), label_base(k, y), LABEL_SIZE[k]
        box = (x - w / 2, base - 0.73 * size, x + w / 2, base)
        for m, mx, my, mr in parts:
            if m != n:
                nx, ny = min(max(mx, box[0]), box[2]), min(max(my, box[1]), box[3])
                if math.hypot(mx - nx, my - ny) < mr:
                    bad.append('%s label covers %s' % (n, m))
        if box[2] > W or box[3] > H:
            bad.append('%s label leaves the panel' % n)
    return bad


def panel():
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, H, INK),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, BAND, LIME),
        text(TITLE, TITLE_X, TITLE_Y, TITLE_SIZE, INK, 'start', 0.39), mascot(MASCOT_X, BAND / 2, MASCOT_R),
        text('my lil triple', W / 2, 12.6, 2.4, WHITE, stroke=0.12),
        '<rect x="2.5" y="94" width="%g" height="32" rx="2" fill="%s"/>' % (W - 5, INK2),
    ]
    for cap, x, y in CAPTIONS:
        out.append(text(cap, x - RADIUS['knob'], y - RADIUS['knob'] - 1.7, 2.2, DIM, 'start'))
    for _, kind, x, y, label in LAYOUT:
        if kind in RING_COLOR:
            out.append('<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="0.5"/>' % (x, y, RING_R, RING_COLOR[kind]))
        elif kind == 'out':
            out.append('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s" stroke-width="0.5"/>' % (x, y, PAD_R, INK, LIME))
        colour, stroke = (LIME, 0.2) if kind == 'out' else (WHITE, 0.15)
        out.append(text(label, x, label_base(kind, y), LABEL_SIZE[kind], colour, stroke=stroke))
    out.append(text('A_DEVELOPMENT', W - 6, 127.2, 2.2, DIM, 'end'))
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def header():
    pos = {n: (x, y) for n, k, x, y, _ in LAYOUT}
    osc = []
    for o in range(3):
        row = [pos.get('OSC%d_%d' % (o, j), (0.0, 0.0)) for j in range(len(OSC_PARTS))]   # osc 1's MIX stays at 0,0 and is skipped
        osc.append('{%s}' % ', '.join('{%.2ff, %.2ff}' % p for p in row))
    lines = ['// layout.hpp: generated by tools/panel.py, do not edit. Component centres in mm.', '#pragma once',
             'namespace layout {', 'namespace triple {',
             'constexpr float OSC[3][%d][2] = {%s};   // wave, invert, phase, coarse, fine, pan, mix' % (len(OSC_PARTS), ', '.join(osc))]
    lines += ['constexpr float %s[2] = {%.2ff, %.2ff};' % (n, x, y) for n, k, x, y, _ in LAYOUT if not n.startswith('OSC')]
    lines += ['constexpr float FACE[3] = {%.2ff, %.2ff, %.3ff};   // mascot centre x, y, radius (TalkingFace)' % (MASCOT_X, BAND / 2, MASCOT_R)]
    lines += ['}', '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    bad = problems()
    assert not bad, '; '.join(bad)
    files = {os.path.join('res', 'Triple.svg'): panel(), os.path.join('src', 'layout.hpp'): header()}
    for rel, body in files.items():
        os.makedirs(os.path.dirname(os.path.join(ROOT, rel)), exist_ok=True)
        with open(os.path.join(ROOT, rel), 'w', encoding='utf-8') as f:
            f.write(body)
    print('wrote', ', '.join(files), '(%d components)' % len(LAYOUT))
```

- [ ] **Step 2: Generate**

Run: `py -3.11 tools/panel.py`
Expected: `wrote res\Triple.svg, src\layout.hpp (40 components)`. The script asserts nothing overlaps (parts, labels, band, screws, edges); an `AssertionError` lists what touches what, fix the table and rerun.
If `fontTools` or the DejaVu font is missing: `py -3.11 -m pip install fonttools`, or set `RACK_FONT` to a DejaVuSans.ttf path.

- [ ] **Step 3: Look at the panel next to a sibling (house-style check)**

Render `res/Triple.svg` and `C:\Users\Arikl\dev\active\my-lil-drums\res\Drums.svg` with Playwright (Edge, `--headless`) and save the screenshots to `C:\Users\Arikl\Desktop\screenshots` (Ari's rule: pass that directory explicitly). Check: lime title band with dark bold `TRIPLE`, magenta mascot top right, row captions (`OSC 1/2/3`, `AMP ENV`, `FILTER`, `FILTER ENV`) in dim grey, jack zone dark rounded, `V/OCT` and `GATE` rings lime, `CUTOFF` ring magenta, `OUT L/R` lime-ringed pads, `A_DEVELOPMENT` footer. Any deviation from Drums: fix in `panel.py`, regenerate.

- [ ] **Step 4: Commit**

```powershell
git add tools/panel.py res src/layout.hpp
git commit -m "Add the panel generator and the generated panel and layout"
```

---

### Task 6: Rack module and widget

**Files:**
- Create: `src/plugin.hpp`, `src/plugin.cpp`, `src/Triple.hpp`, `src/Triple.cpp` (`src/TalkingFace.hpp` was copied in Task 1)

- [ ] **Step 1: Write `src/plugin.hpp`**

```cpp
// plugin.hpp: the plugin's model and the house light colours.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include <rack.hpp>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelTriple;

// House light colours, the same as the panel (tools/panel.py), My lil Drums, Looper and Acid Bass.
struct LimeLight : GrayModuleLightWidget { LimeLight() { addBaseColor (nvgRGB (0xc8, 0xff, 0x3a)); } };
```

- [ ] **Step 2: Write `src/plugin.cpp`**

```cpp
#include "plugin.hpp"
Plugin* pluginInstance;
void init (Plugin* p) { pluginInstance = p; p->addModel (modelTriple); }
```

- [ ] **Step 3: Write `src/Triple.hpp`**

```cpp
// Triple.hpp: the My lil Triple module: 16 poly voices of three oscillators, filter and two ADSRs.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "plugin.hpp"
#include "FaceTap.h"
#include "TripleVoice.h"
#include "WaveTable.h"
#include <atomic>

enum OscParam { O_WAVE, O_INV, O_PHASE, O_COARSE, O_FINE, O_PAN, O_MIX, O_N };

struct Triple : Module
{
    enum ParamId
    {
        OSC_PARAM,   // 3 x O_N
        AM_PARAM = OSC_PARAM + 3 * O_N, HQ_PARAM,
        A_ATK_PARAM, A_DEC_PARAM, A_SUS_PARAM, A_REL_PARAM,
        FTYPE_PARAM, CUTOFF_PARAM, RES_PARAM, ENVAMT_PARAM, KEYTRK_PARAM,
        F_ATK_PARAM, F_DEC_PARAM, F_SUS_PARAM, F_REL_PARAM,
        PARAMS_LEN
    };
    enum InputId { VOCT_INPUT, GATE_INPUT, CUTCV_INPUT, INPUTS_LEN };
    enum OutputId { OUT_L_OUTPUT, OUT_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { INV_LIGHT, AM_LIGHT = INV_LIGHT + 3, HQ_LIGHT, LIGHTS_LEN };

    TripleVoice voice[16];
    TripleCfg cfg;
    std::vector<float>* table = nullptr;                      // custom wave; the audio thread owns this one
    std::atomic<std::vector<float>*> incoming { nullptr };    // the UI thread hands a new one over here
    std::string tablePath;                                    // UI thread only; saved in the patch
    FaceTap face;
    int tick = 0;

    Triple()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        static const char* const names[3] = { "Osc 1", "Osc 2", "Osc 3" };
        for (int o = 0; o < 3; ++o)
        {
            const int b = OSC_PARAM + o * O_N;
            const std::string n = names[o];
            configSwitch (b + O_WAVE, 0.f, W_N - 1, W_SAW, n + " wave", { "Sine", "Triangle", "Square", "Saw", "Rounded saw", "Noise", "Custom" });
            configSwitch (b + O_INV, 0.f, 1.f, 0.f, n + " invert", { "Off", "Inverted" });
            configParam (b + O_PHASE, 0.f, 1.f, 0.f, n + " phase offset (R ahead of L)", " deg", 0.f, 360.f);
            configParam (b + O_COARSE, -24.f, 24.f, 0.f, n + " coarse", " st")->snapEnabled = true;
            configParam (b + O_FINE, -1.f, 1.f, o == 1 ? 0.1f : o == 2 ? -0.1f : 0.f, n + " fine", " st");
            configParam (b + O_PAN, -1.f, 1.f, 0.f, n + " pan");
            if (o > 0) configParam (b + O_MIX, 0.f, 1.f, 0.5f, n + " mix (relative to osc 1)", "%", 0.f, 100.f);
        }
        configSwitch (AM_PARAM, 0.f, 1.f, 0.f, "AM OSC3 (osc 3 modulates osc 1 + 2)", { "Off", "On" });
        configSwitch (HQ_PARAM, 0.f, 1.f, 1.f, "HQ anti-aliasing", { "Off", "On" });
        configParam (A_ATK_PARAM, 0.0001f, 2.f, 0.002f, "Amp attack", " ms", 0.f, 1000.f);
        configParam (A_DEC_PARAM, 0.001f, 4.f, 0.3f, "Amp decay", " ms", 0.f, 1000.f);
        configParam (A_SUS_PARAM, 0.f, 1.f, 0.8f, "Amp sustain", "%", 0.f, 100.f);
        configParam (A_REL_PARAM, 0.001f, 4.f, 0.2f, "Amp release", " ms", 0.f, 1000.f);
        configSwitch (FTYPE_PARAM, 0.f, F_N - 1, F_LP, "Filter type", { "Low-pass", "High-pass", "Band-pass" });
        configParam (CUTOFF_PARAM, std::log2 (20.f), std::log2 (20000.f), std::log2 (20000.f), "Cutoff", " Hz", 2.f);
        configParam (RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f);
        configParam (ENVAMT_PARAM, -1.f, 1.f, 0.f, "Filter envelope amount (+-6 octaves)", "%", 0.f, 100.f);
        configParam (KEYTRK_PARAM, 0.f, 1.f, 0.f, "Key tracking", "%", 0.f, 100.f);
        configParam (F_ATK_PARAM, 0.0001f, 2.f, 0.002f, "Filter attack", " ms", 0.f, 1000.f);
        configParam (F_DEC_PARAM, 0.001f, 4.f, 0.3f, "Filter decay", " ms", 0.f, 1000.f);
        configParam (F_SUS_PARAM, 0.f, 1.f, 0.5f, "Filter sustain", "%", 0.f, 100.f);
        configParam (F_REL_PARAM, 0.001f, 4.f, 0.2f, "Filter release", " ms", 0.f, 1000.f);
        configInput (VOCT_INPUT, "V/OCT (poly)");
        configInput (GATE_INPUT, "Gate (poly)");
        configInput (CUTCV_INPUT, "Cutoff CV (1 V/oct, poly)");
        configOutput (OUT_L_OUTPUT, "Left (poly)");
        configOutput (OUT_R_OUTPUT, "Right (poly)");
        refresh();
    }

    ~Triple()
    {
        delete table;
        delete incoming.load();
    }

    // ---- custom wave (UI thread) ----
    // An empty table means "sine"; the audio thread only ever sees whole tables.
    bool loadTable (const std::string& path)
    {
        std::vector<float>* t = new std::vector<float>;
        bool ok = false;
        if (! path.empty())
        {
            try { std::vector<uint8_t> d = system::readFile (path); Sample s; ok = wavfile::parse (d.data(), d.size(), s) && makeTable (s, *t); }
            catch (const std::exception&) {}
        }
        if (! ok) t->clear();
        tablePath = ok ? path : "";
        delete incoming.exchange (t);   // a previous unadopted table is dropped
        return ok;
    }

    // ---- audio thread ----
    void refresh()
    {
        for (int o = 0; o < 3; ++o)
        {
            const int b = OSC_PARAM + o * O_N;
            OscCfg& c = cfg.osc[o];
            c.wave = (int) std::round (params[b + O_WAVE].getValue());
            c.invert = params[b + O_INV].getValue() > 0.5f;
            c.phase = params[b + O_PHASE].getValue();
            c.coarse = std::round (params[b + O_COARSE].getValue());
            c.fine = params[b + O_FINE].getValue();
            c.pan = params[b + O_PAN].getValue();
            c.mix = o == 0 ? 1.f : params[b + O_MIX].getValue();
            lights[INV_LIGHT + o].setBrightness (c.invert ? 1.f : 0.f);
        }
        cfg.am = params[AM_PARAM].getValue() > 0.5f;
        cfg.hq = params[HQ_PARAM].getValue() > 0.5f;
        lights[AM_LIGHT].setBrightness (cfg.am ? 1.f : 0.f);
        lights[HQ_LIGHT].setBrightness (cfg.hq ? 1.f : 0.f);
        cfg.ampA = params[A_ATK_PARAM].getValue();   cfg.ampD = params[A_DEC_PARAM].getValue();
        cfg.ampS = params[A_SUS_PARAM].getValue();   cfg.ampR = params[A_REL_PARAM].getValue();
        cfg.ftype = (int) std::round (params[FTYPE_PARAM].getValue());
        cfg.cutoff = std::exp2 (params[CUTOFF_PARAM].getValue());
        cfg.res = params[RES_PARAM].getValue();
        cfg.envAmt = params[ENVAMT_PARAM].getValue();
        cfg.keytrack = params[KEYTRK_PARAM].getValue();
        cfg.fltA = params[F_ATK_PARAM].getValue();   cfg.fltD = params[F_DEC_PARAM].getValue();
        cfg.fltS = params[F_SUS_PARAM].getValue();   cfg.fltR = params[F_REL_PARAM].getValue();
        cfg.table = table;
    }

    void process (const ProcessArgs& a) override
    {
        if (std::vector<float>* t = incoming.exchange (nullptr))
        {
            delete table;   // ponytail: frees on the audio thread; fine for a user-triggered load
            table = t;
            cfg.table = t;
        }
        if ((tick++ & 15) == 0) refresh();   // knobs are read every 16 samples, not every sample

        const int n = std::max (inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels());
        outputs[OUT_L_OUTPUT].setChannels (n);
        outputs[OUT_R_OUTPUT].setChannels (n);
        float sum = 0.f;
        for (int c = 0; c < n; ++c)
        {
            float l, r;
            voice[c].process (cfg, inputs[VOCT_INPUT].getPolyVoltage (c), inputs[GATE_INPUT].getPolyVoltage (c) >= 1.f,
                              inputs[CUTCV_INPUT].getPolyVoltage (c), a.sampleRate, l, r);
            outputs[OUT_L_OUTPUT].setVoltage (5.f * l, c);
            outputs[OUT_R_OUTPUT].setVoltage (5.f * r, c);
            sum += l;
        }
        face.push (clamp (sum, -1.f, 1.f), a.sampleRate);
    }

    json_t* dataToJson() override
    {
        json_t* j = json_object();
        json_object_set_new (j, "customWave", json_string (tablePath.c_str()));
        return j;
    }
    void dataFromJson (json_t* j) override
    {
        json_t* s = json_object_get (j, "customWave");
        if (json_is_string (s) && *json_string_value (s)) loadTable (json_string_value (s));
    }
};
```

- [ ] **Step 4: Write `src/Triple.cpp`**

```cpp
// Triple.cpp: the panel widget and the model.
// Author: Ari Klopfer / A_Development · MIT
#include "Triple.hpp"
#include "TalkingFace.hpp"
#include "layout.hpp"
#include <osdialog.h>

struct TripleWidget : ModuleWidget
{
    TripleWidget (Triple* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Triple.svg")));
        namespace L = layout::triple;
        {
            TalkingFace* f = createWidget<TalkingFace> (mm2px (Vec (L::FACE[0] - L::FACE[2], L::FACE[1] - L::FACE[2])));
            f->box.size = mm2px (Vec (2 * L::FACE[2], 2 * L::FACE[2]));
            if (module) f->tap = &module->face;
            addChild (f);
        }
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        auto knob = [&] (const float* mm, int id) { addParam (createParamCentered<Rogan1PSWhite> (at (mm), module, id)); };
        auto latch = [&] (const float* mm, int id, int light) {
            addParam (createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>> (at (mm), module, id, light));
        };

        for (int o = 0; o < 3; ++o)
            for (int j = 0; j < O_N; ++j)
            {
                if (o == 0 && j == O_MIX) continue;   // osc 1 has no mix
                const int id = Triple::OSC_PARAM + o * O_N + j;
                if (j == O_INV) latch (L::OSC[o][j], id, Triple::INV_LIGHT + o);
                else knob (L::OSC[o][j], id);
            }
        latch (L::AM_PARAM, Triple::AM_PARAM, Triple::AM_LIGHT);
        latch (L::HQ_PARAM, Triple::HQ_PARAM, Triple::HQ_LIGHT);
        knob (L::A_ATK_PARAM, Triple::A_ATK_PARAM);     knob (L::A_DEC_PARAM, Triple::A_DEC_PARAM);
        knob (L::A_SUS_PARAM, Triple::A_SUS_PARAM);     knob (L::A_REL_PARAM, Triple::A_REL_PARAM);
        knob (L::FTYPE_PARAM, Triple::FTYPE_PARAM);     knob (L::CUTOFF_PARAM, Triple::CUTOFF_PARAM);
        knob (L::RES_PARAM, Triple::RES_PARAM);         knob (L::ENVAMT_PARAM, Triple::ENVAMT_PARAM);
        knob (L::KEYTRK_PARAM, Triple::KEYTRK_PARAM);
        knob (L::F_ATK_PARAM, Triple::F_ATK_PARAM);     knob (L::F_DEC_PARAM, Triple::F_DEC_PARAM);
        knob (L::F_SUS_PARAM, Triple::F_SUS_PARAM);     knob (L::F_REL_PARAM, Triple::F_REL_PARAM);
        addInput (createInputCentered<PJ301MPort> (at (L::VOCT_INPUT), module, Triple::VOCT_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::GATE_INPUT), module, Triple::GATE_INPUT));
        addInput (createInputCentered<PJ301MPort> (at (L::CUTCV_INPUT), module, Triple::CUTCV_INPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::OUT_L_OUTPUT), module, Triple::OUT_L_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::OUT_R_OUTPUT), module, Triple::OUT_R_OUTPUT));
    }

    void appendContextMenu (Menu* menu) override
    {
        Triple* m = dynamic_cast<Triple*> (module);
        if (! m) return;
        menu->addChild (new MenuSeparator);
        menu->addChild (createMenuItem ("Load custom wave (single-cycle WAV)...", "", [=] {
            osdialog_filters* filters = osdialog_filters_parse ("WAV:wav");
            char* path = osdialog_file (OSDIALOG_OPEN, nullptr, nullptr, filters);
            osdialog_filters_free (filters);
            if (path) { m->loadTable (path); std::free (path); }
        }));
        menu->addChild (createMenuLabel (m->tablePath.empty() ? "Custom wave: none (sine)" : "Custom wave: " + system::getFilename (m->tablePath)));
        menu->addChild (createMenuItem ("Clear custom wave", "", [=] { m->loadTable (""); }));
    }
};

Model* modelTriple = createModel<Triple, TripleWidget> ("Triple");
```

- [ ] **Step 5: Build the plugin**

Run: `pwsh -NoProfile -File tools/mk.ps1`
Expected: g++ compiles `plugin.cpp` and `Triple.cpp` with no warnings, links, exit code 0, and `plugin.dll` exists in the repo root (`Test-Path plugin.dll` -> True).

- [ ] **Step 6: Re-run the core tests (guards against a header edit slipping in)**

Run: `pwsh -NoProfile -File tools/mk.ps1 test`
Expected: `passed: 0 failure(s)`.

- [ ] **Step 7: Commit**

```powershell
git add src plugin.json
git commit -m "Add the Rack module and widget"
```

---

### Task 7: Install and listen

Rack must be closed to pick up a new plugin, and it is Ari's live session, so **ask Ari to close Rack** (or confirm it may be restarted) before this task.

**Files:** none in the repo.

- [ ] **Step 1: Package**

Run: `pwsh -NoProfile -File tools/mk.ps1 dist`
Expected: `dist\MyLilTriple-2.0.0-win-x64.vcvplugin` exists.

- [ ] **Step 2: Install (after Ari closes Rack)**

```powershell
Get-Process Rack -ErrorAction SilentlyContinue   # must print nothing
Copy-Item dist\MyLilTriple-2.0.0-win-x64.vcvplugin "$env:LOCALAPPDATA\Rack2\plugins-win-x64\"
```
Start Rack. It unpacks the `.vcvplugin` into `plugins-win-x64\MyLilTriple`. Check `Rack2\log.txt` for `Loaded plugin MyLilTriple` and no error for it:

```powershell
Select-String -Path "$env:LOCALAPPDATA\Rack2\log.txt" -Pattern 'MyLilTriple' | Select-Object -Last 5
```

- [ ] **Step 3: Patch and measure with the `rack` MCP**

Server `rack` (Ari's My lil MCP on 127.0.0.1:2600/mcp; see memory `reference-vcv-rack-mcp-server` for the patch / measure-via-LISTEN recipe, and its TRANSIT-maps trap). If it reports ECONNREFUSED, Rack is not running or the MCP module is not loaded: tell Ari rather than guessing. Build in a new patch: a MIDI-to-CV or a sequencer into Triple V/OCT + GATE, Triple OUT L/R into an audio module. Verify by measurement, not by eye:
1. Default patch plays: output level above silence while the gate is high, and back to silence after release.
2. Wave knob: each of the 7 settings changes the measured spectrum; Custom with no file is a sine.
3. Poly: a 4-channel V/OCT and gate gives 4-channel L/R and four distinct pitches.
4. Filter: cutoff down darkens the output; env amount up makes it open on the note; HP and BP types change the balance.
5. AM OSC3 on: output level drops; INV on osc 2 with osc 2 mix at 100% and osc 1 matching: audible thinning or cancellation.
6. Load a single-cycle WAV from the context menu, set an osc to Custom, confirm the pitch holds; save the patch, reopen, confirm the path reloads.
7. Mascot mouth moves with the sound.

- [ ] **Step 4: Fix anything found, add a regression check to the core tests where it is DSP, rebuild, repeat Step 2 and 3**

- [ ] **Step 5: Commit any fixes**

```powershell
git add -A
git commit -m "Fix issues found while listening in Rack"
```

---

### Task 8: README and handoff

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write `README.md`**

````markdown
# My lil Triple

A polyphonic three-oscillator synth voice for VCV Rack 2 by A_Development, in the spirit of FL Studio's 3x Osc, with an amp ADSR and a filter with its own ADSR.

Built clean-room from the published behavior of 3x Osc. No Image-Line code, samples or artwork.

## What it has

- 3 oscillators: sine, triangle, square, saw, rounded saw, noise, custom (single-cycle WAV from the context menu)
- Per oscillator: invert, phase offset (R ahead of L), coarse +-24 semitones, fine +-1 semitone, pan, mix (osc 2 and 3, relative to osc 1)
- AM OSC3: osc 3 amplitude-modulates osc 1 + 2
- HQ: polyBLEP anti-aliasing on or off
- Amp ADSR; filter (low-pass, high-pass, band-pass) with cutoff, resonance, envelope amount, key tracking, and its own ADSR
- Ports: V/OCT, gate, cutoff CV in (all polyphonic, 16 voices); L and R out (polyphonic)

## Build

Rack SDK 2.6.x. `make test` runs the DSP tests, `make dist` packages the plugin.
`tools/panel.py` regenerates the panel and `src/layout.hpp`.

MIT. Author: Ari Klopfer / A_Development.
````

- [ ] **Step 2: Commit**

```powershell
git add README.md
git commit -m "Add README"
```

- [ ] **Step 3: Stop and report to Ari. Do not publish.**

Creating `Adev-io/my-lil-triple`, pushing, tagging `v2.0.0`, adding it to `mylilvj.com/source/` and to `Desktop\VCV-Library-submissions.md` are outward-facing: ask Ari first, as with My lil Drums. When Ari says go: keep `docs/superpowers/` out of the published tree the way my-lil-drums did (untrack it, add to `.git/info/exclude`), then create the repo, push `main` and the tag, confirm CI is green. Add a project memory note for My lil Triple at the same time.

---

## Self-review

**Spec coverage**
- Per-osc wave (7), invert, phase offset, coarse, fine, pan, mix: Tasks 2, 4, 6 (`OscCfg`, `oscSample`, params).
- AM OSC3, HQ: Tasks 2, 4, 6.
- Amp ADSR, filter LP/HP/BP, cutoff, resonance, env amount, key track, filter ADSR: Tasks 3, 4, 6.
- Poly 16, V/OCT, gate, cutoff CV, L/R poly out: Task 6 `process`.
- Context menu custom wave: Task 6 widget; saved path: `dataToJson`.
- House style, mascot: Tasks 1 (copy `TalkingFace.hpp`, `FaceTap.h`), 5, 6.
- Tests 1-8 from the spec: Task 4 (1 pitch, 2 coarse/fine, 3 invert, 5 envelope release + `test_adsr.h`, 7 sixteen voices, 8 pan) and Task 2 (4 HQ) and Task 3 (6 filter).
- Install and listen: Task 7. Publishing is deliberately a question for Ari: Task 8.

**Placeholders:** none; every code step carries its full file.

**Consistency:** `Wave`/`W_*`, `FilterType`/`F_*`, `OscCfg`, `TripleCfg`, `TripleVoice::process(cfg, voct, gate, cutCv, sr, l, r)`, `svfCoef`, `makeTable`, `TABLE_N`, `O_*`, `layout::triple::OSC[3][7][2]` are used with the same names in every task.

**Known ceilings (ponytail comments or noted here):** knobs read every 16 samples; one `tan` per voice per sample; custom wave read is linear-interpolated without mip-maps (aliases at high pitch); saved wave path is absolute; gate threshold is a plain 1 V compare without hysteresis.
