# Trigger scintillator waveform development

This update extends the separate `MOLLERTriggerScintillatorDigitizer` on MOLLER_Replay commit `94f0d2c51c564c9895d5f3504d8ec9ed7bba3bd1`. The MD/SM classes, calibration files, lookup tables, and replay code are not modified. Original TS PE-only settings continue to work: `enable_electronics` is false by default. TS Podd reconstruction is not included in this update.

## Response and timing

For each event and selected (detector ID, copy ID), `meanpe = sum.edep[MeV] * ts_pe_per_mev`, followed by a Poisson draw. Default IDs remain 400–413 and effective yield remains 125 PE/MeV. The nominal EJ-200-like photon yield 10,000 photons/MeV motivates only a reference scale: 125/10000 = 1.25% is the implied combined efficiency of an assumed yield, not a measured optical efficiency. This code simulates one effective readout per detector/copy, not two PMT ends. Do not apply 125 PE/MeV independently to two ends without defining their respective efficiencies.

Original REMOLL hit times and first recorded hit time are preserved. PE arrival times are generated as:

`arrival = anchor hit time + transit_time_ns + Exp(scint_decay_ns) + Gaussian(0,sigma_time_ns)`.

Defaults are scintillation decay 2.1 ns, Gaussian spread 1 ns, and zero transit offset. The 2.1 ns value is an EJ-200-like single-component model; no scintillator rise time is modeled. Gaussian spread is a zero-mean timing deviation and may place an arrival before the anchor when transit offset is zero. This is not a full optical flight-time/transit-time model.

`ts_hit_time_mode 0` (default) uses the earliest recorded hit as the anchor for every PE. `1` samples the recorded positive-hit-edep times with weights proportional to their saved `hit.edep`, falling back to the earliest recorded hit if all saved deposits are zero. These weights affect timing only: PE yield still uses `sum.edep`. Because saved hits can be filtered, mode 1 is an approximation to deposition timing, not recovery of the complete step history. `timing_model` per response row reports 0=no positive signal, 1=weighted saved-hit anchors, 2=earliest recorded hit, 3=positive PE with unavailable hit time.

The emitted PE times are convolved with the same area-normalized, single-sided exponential SPE charge model used in MD/SM: `I(t) = Qpe / pulse_tau * exp(-(t-tpe)/pulse_tau)` after the PE arrival, zero before. Default pulse_tau is **10 ns**, independent of the **2.1 ns scintillation decay**. Signal broadening comes from these distinct processes. `Qpe = elementary charge * PMT gain`. Gain does not change PE count. There is no PMT single-PE gain spread, dark current, afterpulsing, inter-event pileup, common-mode noise, Birks quenching, or position-dependent optical attenuation in this model.

REMOLL `trigscintMaterials.xml` at develop `9e22b834` references `trigscintMatrices.xml`, whose EJ200-labelled values are 10200 photons/MeV and time components 2.5/4.5 ns with weights 1/0. This digitizer's configurable 2.1 ns is an independent nominal EJ-200-like model, not a claim that it reproduces those constants. Reference: https://eljentechnology.com/products/plastic-scintillators/ej-200-ej-204-ej-208-ej-212

IDs 400–413 remain the requested parallel-world scoring source. `physical_scoring_validated` stays 0; the code does not infer validated physical energy from this source.

## Pedestal, gain, mapping and full readout

The bundled `TS_channel_calibration.csv` has 14 rows sorted upstream then downstream by sector. Columns:

`detid,pedestal_mean_adc,pedestal_sigma_adc,pmt_gain`.

These are synthetic development values: pedestal mean at 200 mV with channel offsets from -8 to +9 mV (converted to ADC counts for 1 V / 12 bits), per-sample sigma 5 ADC counts, gain 0.92–1.08 million. They are constant per channel, not newly randomized per event. Gaussian pedestal noise is independently sampled every ADC sample. Replace them with calibration data when available. If ADC range/bit depth changes, update ADC-unit pedestal values accordingly.

`MOLLER_trigscint_electronics_map.csv` assigns simulation-only ROC 3, slot 3, channels 0–13. These are **provisional addresses**, not verified real hardware wiring. Plane 0 is upstream and plane 1 downstream; sectors are 1–7. Both CSVs must cover every selected ID if provided; duplicates/invalid rows are rejected. Global pedestal/gain defaults apply only when no calibration file is supplied.

