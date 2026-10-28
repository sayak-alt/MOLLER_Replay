# Shower-Max charge validation with channel-dependent gain

Copy `validate_sm_charge.C` and `SMChargeValidation.h` together into `MOLLER-replay/scripts/` in the working replay project. No offline-library rebuild or new replay is required.

Run from the `MOLLER_Replay` top directory in your usual ROOT environment (csh/tcsh single-quoted command):

```csh
root -l -b -q 'MOLLER-replay/scripts/validate_sm_charge.C+("/w/halla-scshelf2102/moller12gev/sayak/counting_mode_analysis/test_moller_dig_MD/SM_det/moller-dig-MD/moller-dig-final-MD-showermax/sm_moller_50k_10001_dig.root","replayed_showermax_all.root","SM73004_validation",73004)'
```

This checks module 73004 across all matching events. The fourth argument selects 73001..73028; use 0 to combine all modules. A ROOT file named `replayed_showermax_all.root` must exist in the current directory, or replace the second argument with its actual path.

Outputs in the current directory:

- `SM73004_validation.png` and `.pdf`: generated/replayed charge correlation with a y=x line, absolute residual, fractional residual and detection efficiency versus generated charge.
- `SM73004_validation.root`: the histograms and efficiency graph.
- `SM73004_validation.csv`: each selected event/module's generated/replayed charge, PE count and residual.

The macro requires the new digitiser's `qpe_c` vector<double> branch. It uses `Q_generated = npe_poiss * qpe_c * 1e12` pC. It never assumes all channels have the same gain, and does not use meanpe as realised PE truth. Truth values are used only for validation, never reconstruction.

Events are joined by input evnum / replay moller.sm.evnum, and channels by detector ID. Array ordering is not assumed. Duplicate event/module IDs are rejected. The summary reports unmatched events, missing replay channels and missing waveforms. Input reading disables the waveform branches to reduce I/O. No REMOLL library, replay database or libmollershowermax load is required for this validation.

Charge comparisons select positive generated PE, readout available, pulse found, unclipped and unsaturated. No cut is applied to reconstructed charge, so negative reconstructed values are retained. The 608 clean detections counted for module 73004 may include zero-PE false detections; the selected positive-PE charge-comparison count can therefore be smaller.

Detection efficiency uses all matched positive-PE channel-events in the selected module(s), including missing waveforms as not detected, and has no clipping/saturation cut. Zero-PE false-positive fraction uses zero-PE channels with an available waveform. Unmatched events/channels are reported and excluded; resolve unexpected mismatches before interpreting the plots. Binomial Clopper-Pearson intervals are used in the efficiency graph.

Interpretation:

- Near the y=x line means agreement with full generated charge.
- Residual is Q_replay - Q_generated, in pC. Negative values mean reconstructed charge is smaller.
- Fractional residual is 100*(Q_replay-Q_generated)/Q_generated, in percent.
- This full generated charge is NOT the exact expected charge inside the finite replay integration window. Window/gate losses, baseline estimation, rounding and electronic noise all contribute to residuals.
- The charge residual sample is conditional on detection, with threshold selection bias at small charge. Its global spread mixes charges and potentially channels; it is not a fitted or charge-independent detector resolution.
- Compare residual trends versus generated charge and individual modules before making a resolution claim. Timings are not validated by this macro.

The original `validate_showermax_replay.C` is not overwritten. MD and SM reconstruction, digitisation and databases are untouched.

Verification here: local C++ tests passed for reordered channel matching, duplicate rejection, charge calculation with different gains, zero-PE truth and sample standard deviation. ROOT is unavailable in the creation environment, so ACLiC compilation and real ROOT I/O must be checked on ifarm. If that command fails, share the first compilation/runtime error.
