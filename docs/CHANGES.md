# Changes relative to the working flat replay prototype

1. Added a CMake-built libmoller.so and ROOT dictionary, rather than per-session
   ACLiC compilation of each source.
2. Split compiled classes into MOLLER-offline and run-time replay assets into
   MOLLER-replay. Included unchanged digitiser snapshot under MOLLER-digitisation.
3. Replaced MOLLERMDReco (THaPhysicsModule) with MOLLERMDDetector (THaDetector)
   owned by MOLLERApparatus (THaApparatus), registered in gHaApps.
4. MOLLERMDDecoder now loads ordered waveform samples into Podd THaSlotData;
   detector Decode reads the standard GetNumHits/GetData channel interface.
   Apparatus Reconstruct calls the detector's pulse reconstruction stage.
5. Replaced md_replay.conf with Podd DB/db_moller.md.dat, including all 224
   channels. Runtime setting DB_DIR selects the database directory.
6. Reconstruction output prefix changed from md.reco to moller.md, and added
   moller.md.nchannels. Counter variables md.input.* remain.
7. Updated replay macro, launcher, setup scripts and validation macro.
8. Reader's input validation and MDPulse.h algorithm are unchanged.

This is an architecture update, not a new pulse algorithm or change in physics.
No remoll source, generated PE, Poisson fluctuations, PMT gains, pedestal draws,
pulse shaping, sample timing or ADC saturation behavior was modified.

Remaining work: build/test this version with the user's Podd 1.7.12 on ifarm;
repeat numeric validation; evaluate low-PE threshold efficiency and false peaks;
add independently calibrated gain-to-PE if needed; later add optional remoll
truth joining with source-file/entry provenance. Current validation checks
injected PE charge, not independent raw-remoll truth. Real-data CODA decoding
and firmware fADC modes remain separate future work.