Full readout produces 14 effective channels and 1400 samples/event at the default 100 samples/channel, including events with no selected input records. It requires at most one copy per selected detector in each event. Multiple copies are rejected rather than silently combined; verify `hit.id`/`sum.vid` if this occurs. Sparse mode (`ts_readout_all_channels 0`) retains the original separate detector/copy grouping.

Important distinctions:

| Input case | Truth output | Waveform behavior |
|---|---|---|
| Valid sum, zero sampled PE | Known zero PE; hit time may be absent | Pedestal/noise; waveform_valid=1 |
| Valid sum, positive PE and hit time | Known PE and modeled arrival times | Signal + pedestal/noise; waveform_valid=1 |
| Valid sum, positive PE but no hit time | PE retained; signal time unknown | Pedestal/noise retained; waveform_valid=0; gate-charge truth NaN |
| Hit exists but matching sum absent | sum_valid=0; mean/energy NaN; npe_poiss=-1 | Pedestal/noise retained; waveform_valid=0 |
| No hit or sum, full readout enabled | input_present=0, sum_valid=0; assumed no-signal mean=PE=0 | Pedestal/noise; waveform_valid=1 |

A missing input record is an explicit no-signal readout assumption, not proof of zero physical energy. `input_present` distinguishes it from an actual zero-energy sum. The last case adds an otherwise absent detector with copyid=0; it is a readout row, not an invented Geant4 hit.

## Configuration

Keep one active `detectors_list` line. For TS only use `detectors_list trigscint`; for all three use `detectors_list mollerpmt showermax trigscint`. Put TS response and electronics settings before `endconfig` in `db_gmn_conf_8gemmodules_GMN9.dat`. The contents of `trigscint_waveforms.dat.inc` are a paste-in snippet, not an automatically included file. All selected detectors use the same input event stream. Run from `MOLLER-digitisation` so relative CSV paths resolve.

```text
ts_det_ids 400 401 402 403 404 405 406 407 408 409 410 411 412 413
ts_pe_per_mev 125
ts_enable_electronics 1
ts_readout_all_channels 1
ts_store_waveforms 1
ts_store_pe_times 0
ts_calibration_file TS_channel_calibration.csv
ts_detector_map_file MOLLER_trigscint_electronics_map.csv
ts_gate_ns 400
ts_dt_ns 4
ts_gate_offset_ns 30
ts_range_v 1
ts_resistance_ohm 50
ts_nbits 12
ts_scint_decay_ns 2.1
ts_pulse_tau_ns 10
ts_sigma_time_ns 1
ts_transit_time_ns 0
ts_hit_time_mode 0
```

`ts_waveform_example.dat` is a complete TS-only test configuration with Rseed=12345; `ts_example.dat` remains the original PE-only example. Global optional keys without a calibration CSV are `ts_pedestal_mean_adc`, `ts_pedestal_sigma_adc`, `ts_pmt_gain`. Configure before booking tree branches; changing waveform-storage options after branch booking is unsupported.

The fixed acquisition gate starts at `-0.5*gate_ns + gate_offset_ns = -170 ns`. Sample centers are `gate_start_ns+(wf_samp+0.5)*dt_ns`; gate edges are -170 to +230 ns for this configuration. Waveform ADC values are positive-polarity pulse samples plus baseline, rounded to nearest count and clipped to 0..4095. This is a fixed gate, not a peak-centered acquisition window. No NSB/NSA pulse integration or peak finding occurs here; those belong in future TS replay. `adc_int` sums the whole acquired waveform, and `adc_int_pedsub` subtracts the configured baseline from every sample.

TS Poisson sampling and TS timing/noise have separate random streams. A fixed nonzero seed preserves PE counts when electronics/storage is enabled, and no MD/SM random generators are used. Seed 0 is ROOT's nondeterministic mode. Requesting PE-time storage does not add random draws.

## Build and test

From the MOLLER_Replay root:

```tcsh
setenv REMOLL_DIR /work/halla/moller12gev/sayak/remoll
cmake -S MOLLER-digitisation -B build/digitisation -DREMOLL_DIR="$REMOLL_DIR" -DBUILD_TESTING=ON
cmake --build build/digitisation -j4
ctest --test-dir build/digitisation -R trigger_scintillator --output-on-failure
```

The older response test remains; the new waveform test covers full readout, pedestal retention, charge conservation, ADC rounding/saturation, missing records/times, out-of-gate signals, exponential emission statistics, optional hit weighting, PE random-stream preservation, external CSVs, and integrated noise width. The assistant compiled/reran both tests with typed in-memory ROOT API stand-ins under address/undefined-behavior sanitizers (leak checking disabled because the container cannot support it). ROOT is unavailable here; run these tests against real ROOT on ifarm. Full-driver ROOT/REMOLL execution remains an ifarm check, not an assistant-claimed result.

