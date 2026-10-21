# Separate Shower-Max replay add-on

Apply to your existing working MOLLER_Replay directory with the MD timing update installed.
Copy the new `MOLLER-offline/showermax` directory into `MOLLER-offline/`, and merge the supplied `MOLLER-replay` directories into `MOLLER-replay/`. All filenames are new. No MD digitiser, MD replay source, setup file or existing database is replaced.

## What changes

A separate `libmollershowermax.so` provides MOLLERShowerMaxReader, MOLLERShowerMaxRun, MOLLERShowerMaxDecoder, MOLLERShowerMaxApparatus and MOLLERShowerMaxDetector. It uses Podd's THaAnalyzer, THaRunBase, THaEvData, THaSlotData, THaApparatus and THaDetector. It does not require SBS-offline or REMOLL libraries to replay the already digitised tree.

The shared existing `MOLLER-offline/include/MDPulse.h` supplies the same baseline, peak search, charge integration and software timing algorithm as the working MD replay. It is not changed or copied by this add-on. The Shower-Max database independently sets its parameters.

The input tree is `showermax_digi`, matching the supplied schema: ADC values are vector<unsigned short>, raw integrals vector<int>, electronics addresses vector<int>, rates double. Each available waveform must have exactly samples 0–99, valid addresses and a matching raw ADC sum. The CSV and database use 73001–73028: ROC 2, slot 3 channels 0–15, slot 4 channels 0–11. Change both maps together if your actual association differs.

Input can have zero response channels, a response channel without a waveform, or one or more complete waveforms. Empty input events remain output events. The output has 28 mapped rows per event, **not 28 manufactured measurements**:

- `moller.sm.readout==1`: an actual waveform was present; reconstruction ran.
- `moller.sm.readout==0`: no waveform was present; charge, baseline and noise are NaN, found is 0 and software time_quality has value 32 (missing waveform). Do not interpret these rows as measured zero signal.
- `moller.sm.nchannels` is always 28; `moller.sm.nreadout` is the available waveform count.
- `sm.input.nresponse_channels` counts input response entries, including any without a waveform. `sm.input.nwaveform_channels` counts actual waveforms; `sm.input.nsamples` is 100 times that count.
- `sm.input.event_rate_hz` and `sm.input.rate_GHz` preserve event metadata. Rates do not enter pulse reconstruction.

The digitiser's PE truth stays in the original file. It is used only by the validation macro. No lookup table is needed by replay: its effect is already contained in the simulated waveform.

## Build (csh/tcsh)

Run from the existing `MOLLER_Replay` top directory, after copying the add-on:

```csh
cmake -S MOLLER-offline/showermax -B build/showermax -DPODD_PREFIX=/u/group/halla/apps/analyzer/1.7.12/el9/RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$cwd/install"
cmake --build build/showermax -j4
ctest --test-dir build/showermax --output-on-failure
cmake --install build/showermax
source setup.csh
setenv DB_DIR "$MOLLER_REPLAY/DB_showermax"
```

Always select DB_DIR **after** sourcing setup.csh, which otherwise selects the MD database. This build creates a separate library and leaves libmoller.so intact. Start a fresh analyzer process for each SM replay.

## First run

Set SM_INPUT to the actual digitised Shower-Max file. This is the filename used earlier in this work; check that it exists:

```csh
setenv SM_INPUT "/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/SM_det/moller-dig-MD/moller-dig-final-MD-showermax/sm_moller_50k_10001_dig.root"
ls -l "$SM_INPUT"
root -l -b -q "$MOLLER_REPLAY/scripts/check_showermax_reader.C+(\"$SM_INPUT\",100)"
bash "$MOLLER_REPLAY/scripts/run_showermax_replay.sh" "$SM_INPUT" "$cwd/replayed_showermax_100.root" 100
```

If the file was moved or renamed, update only SM_INPUT. The first command validates the actual tree, mapping, complete waveform samples and raw sums. The second reconstructs exactly 100 input entries (last event index 99). To process all entries:

