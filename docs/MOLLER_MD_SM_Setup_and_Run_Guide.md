# MOLLER MD and Shower-Max: setup and running guide

**Baseline:** `sayak-alt/MOLLER_Replay`, commit `93617e8`  
**Date:** 28 October 2026  
**Environment tested:** JLab ifarm, tcsh, ROOT 6.30/04, Hall A Analyzer/Podd 1.7.12, Geant4 11.2.1, GCC 11.5.

Repository: https://github.com/sayak-alt/MOLLER_Replay

This guide covers the separate Main Detector (MD) and Shower-Max (SM) digitization and replay paths. It starts from existing REMOLL simulation ROOT files. It does not install REMOLL, ROOT, Geant4 or Podd, generate new REMOLL events, or include the provisional trigger-scintillator addition.

## 1. What each component does

| Component | Purpose | Product |
|---|---|---|
| REMOLL | Geant4 particle transport and detector scoring | Simulation ROOT tree `T` |
| `MOLLER-digitisation` | PE response, Poisson sampling, PMT charge, noise and sampled ADC | `mollerdig`; trees `pmt_digi` and/or `showermax_digi` |
| `MOLLER-offline` | MD ROOT reader, Podd run/decoder adapter, apparatus and reconstruction | `libmoller.so` |
| `MOLLER-offline/showermax` | Separate SM reader, Podd adapter, apparatus and reconstruction | `libmollershowermax.so` |
| `MOLLER-replay` | Replay steering macros, databases, launchers and validation | Replayed ROOT tree `T`, logs, validation plots |

Digitization and reconstruction are distinct. The replay uses the Hall A Podd framework with MOLLER-specific classes; it does not require SBS-offline. The simulation ROOT adapter is not a real CODA/EVIO hardware decoder.

The digitizer calculates expected PE by detector within each event, draws Poisson PE, and makes waveforms. Replay estimates the baseline/noise from samples, finds a pulse, integrates a configured window and reconstructs charge/time. Simulation truth is used afterward for validation, not to reconstruct the waveform.

## 2. Shell and prerequisites

Commands marked `tcsh` run at the **ifarm shell prompt**. Commands marked `cpp` run at the **ROOT prompt**. Do not paste shell commands into ROOT. At ROOT, use `.q` to return to the shell. Enter each ROOT statement on a single line.

Use an ifarm session with the standard experiment software environment already initialized. Check:

```tcsh
which root
which root-config
which analyzer
which cmake
root-config --version
```

The tested Analyzer installation is:

```text
/u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo
```

The tested REMOLL installation is:

```text
/work/halla/moller12gev/sayak/remoll
```

Its include directory and `lib64/libremoll.so` must exist for the current digitizer CMake setup. `REMOLL_DIR` is supplied explicitly to CMake; setting the environment variable alone is not sufficient for this CMakeLists. Use mutually compatible ROOT, compiler, Geant4 and REMOLL installations.

## 3. Clone and establish a stable working-directory variable

From a directory where you want a new checkout:

```tcsh
git clone https://github.com/sayak-alt/MOLLER_Replay.git MOLLER_Replay_verify
cd MOLLER_Replay_verify
setenv MOLLER_WORK "$cwd"
git log -1 --oneline
```

The tested commit is `93617e8`. A later `main` may contain changes. For exact baseline reproduction in a fresh, unmodified clone, use `git checkout 93617e8`; that selects a detached checkout.

Expected source directories are `MOLLER-digitisation`, `MOLLER-offline`, `MOLLER-replay` and `docs`. There is **no top-level CMakeLists.txt**. Therefore, do not run `cmake ..` from the generic `build` directory.

`MOLLER_WORK` is a convenience variable introduced by this guide. In a new shell, return to the framework root and set it again. It prevents commands from depending on which subdirectory you last entered.

## 4. Build the digitizer

Run from the framework root:

```tcsh
cd "$MOLLER_WORK"
setenv REMOLL_DIR /work/halla/moller12gev/sayak/remoll
cmake -S MOLLER-digitisation -B build/digitisation -DREMOLL_DIR="$REMOLL_DIR"
cmake --build build/digitisation -j4
ls -lh build/digitisation/mollerdig
```

