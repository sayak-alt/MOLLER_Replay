# Separate TS replay with Hall A Podd

This add-on follows the tested MD/SM apparatus–detector–run–decoder structure. It requires the TS **waveform** digitizer update (schema 2), not only the initial PE/time tree. It adds new TS files and does not modify MD/SM reconstruction, the shared `MDPulse.h`, or the TS digitizer. The supplied map matches the waveform-development map: one effective channel per ID 400–413, provisional ROC 3 / slot 3 / channels 0–13.

## Processing architecture

- `MOLLERTriggerScintillatorReader`: reads `trigscint_digi` waveform/address/electronics data, validates sample completeness and raw sums, and groups by detector/copy. It reads no generated PE, injected pedestal/gain, hit-time truth, or `waveform_valid` simulation flags.
- `MOLLERTriggerScintillatorRun : THaRunBase`: presents sequential ROOT waveform events to Podd through a simulation event-buffer adapter. Event IDs must fit 0..INT_MAX in this version; duplicate detector IDs/copies within one event are rejected.
- `MOLLERTriggerScintillatorDecoder : THaEvData`: initializes the base Podd crate-map state, clears previous slot data, allocates standard `THaSlotData` through `idx()`, and loads each ADC sample with `loadData("adc",...)`. No raw CODA/fADC firmware words are invented.
- `MOLLERTriggerScintillatorApparatus : THaApparatus`: owns the separate `ts` detector under apparatus `moller` and calls its reconstruction stage.
- `MOLLERTriggerScintillatorDetector : THaDetector`: gets ADC samples from Podd slot data and uses the existing `ReconstructMDPulse` helper for baseline/noise, pulse selection, NSB/NSA integration, charge and timing. Electronics metadata and input/CSV/DB addresses must agree; the detector fails explicitly on mismatch.

Podd's standard `THaAnalyzer` manages the event loop and writes output definitions from `replay_moller_trigscint.odef`. This uses Podd directly, not SBS-offline. The current adapter is for mock ROOT waveforms; real CODA/EVIO data require an appropriate real-data decoder.

## Build and install

Run from the framework root containing `MOLLER-offline` and `MOLLER-replay` (no top-level CMakeLists). In tcsh:

```tcsh
setenv MOLLER_WORK "$cwd"
cmake -S MOLLER-offline/trigscint -B build/trigscint -DPODD_PREFIX=/u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$MOLLER_WORK/install" -DBUILD_TESTING=ON
cmake --build build/trigscint -j4
ctest --test-dir build/trigscint --output-on-failure
cmake --install build/trigscint
ls -lh install/lib64/libmollertrigscint.so
```

The library is independent of MD/SM libraries. The build uses the shared source `MOLLER-offline/include/MDPulse.h` and installs the same header as a dependency; it does not edit it. Replay requires ROOT and Podd but does not link REMOLL or Geant4. The digitizer does not need rebuilding for this replay-only addition.

Tests: `ts_input` checks map/metadata/sample integrity and absent-vs-measured-zero readout; `ts_pulse` checks a deterministic 2.1 ns emission/10 ns pulse-shape convolution against the shared extraction; `ts_reader` uses a synthetic ROOT fixture containing ADC/electronics branches and **no simulation truth branches**, covering full, sparse and empty events and corrupt inputs. The standalone input/pulse tests passed with the system C++ compiler. Reader behavior and library syntax passed with ROOT/Podd API stand-ins; native ROOT dictionary generation, Podd linking and analyzer execution require the ifarm tests above. A pure C++ test-only build is available with `-DBUILD_REPLAY=OFF` when ROOT/Podd are unavailable.

## Configure the runtime and check the reader

Always source setup from this framework root, then select the TS database explicitly:

```tcsh
cd "$MOLLER_WORK"
source setup.csh
setenv DB_DIR "$MOLLER_REPLAY/DB_trigscint"
root -l -b -q 'MOLLER-replay/scripts/check_trigscint_reader.C+("/w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout_dig.root",100)'
```

This input path is Sayak's existing waveform output. Substitute your actual waveform file if different. With default full readout, expect 100 events, 1400 complete waveforms and 140000 ADC samples. A standalone reader map filename may be supplied as the third macro argument if `MOLLER_REPLAY` is unset.

## Run replay

```tcsh
bash "$MOLLER_REPLAY/scripts/run_trigscint_replay.sh" "/w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout_dig.root" "$MOLLER_WORK/replayed_ts_100.root" 100
```

Expect 100 events read/decoded/analyzed/accepted. `SetLastEvent(nevents-1)` implements an exact positive event count. Use `-1` for all digitized entries:

```tcsh
bash "$MOLLER_REPLAY/scripts/run_trigscint_replay.sh" "/w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout_dig.root" "$MOLLER_WORK/replayed_ts_all.root" -1
```

Input/output must differ. Reusing an output filename overwrites the previous replay output. Log filenames follow the output ROOT name with `.log`. The wrapper accepts an existing input path; the direct GEM-style steering macro additionally accepts a file base with `DATA_DIR` and `OUT_DIR` support. A direct analyzer session must load `libmollertrigscint.so` before `.L replay_moller_trigscint.C`. Use a fresh analyzer process when switching detector systems; this add-on does not combine MD/SM/TS replay in one apparatus.

