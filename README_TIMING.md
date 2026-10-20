# MD replay timing update

This is a source overlay for the working MOLLER-offline / MOLLER-replay package.
It changes replay only. Digitisation, raw samples, generated PE, pedestal/gain CSVs
and electronics simulation are unchanged. Existing digitised ROOT files can be reused.

## Install the overlay

From your existing MOLLER_Replay project root, back up MOLLER-offline and
MOLLER-replay. Extract MOLLER-MD-timing-update.zip there. It creates MD-timing-update.
Copy its source overlays:

```tcsh
cp -r MD-timing-update/MOLLER-offline/. MOLLER-offline/
cp -r MD-timing-update/MOLLER-replay/. MOLLER-replay/
cmake -S MOLLER-offline -B build/offline
cmake --build build/offline -j4
ctest --test-dir build/offline --output-on-failure
cmake --install build/offline
source setup.csh
```

The existing CMake cache retains your installation and Podd paths. If creating a
new build directory instead, supply -DPODD_PREFIX="$ANALYZER" and
-DCMAKE_INSTALL_PREFIX="$cwd/install" as before.

IMPORTANT: included database maps are the original 224-channel map. If you have
customized mapping or other constants, retain your customized rows/constants
and add the three new optional keys instead of replacing that database. Keep
DB and DB_timing mapping/electronics constants synchronized for a fair comparison.

Close any ROOT/analyzer sessions using the old libmoller.so before installing.
Use a fresh analyzer process for the updated library.

## Profiles

DB/db_moller.md.dat preserves the old result by default:

```text
moller.md.timing_reference = 0
moller.md.integration_reference = 0
moller.md.threshold_samples = 1
```

DB_timing/db_moller.md.dat selects the new time definition and integration anchor:

```text
moller.md.timing_reference = 2
moller.md.integration_reference = 1
moller.md.threshold_samples = 1
```

- timing_reference: 0 = peak sample centre, 1 = interpolated detection-threshold
  crossing, 2 = interpolated rising half-height crossing. Controls time_ns only.
- integration_reference: 0 = peak, 1 = threshold-crossing sample. Controls charge.
- threshold_samples: actual required count of consecutive samples above the
  effective detection threshold. This is NOT the encoded firmware NSAT register.
  Default 1 retains the prior acceptance. With >1, use the highest peak in a
  qualifying above-threshold run. Only one selected pulse per channel is reported.

All three are optional in LoadDB, so old databases retain legacy defaults.
To isolate timing changes without changing charge, use timing_reference=2 and
integration_reference=0. All explicit timing branches are calculated regardless
of which time is selected.

The effective threshold remains max(threshold_adc, threshold_sigma * measured
baseline noise), in counts above baseline. The threshold is exported per channel.
For a fixed 10 mV threshold at 1 V/12 bits, use threshold_adc=40.96 and
threshold_sigma=0. Do not assume 10 mV matches your run without checking its
actual acquisition settings. Existing baseline_samples=10 is preserved; this
is not silently changed to the firmware's baseline estimator.

## Run: new timing and TC integration

From the existing project root, AFTER source setup.csh:

```tcsh
setenv DB_DIR "$MOLLER_REPLAY/DB_timing"
bash "$MOLLER_REPLAY/scripts/run_replay.sh" "/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller_digi_MD_map_updated/moller-dig-MD/o_moller_dig.root" "$cwd/replayed_md_timing.root" 100
```

100 now means exactly 100 events for this zero-based mock run. -1 means all.
The existing launcher and the GEM-style two-argument invocation both work. The
loader path quoting fix and SetLastEvent(nevents-1) are included in this update.

To return to the legacy integration/time profile:

```tcsh
setenv DB_DIR "$MOLLER_REPLAY/DB"
```

Use a different output filename for comparisons. Output files can be overwritten.
Sourcing setup.csh resets DB_DIR to DB, so select DB_timing after sourcing.

## Inspect timing

```tcsh
root -l replayed_md_timing.root
```

At the ROOT prompt:

```cpp
TTree* t=nullptr;
_file0->GetObject("T",t);
t->GetEntries();
t->Scan("moller.md.timing_reference:moller.md.integration_reference","","",1);
t->Scan("moller.md.detid:moller.md.peak_time_ns:moller.md.threshold_time_ns:moller.md.half_height_time_ns:moller.md.time_valid:moller.md.time_quality","moller.md.found==1","",5);
t->Draw("moller.md.peak_time_ns-moller.md.half_height_time_ns","moller.md.found==1 && moller.md.half_height_time_valid==1");
```

Expected references are 2 and 1 for DB_timing. Output now has 33 variables.
All channel arrays remain aligned and all 224 channels remain in each event.

Repeat the existing validation macros with the new replay output filename. They
still work. With TC-based integration, charge, integration bounds and clipping
can change; do not expect old charge numbers to match. Baseline and detection
should remain the same if threshold settings and threshold_samples are unchanged.
The old noise-width estimate using 27 integration and 10 independent baseline
samples still describes a full nonoverlapping window approximately, but pulse
selection and integration-window changes affect its applicability.

## Added output (all under moller.md.)