The executable is `build/digitisation/mollerdig`. No installation step is required to run it. Stop at an actual configure/build error before running subsequent commands. The observed Geant4/PTL CMake deprecation warning did not prevent the tested build.

## 5. Build and install the replay libraries

From the framework root, first build MD:

```tcsh
cd "$MOLLER_WORK"
cmake -S MOLLER-offline -B build/offline -DPODD_PREFIX=/u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$MOLLER_WORK/install"
cmake --build build/offline -j4
cmake --install build/offline
```

Then SM:

```tcsh
cmake -S MOLLER-offline/showermax -B build/showermax -DPODD_PREFIX=/u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$MOLLER_WORK/install"
cmake --build build/showermax -j4
cmake --install build/showermax
ls -lh install/lib64/libmoller.so install/lib64/libmollershowermax.so
```

Optional source tests:

```tcsh
ctest --test-dir build/offline --output-on-failure
ctest --test-dir build/showermax --output-on-failure
```

SM reconstruction shares the timing/pulse helper from the MD source. Keep both components from the same framework revision. `CMAKE_INSTALL_PREFIX` avoids attempts to install into `/usr/local`.

Configure the runtime environment **from the root**:

```tcsh
cd "$MOLLER_WORK"
source setup.csh
```

This sets `MOLLER_OFFLINE`, `MOLLER_REPLAY`, library/include paths and a default MD `DB_DIR`. Select the appropriate database explicitly before each replay. Re-source this setup from this clone if another clone was used earlier in the shell. For a bash shell, use `setup.sh` and bash syntax instead; do not source it in tcsh.

## 6. Configuration and external files

Digitization configuration is `MOLLER-digitisation/db_gmn_conf_8gemmodules_GMN9.dat`. Keep configuration keys before `endconfig`. Replace existing keys rather than adding duplicates. Keep the rest of the supplied configuration, including unused GEM defaults; reading those defaults does not enable GEM digitization.

| Selection | Active configuration line |
|---|---|
| MD only | `detectors_list mollerpmt` |
| SM only | `detectors_list showermax` |
| Both | `detectors_list mollerpmt showermax` |

When both are enabled, both process the same input file/event stream. Use separate runs and file lists for different MD/SM simulation inputs. The documented replay launchers process MD and SM separately.

### MD PE response

```text
pe_method_mollerpmt 0
```

selects the existing Frank–Tamm approximation. Set it to `1` for the LUT response:

```text
pe_method_mollerpmt 1
lut_directory_mollerpmt look_up_table_MD
lut_ring_mollerpmt 0
```

`lut_ring_mollerpmt 0` loads all six rings; `1` through `6` restricts the response calculation to one ring. Full-channel readout can still produce pedestal-only waveforms for the other channels.

**The MD response tables are not bundled at commit 93617e8.** Supply these pairs for each requested ring:

```text
r1_PositionDepTable.csv   r1_EnergyDepTable.csv
r2_PositionDepTable.csv   r2_EnergyDepTable.csv
r3_PositionDepTable.csv   r3_EnergyDepTable.csv
r4_PositionDepTable.csv   r4_EnergyDepTable.csv
r5_PositionDepTable.csv   r5_EnergyDepTable.csv
r6_PositionDepTable.csv   r6_EnergyDepTable.csv
```

Position tables require the expected 13-column layout; energy tables have energy/scale columns, with headers. Do not silently rename an unrelated table to fit the filename. Preserve calibration provenance. These files are PE-response tables, not channel pedestal/gain calibrations.

The tested external table directory was:

```text
/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller-dig-MD/look_up_table_MD
```

### Full-channel waveform readout and calibration

MD settings:

```text
calibration_file_mollerpmt MD_channel_calibration.csv
detector_map_mollerpmt MOLLER_detector_electronics_map.csv
readout_all_channels_mollerpmt 1
store_waveforms_mollerpmt 1
```

SM settings:

