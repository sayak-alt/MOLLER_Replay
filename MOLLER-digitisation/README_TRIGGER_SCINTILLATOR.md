# Trigger scintillator PE prototype

Separate optional digitizer. Add `trigscint` to the existing `detectors_list` line; for TS only use `detectors_list trigscint`. Before `endconfig`, set `ts_pe_per_mev 125`. Default is 125 PE/MeV. Optional `ts_det_ids 400 401 402 403 404 405 406 407 408 409 410 411 412 413` overrides the default selection. Positive finite yield and unique positive IDs are required. `Rseed` is shared configuration, but TS has a separate random generator; ROOT seed 0 is nondeterministic. The existing MD and SM class files are unchanged.

For every input event, selected records are grouped by (det, copy), using `sum.det`, `sum.vid`, `sum.edep` in MeV and `hit.det`, `hit.id`, `hit.t` in ns. For each sum, meanpe = 125 * edep and npe_poiss is drawn once from Poisson(meanpe). All selected hits, including zero-edep hits, are retained; first_hit_time_ns is earliest finite recorded hit time for the matching key. No sum yields sum_valid=0, edep and meanpe NaN, npe_poiss=-1. No matching hit yields time_valid=0 and first_hit_time_ns NaN. sum_valid=1 with edep=0 yields meanpe=npe_poiss=0. Empty events have empty vectors; this does not simulate 14-channel pedestal readout. Check the assumed hit.id/sum.vid correspondence on your REMOLL input. `evnum` is the existing driver's global processed-entry count, not necessarily an original simulation event identifier. Output is `trigscint_digi` with per-key branches detid, copyid, sum_valid, edep_mev, meanpe, npe_poiss, first_hit_time_ns, time_valid, and hit_detid, hit_copyid, hit_pid, hit_time_ns, hit_edep_mev.

EJ-200 manufacturer nominal electron response is 10,000 photons/MeV and decay 2.1 ns; https://eljentechnology.com/products/plastic-scintillators/ej-200-ej-204-ej-208-ej-212 . REMOLL develop commit 9e22b834cb12698d3a4974f2822787553869b648 `geometry/tracking/trigscintMaterials.xml` references constants in `trigscintMatrices.xml`: yield 10200/MeV and components 2.5 ns and 4.5 ns, weights 1.0 and 0.0. This code does not simulate photon emission or apply a decay-time model. The 125 PE/MeV is an initial effective calibration (equivalent to 1.25% of 10000 photons/MeV), not a measured collection efficiency.

Caution: IDs 400–406 and 407–413 are upstream/downstream virtual parallel-world detectors in `geometry/mollerParallel.gdml` and their ghost material is G4_Galactic. Their reported sum.edep must not be assumed to equal physical plastic deposited energy. Validate event-by-event against sensitive physical scintillator scoring, including module mapping, particle species and geometry, before interpreting PE physically. A zero PE result may reflect zero recorded parallel-world energy, not zero physical light. Hit time is simulation truth, not PMT/FADC time. No step-level dE/dx, Birks quenching, optical transport, two-ended readout, waveforms or electronics are modeled. Optical photon energy double counting must be assessed if optical physics is enabled.

Build using the normal ROOT/Geant4/REMOLL setup, e.g. `cmake -S . -B build -DREMOLL_DIR=/path/to/remoll/install` then `cmake --build build -j4`. `REMOLL_DIR` must contain include and lib64/libremoll.so per CMakeLists. Run from MOLLER-digitisation: `./build/mollerdig config.dat filelist.txt 100`. filelist contains ROOT paths; output is each input path with `.root` replaced by `_dig.root`, and may overwrite an existing output. The existing driver requires `hit`, `sum`, and `rate` branches. In ROOT: `TTree *ts=nullptr; _file0->GetObject("trigscint_digi",ts); ts->Scan("evnum:detid:copyid:sum_valid:edep_mev:meanpe:npe_poiss:first_hit_time_ns:time_valid","","",100);`. Compare `sum.det:sum.vid:sum.edep` and `hit.det:hit.id:hit.t` directly on the corresponding input event; verify nonzero energy and association before further analysis.

## Verified MD/SM baseline and TS installation

This integration is based on MOLLER_Replay commit `93617e8`, which passed the user's MD/SM fresh-clone 100-event digitization/replay checks. MD and SM class files are untouched. Only the central driver and digitizer CMakeLists are edited, with new TS files added. TS is PE/truth-time only in this first implementation; it does not generate the waveform required by existing MD/SM replay.

The example `ts_example.dat` is a complete minimal TS-only configuration with fixed seed 12345. With the framework root as the launch directory:

```tcsh
setenv REMOLL_DIR /work/halla/moller12gev/sayak/remoll
cmake -S MOLLER-digitisation -B build/digitisation -DREMOLL_DIR="$REMOLL_DIR"
cmake --build build/digitisation -j4
ctest --test-dir build/digitisation -R trigger_scintillator_response --output-on-failure
```

The added CTest executable uses actual ROOT TTree read/write in memory to check branch I/O, detector/copy associations, zero/absent energy records, time validity, event reset, invalid input, and Poisson mean/variance. ROOT is unavailable in the assistant runtime, so that test is supplied for ifarm, not claimed passed here. Independent response/control-flow checks compiled with lightweight ROOT stand-ins and passed. No physical REMOLL input has been exercised here.

Run from `MOLLER-digitisation`:

```tcsh
../build/digitisation/mollerdig ts_example.dat filelist_TS.txt 100
```

`filelist_TS.txt` must list actual simulation input containing selected TS hit/sum records. Use a separately named input symlink under `verification_input` so `_dig.root` does not overwrite your established MD/SM outputs. Preserve the input symlink's parent as a writable directory.

Every output event also stores `pe_per_mev` and `physical_scoring_validated` (fixed at 0). Absence of hits is not fabricated into a hit time. Do not pass npe_poiss=-1 (missing sum) to positive/zero-PE physics selections as a measurement.

Before digitization, inspect your REMOLL file using its compatible reroot:

```cpp
TTree *input=nullptr;
_file0->GetObject("T",input);
input->Scan("sum.det:sum.vid:sum.edep","sum.det>=400 && sum.det<=413","",20);
input->Scan("hit.det:hit.id:hit.t:hit.edep","hit.det>=400 && hit.det<=413","",20);
```

Check all requested detector IDs, finite nonnegative energy sums, and correspondence of hit.id with sum.vid. A hit can legitimately have zero step energy, so stored time is taken from the earliest selected hit, not only positive-edep hits. Records are grouped by detector and immediate copy ID. This preserves information but does not assert that copy ID alone provides the validated physical module or PMT mapping. The code rejects duplicate sums for the same key rather than silently double counting them.

After digitization, open the reported output and check:

```cpp
TTree *ts=nullptr;
_file0->GetObject("trigscint_digi",ts);
std::cout << "Events: " << ts->GetEntries() << std::endl;
ts->Scan("evnum:detid:copyid:sum_valid:edep_mev:meanpe:npe_poiss:first_hit_time_ns:time_valid","","",20);
std::cout << "Response inconsistencies: " << ts->Draw("meanpe","sum_valid==1 && abs(meanpe-edep_mev*pe_per_mev)>1e-9","goff") << std::endl;
```

Expect zero response inconsistencies, not an exact equality of npe_poiss with meanpe. There is no requirement for 14 output rows/event in this PE-only prototype. All input events remain represented, including empty selected-response events.