| Name | Meaning |
|---|---|
| peak_time_ns | Peak sample centre, always the old peak definition |
| threshold_time_ns | Linear rising-edge crossing of the effective detection threshold |
| half_height_time_ns | Linear rising-edge crossing at baseline + half selected peak height |
| threshold_sample | First above-threshold sample in the selected run (zero based) |
| threshold_time_valid | Valid bracket for threshold interpolation |
| half_height_time_valid | Valid bracket and usable sampled peak |
| time_valid | Whether the selected time_ns is valid |
| time_quality | Software bit mask, described below |
| applied_threshold_adc | Effective detection threshold above baseline [ADC counts] |
| timing_reference | Scalar selector defining time_ns |
| integration_reference | Scalar selector defining integration bounds |
| threshold_samples | Scalar required consecutive sample count |

Existing time_ns retains the old meaning by default. With reference=2 it instead
contains half_height_time_ns; always inspect time_valid. Invalid times are NaN.
No accepted pulse gives threshold_sample=-1 and no valid timing. Zero charge and
existing found=0 conventions are retained.

Software time_quality bits (NOT the hardware fADC bit definitions):
- 1: no usable threshold-interpolation bracket for the selected run.
- 2: half-height time unavailable.
- 4: some sample in the waveform saturated (conservative waveform-wide flag).
- 8: selected peak is the final recorded sample; true peak is not established.
- 16: no accepted pulse.
A valid threshold time can coexist with a saturated pulse; check flags according
to analysis needs. Half-height is invalidated for saturation or a final-sample
peak. No automatic substitution of TC for a failed half-height result is made:
time_valid=0 and NaN prevent mixing different time definitions silently.

## Algorithms and scope

Times use the existing waveform sample-centre coordinate:
  gate_start_ns + (index + 0.5)*dt_ns.
Interpolations use those same coordinates. All times are relative to the
simulation gate coordinate, not absolute trigger timestamps or encoded hardware
coarse/fine words. Timing reference choice does not move the waveform.

For a selected peak with amplitude A above baseline B, half-height is B+A/2.
Find the last rising bracket before the selected peak where V_i <= level < V_i+1,
then linearly interpolate. Half-height can occur before threshold crossing.
The timing values are floating-point interpolation estimates, not quantized to
62.5 ps, and imply no guaranteed timing resolution.

TC integration includes samples [TC-NSB, TC+NSA-1], clipped to the recorded gate.
NSA includes TC. The default NSB=5, NSA=22 gives 27 samples when not clipped.
A new gate-overlap condition may change clipped even if charge is similar.

This is fADC-inspired software analysis, NOT bit-for-bit firmware emulation.
Firmware-specific first-decrease peak selection, pedestal/MaxPed checks,
integer arithmetic, coarse-time numbering, fine-time quantization, automatic
TC fallback and multipulse handling are not reproduced. Our software retains
its existing highest-peak selection, now optionally requiring consecutive
above-threshold samples. Exact comparison to real hardware requires the run's
firmware version and settings.

## Changed files

1. MOLLER-offline/include/MDPulse.h: timing algorithms, validity/quality flags,
   optional consecutive-sample requirement, selectable integration/time references.
2. MOLLER-offline/include/MOLLERMDDetector.h: added output members.
3. MOLLER-offline/src/MOLLERMDDetector.cxx: optional DB keys and output registration.
4. MOLLER-replay/replay/replay_moller_md.odef: 12 additional variables.
5. MOLLER-replay/DB/db_moller.md.dat: explicit legacy-default keys; map unchanged.
6. MOLLER-replay/replay/replay_moller_md.C: GEM-style launcher with event-limit fix.
7. MOLLER-replay/replay/replay.C: previously diagnosed macro-path quoting fix.
8. MOLLER-offline/CMakeLists.txt: timing test registration.

Added: MOLLER-offline/tests/test_timing.cpp and the DB_timing alternative profile.
The prior test_pulse.cpp is included unchanged. No decoder, run, reader,
apparatus, digitiser or simulation changes are required.

## Validation performed here

Pure C++ legacy pulse tests and new timing tests pass with g++ -std=c++17.
New tests cover known threshold/half-height crossings, independent time/charge
references, half-height below threshold, time offset, consecutive-sample logic,
clipping, saturation, final-sample peak, missing bracket and invalid config.
A separate seeded test compared 20,000 synthetic waveforms against the original
MDPulse implementation; default-mode charge, peak time, acceptance and flags
were identical. Output definitions and both 224-channel maps were checked.
ROOT/Podd compilation and end-to-end replay are not available in this environment;
build on ifarm and first test 100 events before running the full file.

References checked for timing definitions:
- JLab MOLLER fADC250 processing firmware 0x0D01/A3, readout processing TDC section:
  https://coda.jlab.org/drupal/system/files/coda/HardwareManual/fADC250/Moller_FADC250V2_Firmware_ver_0x0D01_A3_Description_Instructions.pdf
- JLab fADC250 processing firmware 0x0C14, pulse definition and TDC sections:
  https://coda.jlab.org/drupal/system/files/pdfs/HardwareManual/fADC250/FADC250_Processing_FPGA_Firmware_ver_0x0C14_Description_Instructions.pdf