```text
lut_directory_showermax showermax_tables
legacy_pion_names_showermax 1
calibration_file_showermax SM_channel_calibration.csv
detector_map_showermax MOLLER_showermax_electronics_map.csv
readout_all_channels_showermax 1
store_waveforms_showermax 1
```

SM response tables are bundled. Preserve `legacy_pion_names_showermax` for the bundled table convention unless intentionally changing and validating it.

Channel calibration supplies pedestal mean and per-sample Gaussian width in ADC counts, and dimensionless PMT gain. Charge per PE is elementary charge times gain. The class supports different gains per channel; **the bundled MD CSV currently sets all gains to 1,000,000**, while the SM CSV contains varying gains. Both bundled calibrations have per-sample width 5 ADC counts. Pedestal means vary by channel. These are simulation inputs, not measured detector calibrations.

With full readout enabled, channels with zero PE still receive pedestal/noise waveforms, including in otherwise empty events. At the default 400 ns gate and 4 ns sampling, MD has 224 × 100 = 22,400 samples/event and SM has 28 × 100 = 2,800 samples/event. Electronics maps use simulation hardware addresses; verify them before connecting to actual apparatus data.

For reproducible new stochastic runs, set a fixed nonzero `Rseed`; ROOT seed 0 is nondeterministic. Generated PE/noise need not match a previous seed-0 file exactly.

## 7. Prepare isolated verification inputs and configurations

Run digitization from `MOLLER-digitisation` so relative CSV/table paths resolve correctly. Each input ROOT path is listed on its own line in a text file. The current driver also requires the `rate` branch; event data are read from tree `T`.

The driver writes next to the input path, replacing `.root` with `_dig.root`. **It recreates that output, overwriting an existing file with the same name.** The separate symlinks below isolate verification outputs from earlier production outputs. Create the links once; if `ln` reports that a link already exists, inspect it rather than force-replacing it.

```tcsh
cd "$MOLLER_WORK/MOLLER-digitisation"
mkdir -p verification_input
```

### SM test setup

The following is the existing SM input used in the successful test:

```tcsh
ln -s /w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/SM_det/moller-dig-MD/moller-dig-final-MD-showermax/sm_moller_50k_10001.root verification_input/sm_verify.root
echo verification_input/sm_verify.root > filelist_SM_verify.txt
sed 's/^[[:space:]]*detectors_list[[:space:]].*/detectors_list showermax/' db_gmn_conf_8gemmodules_GMN9.dat > sm_verify.dat
```

### MD LUT test setup

Copy the external tables once into the fresh checkout:

```tcsh
cp -R /w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller-dig-MD/look_up_table_MD .
```

If the destination already exists, inspect it first; repeatedly copying directories can introduce an unwanted nested directory.

```tcsh
ln -s /w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/moller_digi_MD_map_updated/moller-dig-MD/o_moller.root verification_input/md_verify.root
echo verification_input/md_verify.root > filelist_MD_verify.txt
sed -e 's/^[[:space:]]*detectors_list[[:space:]].*/detectors_list mollerpmt/' -e 's/^[[:space:]]*pe_method_mollerpmt[[:space:]].*/pe_method_mollerpmt 1/' -e 's/^[[:space:]]*lut_directory_mollerpmt[[:space:]].*/lut_directory_mollerpmt look_up_table_MD/' db_gmn_conf_8gemmodules_GMN9.dat > md_verify.dat
```

These absolute input locations are Sayak's tested ifarm files. On another account/system, substitute existing REMOLL inputs and a compatible MD LUT source. Keep a separate writable verification output location.

## 8. Run digitization

From `MOLLER-digitisation`, run the detectors separately:

```tcsh
cd "$MOLLER_WORK/MOLLER-digitisation"
../build/digitisation/mollerdig sm_verify.dat filelist_SM_verify.txt 100
../build/digitisation/mollerdig md_verify.dat filelist_MD_verify.txt 100
```

Outputs are `verification_input/sm_verify_dig.root` and `verification_input/md_verify_dig.root`. The value `100` limits processed signal events across the listed inputs. Use explicit `-1` to process all available entries; keep the input/output names under control before a large run.

### Check SM digitized output

