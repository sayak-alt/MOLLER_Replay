# MOLLER MD framework: SBS-style separation, using Podd directly

This development version reorganizes the tested MOLLER prototype into a compiled
MOLLER-offline library plus a MOLLER-replay package. It does not load SBS-offline,
SBS detector classes, G4SBS, or SBSSimDecoder. Only MD is enabled.

**Validation status:** the unchanged pulse unit tests pass with g++. The generated
map, output definitions and launcher checks pass locally. ROOT, Podd and CMake
are not installed in this build environment, so the new shared library and its
end-to-end replay MUST be built/tested on ifarm. The previous flat prototype was
successfully tested by you; that does not establish that this new library build
has passed. Keep the previous working package until this version passes the same
100-event validation.

## Directory responsibilities

| Package | Purpose | SBS counterpart |
|---|---|---|
| External remoll | Geant4 transport and simulation truth | g4sbs |
| MOLLER-digitisation | Existing digitiser, included unchanged; MD-only config | libsbsdig |
| MOLLER-offline | Reader, simulation run/decoder, apparatus, MD detector, pulse code and ROOT dictionary; builds libmoller.so | SBS-offline detector/decoder library |
| MOLLER-replay | Podd database, replay macro, output definitions and validation | SBS-replay |

The existing digitisation source retains inactive GEM/SM classes for build
compatibility; `detectors_list mollerpmt` enables only MD. No changes to those
other detectors are part of this update. MD LUT CSVs are not included in this
snapshot: keep using your existing lookup table directory if selecting LUT mode.

## 1. Build on ifarm (tcsh)

Extract into a NEW directory, leaving the working prototype intact. From the
extracted `MOLLER-MD-framework` directory, with your usual ROOT/analyzer environment:

```tcsh
which analyzer
which root-config
setenv ANALYZER /u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo
cmake -S MOLLER-offline -B build/offline -DPODD_PREFIX="$ANALYZER" -DCMAKE_INSTALL_PREFIX="$cwd/install"
cmake --build build/offline -j4
ctest --test-dir build/offline --output-on-failure
cmake --install build/offline
source setup.csh
```

Use `-D`, not `-C`, for CMake variables. The analyzer prefix above is the one
shown in your logs; change it only if your installed analyzer changes. ROOT and
the compiler should match that analyzer installation. An ABI warning should be
resolved by the ifarm environment maintainers if it causes loading failures.

`setup.csh` must be sourced from the framework root AFTER installation. It sets
MOLLER_OFFLINE, MOLLER_REPLAY, DB_DIR and library/include search paths. It does not
source or require sbsenv.csh. Your environment must still provide analyzer and
its dependencies. Bash users can source `setup.sh` instead.

## 2. Replay the existing digitised file: first 100 events

No remoll rebuild or new digitisation is needed for this check. From the same
framework root, run this SINGLE shell command (not at a ROOT prompt):

```tcsh
bash "$MOLLER_REPLAY/scripts/run_replay.sh" "/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller_digi_MD_map_updated/moller-dig-MD/o_moller_dig.root" "$cwd/replayed_md.root" 100
```

The output path above is explicit: `replayed_md.root` in your current directory.
It will overwrite an existing output with that name, like your previous replay.
Use a new output name when retaining prior results.

To process every event, replace `100` with `-1`. The wrapper defaults to 100
when its event argument is omitted. Every invocation starts a fresh analyzer.
Do not add SBS replay startup scripts or unrelated apparatus to that process.

Equivalent direct invocation from the framework root, after setup:

```tcsh
analyzer -l -b -q 'MOLLER-replay/replay/replay.C("/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller_digi_MD_map_updated/moller-dig-MD/o_moller_dig.root","replayed_md.root",100)'
```

The shell wrapper additionally protects against input/output paths referring to
the same file through aliases. Prefer the wrapper.

## 3. Inspect output

```tcsh
root -l replayed_md.root
```

At the ROOT prompt:

```cpp
TTree* t=nullptr;
_file0->GetObject("T",t);
t->Scan("moller.md.evnum:moller.md.nchannels:md.input.nsamples","","",5);
t->Scan("moller.md.detid:moller.md.baseline:moller.md.noise:moller.md.charge_pc","moller.md.found==1","",5);
```

Expected channel/sample counts are 224 and 22400 per event. Raw sample counters
retain `md.input.*`; reconstruction now uses `moller.md.*` instead of
`md.reco.*`. Peak sample/window indices are zero based. No-pulse channels remain
in the output with found=0 and zero reconstructed charge.

