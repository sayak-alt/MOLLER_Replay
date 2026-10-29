#ifndef MOLLER_TRIGGER_SCINTILLATOR_DIGITIZER_H
#define MOLLER_TRIGGER_SCINTILLATOR_DIGITIZER_H

#include <TRandom3.h>
#include <TTree.h>
#include <map>
#include <string>
#include <vector>

// One effective readout per selected REMOLL detector/copy, not per PMT end.
// Parallel-world sum.edep remains a provisional scintillator energy proxy.
class MOLLERTriggerScintillatorDigitizer {
public:
  struct Config {
    std::vector<int> detector_ids = {400,401,402,403,404,405,406,407,408,409,410,411,412,413};
    double pe_per_mev = 125.0;
    bool enable_electronics = false; // Old PE-only operation remains the default.
    bool readout_all_channels = false;
    bool store_waveforms = false;
    bool store_pe_times = false;
    std::string calibration_file;
    std::string detector_map_file;
    double gate_ns = 400.0, dt_ns = 4.0, t_offset_ns = 30.0;
    double v_range_volt = 1.0, R_ohm = 50.0;
    int nbits = 12;
    double pedestal_mean = 819.2, pedestal_sigma = 5.0, pmt_gain = 1.0e6;
    double scint_decay_ns = 2.1; // Photon-emission exponential; no rise component.
    double pulse_tau_ns = 10.0; // PMT/electronics exponential charge response.
    double sigma_time_ns = 1.0, transit_time_ns = 0.0;
    int hit_time_mode = 0; // 0 earliest saved hit; 1 saved positive-hit-edep weights.
  };
  struct Hit {int det,copy,pid;double time_ns,edep_mev;};
  struct Deposit {int det,copy;double edep_mev;};
  void SetConfig(const Config& config);
  void SetRandomSeed(unsigned int seed);
  void SetEventNumber(Long64_t number) { fEvnum=number; }
  void BookBranches(TTree* tree);
  void DigitizeEvent(const std::vector<Hit>& hits,const std::vector<Deposit>& sums);
private:
  struct Calibration {double pedestal_mean,pedestal_sigma,gain;};
  struct Address {int roc,slot,channel,plane,sector;};
  Config fCfg;
  std::map<int,Calibration> fCalibration;
  std::map<int,Address> fMap;
  TRandom3 fRand, fElectronicsRand; // Separate PE stream preserves PE-only sampling.
  Long64_t fEvnum=0;
  unsigned int fRequestedSeed=0;
  int physical_scoring_validated=0, response_schema_version=2, fNSamples=100, fNChannels=14;
  double fGateStart=-170.0,fQLSB=0.0;
  std::vector<int> detid,copyid,time_valid,sum_valid,npe_poiss,input_present;
  std::vector<double> edep_mev,meanpe,first_hit_time_ns;
  std::vector<int> hit_detid,hit_copyid,hit_pid;
  std::vector<double> hit_time_ns,hit_edep_mev;
  std::vector<int> rocid,slot,channel,plane,sector,mapping_valid;
  std::vector<double> pedestal_mean_adc,pedestal_sigma_adc,pmt_gain,qpe_c;
  std::vector<int> adc_int,waveform_valid,waveform_saturated,waveform_underflow,timing_model;
  std::vector<double> adc_int_pedsub,charge_generated_pc,charge_in_gate_pc;
  std::vector<double> first_pe_time_ns;
  std::vector<int> pe_time_valid;
  std::vector<int> wf_detid,wf_copyid,wf_rocid,wf_slot,wf_channel,wf_samp;
  std::vector<unsigned short> wf_adc;
  std::vector<int> pe_detid,pe_copyid;
  std::vector<double> pe_time_ns;
};
#endif