```tcsh
root -l verification_input/sm_verify_dig.root
```

```cpp
TTree *sm = nullptr;
_file0->GetObject("showermax_digi", sm);
std::cout << "Events: " << sm->GetEntries() << std::endl;
std::cout << "Incomplete events: " << sm->GetEntries("Length$(detid)!=28 || Length$(wf_adc)!=2800") << std::endl;
sm->Scan("detid:npe_poiss:pedestal_mean_adc:pedestal_sigma_adc:pmt_gain:adc_int_pedsub", "Entry$==0 && npe_poiss==0", "", 1);
sm->Draw("adc_int_pedsub>>hSMPed(100,-250,250)", "npe_poiss==0");
hSMPed->GetMean()
hSMPed->GetStdDev()
.q
```

Expect 100 entries and zero incomplete events. For independent noise of 5 ADC counts per sample over 100 samples, integrated noise width is approximately 50 ADC-sum counts. The first test measured mean 0.608 and standard deviation 50.927. A replay baseline estimated from 10 samples has expected residual width about 5/sqrt(10) = 1.58 ADC counts.

### Check MD digitized output

```tcsh
root -l verification_input/md_verify_dig.root
```

```cpp
TTree *md = nullptr;
_file0->GetObject("pmt_digi", md);
std::cout << "Events: " << md->GetEntries() << std::endl;
std::cout << "Incomplete events: " << md->GetEntries("Length$(detid)!=224 || Length$(wf_adc)!=22400") << std::endl;
md->Scan("evnum:detid:meanpe:npe_poiss", "meanpe>0", "", 20);
.q
```

Do not use a null tree pointer if the file/tree was not found. Check `_file0->ls()` for actual tree names. The driver can also create a GEM `T` tree even when no GEM detector is enabled; that is not evidence GEM digitization was requested.

## 9. Run Podd replay

Return to the root; never run these commands while still in ROOT:

```tcsh
cd "$MOLLER_WORK"
source setup.csh
```

### SM replay

```tcsh
setenv DB_DIR "$MOLLER_REPLAY/DB_showermax"
bash "$MOLLER_REPLAY/scripts/run_showermax_replay.sh" "$MOLLER_WORK/MOLLER-digitisation/verification_input/sm_verify_dig.root" "$MOLLER_WORK/replayed_sm_verify.root" 100
```

`DB_showermax/db_cratemap.dat` is the simulation crate-map file used to initialize Podd state. It is included in the tested update. SM slot data are supplied by the custom waveform decoder, not real raw hardware banks.

### MD replay

```tcsh
setenv DB_DIR "$MOLLER_REPLAY/DB"
bash "$MOLLER_REPLAY/scripts/run_replay.sh" "$MOLLER_WORK/MOLLER-digitisation/verification_input/md_verify_dig.root" "$MOLLER_WORK/replayed_md_verify.root" 100
```

Explicit `100` selects entries 0–99 in the updated macros. Explicit `-1` processes all entries present in the digitized file, not all events from the original simulation if only a subset was digitized. Reusing an output name can overwrite the previous replay file. Both commands also write logs alongside their output ROOT files.

For the optional MD timing database, select `setenv DB_DIR "$MOLLER_REPLAY/DB_timing"` before MD replay. Check that directory exists before using it. Always choose the MD/SM database explicitly when switching detectors.

## 10. What replay reconstructs and stores

Replay output tree is `T`. MD branches are under `moller.md.*`; SM branches are under `moller.sm.*`. Decoder summaries use `md.input.*` and `sm.input.*`. Results are associated with detector IDs and mapped channels; do not assume digitized and replay vector indices have the same ordering.

Stored quantities include baseline, noise estimate, found flag, peak height/sample, integration-window bounds, baseline-subtracted integral, charge in pC, timing definitions/validity, and clipping/saturation flags. SM also distinguishes configured channels from channels actually read out. Generated PE, injected calibration, and raw simulation rate weighting belong to digitizer truth, not an independent replay PE measurement.

