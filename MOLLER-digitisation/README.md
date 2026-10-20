# SM digitisation: full readout, pedestal and gain

Only Shower-Max digitisation is updated. MD sources, LUT response and replay sources are unchanged.

## Apply

From your digitiser project root, replace these files with the supplied versions:

- include/MOLLERShowerMaxDigitizer.h
- src/MOLLERShowerMaxDigitizer.cxx

Copy SM_channel_calibration.csv, MOLLER_showermax_electronics_map.csv and the supplied scripts into the same project, preserving their relative paths. The map preserves the current association (ROC 2; slots 3/4).

Add two configuration handlers to your existing driver without replacing the rest of it:

```sh
python3 scripts/patch_sm_config_reader.py src/mollerdig.cxx
```

The helper finds the existing store_waveforms_showermax handler, inserts only the two lines below, and backs up the original driver. It stops if the expected code is missing or a partial patch exists. You can instead insert these lines manually beside the existing Shower-Max handlers:

```cpp
if(skey=="readout_all_channels_showermax") { showerMaxCfg.readout_all_channels = ((TObjString*)(*tokens)[1])->GetString().Atoi()!=0; }
if(skey=="calibration_file_showermax") { showerMaxCfg.calibration_file = ((TObjString*)(*tokens)[1])->GetString().Data(); }
```

In your existing configuration .dat, add or update the following keys **before endconfig**, avoiding duplicates:

```text
readout_all_channels_showermax 1
calibration_file_showermax SM_channel_calibration.csv
detector_map_showermax MOLLER_showermax_electronics_map.csv
store_waveforms_showermax 1
```

Keep all other electronics/LUT settings. For SM only, set the existing detector selection to:

```text
detectors_list showermax
```

To digitise MD and SM together, use `detectors_list mollerpmt showermax` and retain your MD configuration. CSV/LUT relative paths are resolved from the directory where you launch digitisation, just as before.

## Build and run (your csh/tcsh terminal)

From the digitiser root:

```csh
setenv REMOLL_DIR /work/halla/moller12gev/sayak/remoll
cmake -S . -B build -DREMOLL_DIR="$REMOLL_DIR"
cmake --build build -j4
./build/mollerdig db_gmn_conf_8gemmodules_GMN9.dat filelist_SM.txt 100
```

`filelist_SM.txt` lists the original REMOLL input ROOT files, one per line. Keep your current output path setting; the output filename is controlled by the existing driver/configuration. Once the 100-event check passes, use your desired event count or the existing all-events option (`-1`). Avoid overwriting a digitised file you still need for comparison.

## What is generated

Each event includes all 28 SM modules, sorted by detector ID/segment. At the default 400 ns gate and 4 ns sampling, this means 2,800 stored ADC samples per event, including empty REMOLL events. The all-channel option requires the complete 28-module electronics map. The new calibration CSV is optional; omit calibration_file_showermax to use the existing global pedestal and Qpe settings while still reading out all mapped modules.

Every selected module draws N_PE from the same Poisson model applied to the same event/module summed LUT mean. Zero-PE modules skip only signal-pulse addition: they still generate pedestal/noise, ADC rounding/clipping and full-gate integrals. The exponential pulse, timing reference, jitter and gate definition are unchanged. Extra random draws and sorted channel order mean identical seeds do not reproduce the old event-by-event random values.

Per-channel pedestal constants and gains are loaded once, not randomised each event. Each sample receives an independent Gaussian pedestal fluctuation. Signal charge per PE is `qpe_c = 1.602176634e-19 * pmt_gain`, as in MD. Noise is added before ADC rounding and clipping.

The sample calibration file contains **synthetic demonstration values**, sorted by segment:

- pedestal means fixed per channel in the range 190–210 mV, converted to ADC counts for 1 V full scale and 12 bits (200 mV is 819.2 counts);
- pedestal sigma 5 ADC counts per sample (about 1.22 mV at those settings);
- gains fixed per channel between 0.9e6 and 1.1e6.

Edit these values for your intended calibration. They are not measured detector constants. If you change ADC full scale/bit depth, convert pedestal means and widths accordingly. With independent sample noise and no clipping, a 100-sample pedestal-subtracted integral has approximate sigma sqrt(100)*5 = 50 ADC-sum counts; its mean is approximately zero, with small ADC quantisation effects possible.

New calibration metadata branches, aligned with detid: `pedestal_mean_adc`, `pedestal_sigma_adc`, `pmt_gain`, `qpe_c`, all vector<double>. Existing `adc_int` stays vector<int>. **adc_int_pedsub changes to vector<double>**, matching MD and retaining fractional pedestal subtraction. Readers that explicitly bind that branch as vector<int> must update; the supplied SM replay reader only binds raw adc_int and is unaffected.

Full-gate pedestal subtraction is sum(ADC) - N_samples*pedestal_mean_adc. Replay still estimates the baseline and chooses its own integration window; no peak finding was added here. MeanPE and Poisson PE truth do not depend on pedestal/gain. Added zero-PE rows do increase uncut histogram entries.

The full-readout population is the complete map. As in the current MD implementation, the map enabled field remains stored metadata; it does not remove channels from this all-channel acquisition. This update does not change that policy.

## Check your regenerated ROOT file

For output o_moller_dig.root:

```csh
root -l -b -q 'scripts/check_sm_full_readout.C+("o_moller_dig.root",100)'
```

This checks all channel IDs, unique sample indices, raw ADC sums and fractional pedestal subtraction, including zero-PE channels.

Interactive ROOT:

```cpp
TFile* f=TFile::Open("o_moller_dig.root");
TTree* sm=nullptr;
f->GetObject("showermax_digi",sm);
sm->Scan("evnum:Length$(detid):Length$(wf_adc)","","",10);
sm->Draw("adc_int_pedsub>>hSMPedestal(100,-200,200)","detid==73004 && npe_poiss==0");
```

Expect 28 and 2800 in every row. Use one module for a pedestal check so channel-specific distributions are not mixed.

For later charge validation, use **N_PE*qpe_c*1e12** as full generated charge in pC. The earlier SM replay validation macro used constant Qpe by default; that constant is not valid when this sample calibration varies gains across modules. The replay reconstruction itself does not use generated truth and needs no change for these added pedestal waveforms.

## Verification limits

A local C++ test with ROOT API stand-ins passes full empty-event readout, signal plus pedestal, event clearing, sparse readout, unchanged LUT mean summation and exact fractional pedestal bookkeeping. The production LUT source is compiled unchanged in that test. The stand-in RNG is the C++ standard generator, so this is not a test of ROOT's random-number implementation or ROOT I/O. ROOT/REMOLL are unavailable locally; rebuild and run the supplied ROOT check on ifarm.

Optional stand-in test, from your project root after copying the supplied tests directory:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -Iinclude src/MOLLERShowerMaxDigitizer.cxx src/MOLLERShowerMaxLookupResponse.cxx tests/test_sm_readout.cpp -o /tmp/test_sm_readout
/tmp/test_sm_readout . .
```

The tests/stubs directory is only for this explicit test command. Do not add it to the production CMake include paths.