## 4. Repeat the validation you already ran

From the framework root, in the shell:

```tcsh
root -l 'MOLLER-replay/scripts/validate_md_replay.C+("/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller_digi_MD_map_updated/moller-dig-MD/o_moller_dig.root","replayed_md.root")'
```

This creates MD_validation.png and MD_validation.root. For the SAME input and
100-event range, compare with your previous results: 22400 matched
channel-events, 22305 zero-PE channels, 35 false detections, 95 positive-PE
channels, 50 detected, and 6 clipped/saturated channel-events. Baseline residual
mean/RMS was approximately -0.00883/1.5879 ADC counts. These are reference results,
not measured results for this new build. Any discrepancy must be investigated
before replacing the prototype.

## Configuration and algorithms

Edit `MOLLER-replay/DB/db_moller.md.dat`, then rerun replay; no rebuild is needed.
All electronics constants must agree with the digitiser. The input reader still
requires the current 224-channel, 100-sample, 12-bit schema. A different schema
requires a reader change, not just editing database numbers.

Defaults preserve the tested algorithm exactly:

- Baseline: arithmetic mean of the first 10 samples.
- Noise: sample standard deviation of those samples, with denominator N-1.
- Peak: largest sample after the baseline region, strictly above baseline plus
  max(20.48 ADC counts, 5 times measured noise). Positive pulses, at most one peak.
- Integration: peak-NSB through peak+NSA-1, inclusive. With NSB=5 and NSA=22,
  this includes 27 samples (108 ns), truncated at waveform edges and flagged.
- Charge: baseline-subtracted ADC sum times (1 V / 4096) times 4 ns / 50 ohm,
  converted to pC. For these settings: 0.01953125 pC per ADC-sum count.
- Saturation: any sample at 4095; gate clipping and saturation are separate flags.
- Peak time: -170 ns + (peak_sample + 0.5)*4 ns. This is a sample-centre time,
  not a fitted arrival time or hardware time word.

This is NOT the SBS threshold-crossing/multipulse algorithm or fADC firmware
emulation. Architecture is being aligned while retaining your validated physics
and reconstruction behavior. NSB/NSA refer to the detected maximum here.

The detector reconstructs through Podd GetNumHits/GetData by ROC/slot/channel;
it does not dynamic_cast to the simulation decoder or access simulation truth.
The simulation decoder populates THaSlotData directly with ordered sample hits.
There is no CODA packing or hardware fADC module in this version. This leaves a
clear extension point for a real-data decoder but does not establish real CODA
compatibility.

The database has four integers per channel: detid, ROC, slot, channel, sorted by
segment/ring/detid from your existing CSV. The CSV is retained alongside it for
segment/FF/BF/ring metadata. Database mapping is authoritative in replay. If
changing electronics wiring, update BOTH the digitiser mapping and replay map
and rerun validation. The mapping is your mock map, not verified real cabling.

Injected pedestals/gains remain in the digitiser CSV. Replay estimates baseline
from measured samples; it never uses injected pedestal values to subtract charge.
No reconstructed PE or energy calibration is added. Those would require
independent replay calibration constants rather than using truth gain.

## Digitisation remains separate

The included `MOLLER-digitisation` snapshot keeps both Frank-Tamm and LUT options,
all-224 readout, per-channel pedestal/gain and Poisson/electronics behavior.
The replay is independent of which PE method generated the waveform.

For a NEW digitisation, use your existing working build, or from that subdirectory:

```tcsh
cd MOLLER-digitisation
setenv REMOLL_DIR /work/halla/moller12gev/sayak/remoll
bash scripts/build.sh
```

Supply your file list and current `.dat` as before. The default .dat selects MD
and Frank-Tamm, waveforms and full 224-channel readout. LUT mode requires your
existing LUT files and paths. Replay does not require REMOLL_DIR or Geant4;
remoll is needed upstream to generate/read simulation truth during digitisation.
See the included digitiser documentation for its output naming rules.

## Development checks without ROOT/Podd

```sh
cmake -S MOLLER-offline -B build/pulse -DBUILD_REPLAY=OFF
cmake --build build/pulse
ctest --test-dir build/pulse --output-on-failure
```

Or `g++ -std=c++17 -I MOLLER-offline/include MOLLER-offline/tests/test_pulse.cpp -o /tmp/test_md_pulse`
and then `/tmp/test_md_pulse`. This checks pulse math only, not the Podd integration.