The shared pulse algorithm estimates baseline and sample noise from the initial `baseline_samples` samples (10 in the supplied DBs). It selects a qualifying pulse with threshold above the baseline equal to max(threshold_adc, threshold_sigma × measured noise), optionally requiring consecutive samples. It currently reconstructs one selected pulse per channel rather than a complete multi-pulse train.

For an unclipped window, samples run from anchor − NSB through anchor + NSA − 1, inclusive. Thus NSB=5 and NSA=22 means 27 samples, or 108 ns at 4 ns sampling—not the full 400 ns waveform. `integration_reference` selects anchor 0=peak or 1=first above-threshold sample. Baseline is subtracted for each integrated sample.

Charge conversion is integral × (range_v / 2^bits) × dt_seconds / resistance_ohm × 10^12 pC. For 1 V, 12 bits, 4 ns and 50 ohms this is 0.01953125 pC per ADC-sum count.

`timing_reference`: 0=peak sample center, 1=interpolated threshold crossing, 2=interpolated rising half-height. The supplied default MD `DB` selects peak timing and peak-anchored integration; supplied SM `DB_showermax` selects half-height timing and threshold-anchored integration. Read the actual DB before interpreting `time_ns`. These are software approximations; the framework does not emulate every fADC250 firmware behavior.

Sample-center time = gate_start_ns + (sample + 0.5) × dt_ns. The supplied gate starts at −170 ns and contains 100 samples at 4 ns. A time_valid flag reports algorithm validity, not proof that a pulse is a real physics signal. `clipped` means the integration window exceeded the waveform; `saturated` flags an ADC sample at the configured maximum. Distinguish waveform-gate losses from integration-window clipping.

### Inspect replay outputs

```tcsh
cd "$MOLLER_WORK"
root -l replayed_sm_verify.root
```

```cpp
TTree *t = nullptr;
_file0->GetObject("T", t);
std::cout << "Events: " << t->GetEntries() << std::endl;
t->Scan("moller.sm.evnum:moller.sm.nchannels:moller.sm.nreadout:sm.input.nsamples", "", "", 5);
t->Scan("moller.sm.evnum:moller.sm.detid:moller.sm.charge_pc:moller.sm.time_ns:moller.sm.time_valid", "moller.sm.found==1", "", 20);
t->Draw("moller.sm.charge_pc>>hSMCharge(100,-5,50)", "moller.sm.found==1 && moller.sm.clipped==0 && moller.sm.saturated==0");
.q
```

For MD, open `replayed_md_verify.root` and use:

```cpp
TTree *t = nullptr;
_file0->GetObject("T", t);
t->Scan("moller.md.evnum:moller.md.nchannels:md.input.nsamples", "", "", 5);
t->Scan("moller.md.evnum:moller.md.detid:moller.md.charge_pc:moller.md.time_ns:moller.md.time_valid", "moller.md.found==1", "", 20);
t->Draw("moller.md.charge_pc>>hMDCharge(100,-5,50)", "moller.md.detid==110652 && moller.md.found==1 && moller.md.clipped==0 && moller.md.saturated==0");
.q
```

## 11. Validate against digitizer truth

Run from the framework root:

```tcsh
cd "$MOLLER_WORK"
root -l -b -q 'MOLLER-replay/scripts/validate_sm_charge.C+("MOLLER-digitisation/verification_input/sm_verify_dig.root","replayed_sm_verify.root","SM_verify_validation",0)'
root -l -b -q 'MOLLER-replay/scripts/validate_md_replay.C+("MOLLER-digitisation/verification_input/md_verify_dig.root","replayed_md_verify.root","MD_verify_validation")'
```

SM's final argument `0` selects all modules; use `73001` through `73028` to select one. The detailed SM validation writes ROOT/CSV products and summary PNG/PDF plots. With adequate statistics, export its six individual plots using:

```tcsh
root -l -b -q 'MOLLER-replay/scripts/plot_sm_replay_validation.C+("SM_verify_validation.root","SM_verify_validation.csv","SM_verify_plots")'
```

A 100-event sample can have too few populated charge bins for useful bias/spread plots. Use larger statistics for quantitative results. `validate_md_charge.C`, the later detailed MD charge script, is not bundled at this baseline; `validate_md_replay.C` is the available general validation.