```csh
bash "$MOLLER_REPLAY/scripts/run_showermax_replay.sh" "$SM_INPUT" "$cwd/replayed_showermax_all.root" -1
```

The existing GEM-style calling convention is also available after loading the library and macro in analyzer:

```cpp
gSystem->Load(TString::Format("%s/lib64/libmollershowermax.so",gSystem->Getenv("MOLLER_OFFLINE")));
.L MOLLER-replay/replay/replay_moller_showermax.C
replay_moller_showermax("full/path/to/input.root",100);
```

This two-argument form writes moller_showermax_dig_replayed.root in OUT_DIR, or the current directory when OUT_DIR is unset. Prefer the shell launcher when choosing an explicit output filename.

## Inspect output

```csh
root -l replayed_showermax_100.root
```

Then in ROOT:

```cpp
TTree* t=nullptr;
_file0->GetObject("T",t);
t->Scan("moller.sm.evnum:moller.sm.nchannels:moller.sm.nreadout:sm.input.nsamples","","",10);
t->Scan("moller.sm.detid:moller.sm.readout:moller.sm.found:moller.sm.charge_pc","moller.sm.readout==1","",10);
t->Draw("moller.sm.charge_pc>>hSMCharge(100,-5,50)","moller.sm.readout==1 && moller.sm.found==1");
// One module:
t->Draw("moller.sm.charge_pc>>h73004(100,-5,50)","moller.sm.detid==73004 && moller.sm.readout==1 && moller.sm.found==1");
// Timing (ns), only when valid:
t->Draw("moller.sm.time_ns","moller.sm.readout==1 && moller.sm.time_valid==1");
```

For the supplied scan, entries 0–6 should show nreadout=0 and nsamples=0; entries 7 and 9 should show nreadout=1 and nsamples=100. All retain nchannels=28. A pulse detection is not guaranteed just because a waveform exists.

## Validate

From the top directory after setting DB_DIR:

```csh
root -l -b -q "$MOLLER_REPLAY/scripts/validate_showermax_replay.C+(\"$SM_INPUT\",\"$cwd/replayed_showermax_100.root\",\"SM_validation_100\")"
```

This matches by original event number and detector ID, checks availability and rate preservation, and creates SM_validation_100.png. Charge comparisons select readout channels with positive generated PE, a found pulse and no clipping/saturation. Default full generated charge is N_PE times 1.6e-13 C, matching the original SM digitiser's default Qpe_C. If your digitiser uses another value, pass it as the fourth argument. This is not charge truth limited to the finite integration window; residuals include gate/integration losses, baseline noise and detection selection.

Your present SM digitiser omits absent/zero-PE waveforms. Therefore this file cannot validate pedestal-only readout across all 28 channels or the unconditional noise false-positive rate. Producing those measurements would require a separate digitiser change; this add-on does not alter digitisation.

## Database parameters

DB_showermax/db_moller.sm.dat assumes the supplied electronics defaults: 100 samples at 4 ns, sample-window start -170 ns, 12 bits, 1 V full scale, 50 ohm. Baseline uses the first 10 samples. Detection uses max(20.48 ADC, 5 times estimated noise), with one sample above threshold. NSB=5 and NSA=22 define a 27-sample integration window anchored to the first threshold sample. timing_reference=2 selects interpolated rising half-height time; threshold crossing and peak time are also stored with validity flags. These are software estimates sharing the MD implementation, not a bit-exact fADC firmware emulation. Match these settings to the digitiser before interpreting charge or time.

## Verification performed here

The pure C++ sparse-event test passes for empty events, response entries without waveforms, complete pedestal waveforms, and rejection of incomplete samples, wrong sums, duplicate IDs, wrong addresses and ADC overflow. Shell syntax and static map/output-definition checks pass. ROOT, Podd and CMake are unavailable in the creation environment; the library and ROOT macros still require compilation and the 100-event test on ifarm before full production replay.