`DB_trigscint` includes the channel map CSV, `db_moller.ts.dat`, `db_cratemap.dat` and mock `db_run.dat`. The simulation crate-map file intentionally declares no hardware modules; slot data come from the custom decoder. Changing the map requires consistent updates to digitization CSV, replay CSV and the DB channel-map rows. Database loading checks the mapping and electronics instead of silently accepting wrong charge units.

## Reconstruction settings and interpretation

Default replay settings are 14 configured channels, 100 samples/channel, 4 ns/sample, gate start -170 ns, 12 bits, 1 V range, 50 ohms, first 10 samples for baseline/noise, threshold max(20.48 ADC counts, 5 times estimated noise), one sample above threshold, NSB=5 and NSA=22. The gate origin and sample-center convention must match digitization.

`timing_reference`: 0 peak center, 1 linearly interpolated detection threshold, 2 interpolated rising half-height (default). All three times and their validity flags are stored. These are software definitions, not a complete fADC250 firmware emulation.

`integration_reference`: 0 peak or 1 first sample above threshold (default). The integral includes anchor-NSB through anchor+NSA-1, i.e. 27 samples/108 ns with default settings. Baseline is subtracted for each sample. Charge in pC is integral * (range_v / 2^bits) * dt_ns * 1e-9 / resistance_ohm * 1e12. With defaults the conversion is 0.01953125 pC per ADC-sum count. Peak finding is one selected pulse per channel, not full multi-pulse deconvolution.

The reconstruction estimates baseline/noise from data, not from the injected calibration. Gain is not used to reconstruct charge or to select pulses. Reconstructed PE would require a separate replay gain calibration; generated PE is not copied into the replay as a measurement.

Missing waveform readout produces `readout=0`, NaN baseline/noise/charge and a software time-quality bit 32. A recorded pedestal waveform with no accepted pulse has `readout=1`, `found=0`, baseline/noise estimates, zero extracted pulse charge and no valid pulse time. Negative reconstructed charge is allowed for detected noise/low-signal pulses; no post-selection charge cut is applied by the detector. `found=1` and `time_valid=1` do not prove a physical pulse.

Crucially, a digitizer row with `waveform_valid=0` is still a recorded ADC waveform and is replayed normally. For the user's current 5000-event sample, the 25 positive-PE rows with unknown injected signal time are **not** suppressed by the reader/decoder/detector. The validation script excludes that group from physical pulse-efficiency/charge comparisons and reports any pulses found in it separately. Thus simulation truth never affects pulse finding.

## Inspect output

```tcsh
root -l replayed_ts_100.root
```

At ROOT, enter each statement on one line:

```cpp
TTree *t=nullptr;
_file0->GetObject("T",t);
std::cout << "Events: " << t->GetEntries() << std::endl;
t->Scan("moller.ts.evnum:moller.ts.nchannels:moller.ts.nreadout:ts.input.nsamples","","",5);
t->Scan("moller.ts.evnum:moller.ts.detid:moller.ts.charge_pc:moller.ts.time_ns:moller.ts.time_valid","moller.ts.found==1","",20);
t->Draw("moller.ts.charge_pc>>hTSQ(100,-5,40)","moller.ts.readout==1 && moller.ts.found==1 && moller.ts.clipped==0 && moller.ts.saturated==0");
.q
```

Default full readout gives 14 configured/readout channels and 1400 samples per event. Output also includes copy ID, provisional ROC/slot/channel/plane/sector metadata, baseline, noise, peak height/sample, integration bounds, integral, saturation/clipping and timing-quality flags. Variables are `moller.ts.*` and decoder summaries `ts.input.*`. Metadata default copy IDs are 0 in full-readout padding, while a truly absent response/readout row appears as copyid=-1 in the configured replay output.

## Validate after replay

For the first 100 entries, from the framework root:

```tcsh
root -l -b -q 'MOLLER-replay/scripts/validate_trigscint_replay.C+("/w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout_dig.root","replayed_ts_100.root","TS_verify_validation",0,100)'
```

For the full replay:

```tcsh
root -l -b -q 'MOLLER-replay/scripts/validate_trigscint_replay.C+("/w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout_dig.root","replayed_ts_all.root","TS_all_validation",0)'
```

Fourth argument 0 selects all modules; use 400–413 for one detector. The fifth argument optionally limits input entries, default -1/all. Scope matching matters: comparing a 100-event replay against all 5000 input events without the limit reports expected unmatched input events.

The validator matches by event and detector/copy identity, reports readout availability, excludes unknown-energy rows and model-invalid positive PE, computes efficiency for eligible positive PE and false detections for known zero-PE waveforms, and compares clean detected charge to generated npe_poiss*qpe_c. It produces PNG/PDF plus ROOT histogram products. Charge selection requires a modeled valid signal, recorded waveform, positive PE, pulse found, no clipping or saturation; there is no cut on reconstructed charge. The graph's plotted PE range is 0..100; higher counts enter overflow while summary counters cover all eligible rows.

Full generated charge is not the noiseless charge expected inside the finite replay window. Charge residuals are conditional on detection, with threshold bias at low charge. Replay time minus earliest saved hit includes scintillation emission, PMT response/TTS and the chosen estimator offset; it is not an intrinsic timing resolution. Stored simulation PE/gain/pedestal/hit-time validity are read only by this validation macro, not by reconstruction.

This is a waveform/Podd implementation check using the requested parallel-world inputs. The underlying deposited-energy proxy and 125 PE/MeV remain provisional for physical interpretation, as already recorded by the digitizer.