Expected structural checks are matched events with no missing channels/waveforms. SM truth charge uses each channel/event's `npe_poiss × qpe_c × 10^12` pC. MD general validation uses stored gain and generated PE. Neither comparison is a direct comparison with original REMOLL optical truth. Full generated PE charge can extend beyond the acquired or integrated window. Detection selection, baseline uncertainty, noise and gate losses affect residuals. False detections are possible in zero-PE channels. Keep rate×PE separate from the unweighted electronics/charge simulation; rate normalization is an analysis choice, not a PE multiplier.

### Results from the completed fresh-clone checks

| Metric | MD LUT test | SM test |
|---|---:|---:|
| Matched input/replay events | 100 | 100 |
| Matched channel-events | 22,400 | 2,800 |
| Positive-PE channel-events | 46 | 42 |
| Detected positive-PE channels | 43 | 38 |
| Zero-PE false detections | 22 / 22,354 | 4 / 2,758 |
| Baseline residual mean / RMS [ADC] | 0.0158 / 1.5803 | Not reported in this charge summary |
| Selected charge residual mean / sample SD [pC] | Not reported in this summary | −0.0884 / 0.9994 |

These are small-sample functional checks, not precision detector efficiency, timing resolution or intrinsic charge-resolution measurements. New seed-0 runs can give different numbers. The completed checks do not demonstrate that the MD LUT matches the standalone code or that all physical response assumptions are correct.

## 12. Updating, troubleshooting and preserving the baseline

| Symptom | Check/action |
|---|---|
| CMake reports no top-level CMakeLists | Return to `$MOLLER_WORK`; use the explicit `-S` and `-B` commands above. |
| `/usr/local` permission denied | Reconfigure the correct build with `-DCMAKE_INSTALL_PREFIX="$MOLLER_WORK/install"`, then install. |
| `Illegal variable name` from setup.sh | You are in tcsh; source `setup.csh` and use `setenv`, not `export`. |
| Missing `libmoller.so` or `libmollershowermax.so` | Build/install the appropriate library and source this clone's setup. |
| Missing detector database | Print `echo "$DB_DIR"`; select `DB` for MD or `DB_showermax` for SM. |
| SM crate-map initialization failure | Ensure `DB_showermax/db_cratemap.dat` exists and the updated SM library is installed. |
| No SM tree | Check active `detectors_list` includes `showermax`; inspect `_file0->ls()`. |
| Sparse SM output despite full-readout key | Ensure the source parses both SM calibration/readout keys, rebuild the executable actually being run, and supply the complete SM map. |
| Missing MD LUT | Supply external CSVs and correct `lut_directory_mollerpmt`; run from the documented directory. |
| ROOT undeclared `t`, `md`, or `_file0` | Open the actual file, declare the tree pointer and retrieve the proper tree before use. |
| ROOT error on a line beginning `<<` | Paste the entire `std::cout` statement as one line. |
| Unexpected output location | Digitizer derives output from the input filename, not simply the launch directory. |
| Graph points skipped | Often empty efficiency bins; examine statistics before interpreting graphs. |
| Cling ABI warning | It was nonfatal in these runs; use compatible compiler/ROOT builds and investigate if compilation or execution fails. |

Rebuild digitization after changes to digitizer source. Rebuild and install the affected offline library after reconstruction/decoder changes. Database or configuration changes generally require rerunning, not recompiling, provided the relevant parser already exists. Do not change ADC range, sampling, resistance, bit depth or timing origin in only one side of the digitization/replay chain.

Preserve the tested commit and record configuration, calibration files, external LUT provenance, input files, seed and event count for every study. To make the MD LUT run self-contained, add the approved response tables to the repository or document a durable source and checksums. Commit source/configuration/calibration files selectively; exclude builds, installs, compiled ACLiC products, ROOT data and generated validation plots. Never commit access tokens.

Trigger scintillator development is separate and not part of the verified MD/SM baseline. Its proposed 400–413 parallel-world `sum.edep` response requires validation against physical scintillator scoring before physical PE conclusions.