## First isolated input test

From the framework root after building:

```tcsh
cd MOLLER-digitisation
mkdir -p verification_input
ln -s /w/halla-scshelf2102/moller12gev/sayak/remoll_TS/remollout.root verification_input/ts_waveform_verify.root
echo verification_input/ts_waveform_verify.root > filelist_TS_waveforms.txt
../build/digitisation/mollerdig ts_waveform_example.dat filelist_TS_waveforms.txt 100
```

Create the symlink once. The existing driver recreates the corresponding `_dig.root` output on reruns. This new alias isolates output from the older PE-only file. The input must contain compatible REMOLL `hit`, `sum` and `rate` branches. The run still links the configured REMOLL installation; use a ROOT/REMOLL dictionary compatible with the input file.

Check the output structurally:

```tcsh
root -l -b -q 'scripts/check_ts_waveforms.C+("verification_input/ts_waveform_verify_dig.root",100)'
```

Expected default full-readout sizes: 100 entries, 1400 channel-events and 140000 samples in a 100-event run. The checker reports unknown-energy and positive-PE/missing-time cases; invalid waveforms remain explicitly flagged, not interpreted as measured zero-charge signals. It checks branch alignment, uniqueness of sample indices, waveform sums versus ADC integrals, mean PE versus sum energy, generated charge, and gate-charge bounds. It does not validate physical light yield.

## Output and plotting

Tree name remains `trigscint_digi`, schema version 2. The original PE/hit branches remain. New scalar metadata includes gate start, sample spacing/count, ADC bit depth/range, resistance, charge per ADC-sample LSB, scintillation decay, SPE pulse tau, timing spread/offset/mode, requested seed, full-readout setting and electronics enable flag. Requested seed=0 does not record the resolved nondeterministic random state.

New aligned per-channel quantities include `input_present`, provisional electronics addresses, `pedestal_mean_adc`, `pedestal_sigma_adc`, `pmt_gain`, `qpe_c`, full-gate ADC sums, `waveform_valid`, `waveform_saturated`, `waveform_underflow`, `charge_generated_pc`, `charge_in_gate_pc`, `first_pe_time_ns`, `pe_time_valid`, and `timing_model`. Saturated/underflow flags mean any sample reached the upper/lower ADC rail, including exactly-at-rail samples. `charge_in_gate_pc` is noiseless injected signal truth before ADC clipping; it is not reconstructed charge. The difference from full generated charge includes finite waveform-gate losses.

Waveform vectors: `wf_detid`, `wf_copyid`, `wf_rocid`, `wf_slot`, `wf_channel`, `wf_samp`, `wf_adc`. Optional individual PE arrival vectors are `pe_detid`, `pe_copyid`, `pe_time_ns`; enable `ts_store_pe_times 1` for diagnostics, with larger files. PE times are generated truth, not extracted threshold times.

Open the file and enter ROOT statements on single lines:

```tcsh
root -l verification_input/ts_waveform_verify_dig.root
```

```cpp
TTree *ts=nullptr;
_file0->GetObject("trigscint_digi",ts);
ts->Scan("evnum:Length$(detid):Length$(wf_adc)","","",5);
ts->Scan("evnum:detid:npe_poiss:input_present:time_valid:waveform_valid:adc_int_pedsub:charge_in_gate_pc","npe_poiss>0","",10);
ts->Draw("wf_adc:(gate_start_ns+(wf_samp+0.5)*dt_ns)","Entry$==0 && wf_detid==403","LP");
ts->Draw("adc_int_pedsub>>hTSPed(100,-250,250)","npe_poiss==0 && waveform_valid==1");
```

The event-0/module-403 plot is an example based on the user's earlier input; choose an event with positive PE and valid timing if that input changes. X is ns, Y is raw ADC counts. For pedestal width 5 counts and 100 independent samples, the full-gate pedestal-subtracted distribution has mean near zero and width about 50 ADC-sum counts. Raw ADC-sum-to-charge factor is `(range_v / 2^nbits) * dt_ns * 1e-9 / resistance_ohm * 1e12`, or 0.01953125 pC/count at these settings. Negative pedestal-subtracted noise integrals are allowed.

A separate TS Podd detector/replay class, threshold/half-height extraction, and finite-window charge reconstruction are the next development phase; MD/SM replay scripts do not yet decode this TS tree.
