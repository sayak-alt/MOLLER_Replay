# TS waveform development: one effective readout per detector (not per PMT end).
# Add these keys BEFORE endconfig in your existing .dat file.
# Add trigscint to the existing single detectors_list line.
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
