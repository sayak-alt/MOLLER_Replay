# Basis for the package structure

Reviewed uploaded SBS-offline-master, SBS-replay-master, libsbsdig-master and
g4sbs-master sources. This package is MOLLER-specific code, not copied SBS
apparatus or SBS replay macros.

| SBS source responsibility | MOLLER implementation |
|---|---|
| G4SBS simulation I/O | External remoll simulation and existing digitiser input |
| libsbsdig PMT/electronics digitisation | Existing MOLLERPMTDigitizer in MOLLER-digitisation |
| SBSSimFile/decoder simulation adapter | MOLLERMDReader, MOLLERMDRun, MOLLERMDDecoder |
| Decoder channel data | Ordered samples in Podd THaSlotData, addressed by ROC/slot/channel |
| Apparatus in gHaApps | MOLLERApparatus |
| Detector Decode and reconstruction | MOLLERMDDetector Decode and Reconstruct |
| SBS-replay DB, macros, output definitions | MOLLER-replay DB, replay and scripts |

Differences are deliberate: input is the existing pmt_digi TTree; transport uses
an in-process event pointer; the decoder does not encode CODA or simulated fADC
module words; detector uses generic sample-hit storage; pulse reconstruction
retains the already tested maximum-based NSB/NSA logic. No tracking, GEM, SM,
helicity reconstruction, run-dependent conditions, or energy calibration is
introduced. The simulation reader currently validates known digitiser integral
branches for integrity, but detector reconstruction only receives ADC samples.
