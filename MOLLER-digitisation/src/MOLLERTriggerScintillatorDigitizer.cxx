#include "MOLLERTriggerScintillatorDigitizer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {
constexpr double electron_charge_c=1.602176634e-19;
const double missing=std::numeric_limits<double>::quiet_NaN();
std::string trim(const std::string& s) {
  auto a=s.find_first_not_of(" \t\r\n");
  if(a==std::string::npos)return {};
  return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
std::vector<std::vector<std::string>> csv(const std::string& path,const std::string& header,size_t columns) {
  std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open TS CSV: "+path);
  std::string line;if(!std::getline(f,line)||trim(line)!=header)throw std::runtime_error("Unexpected TS CSV header: "+path);
  std::vector<std::vector<std::string>> rows;
  while(std::getline(f,line)) {
    line=trim(line);if(line.empty()||line[0]=='#')continue;
    std::vector<std::string> fields;size_t start=0;
    while(true){auto comma=line.find(',',start);fields.push_back(trim(line.substr(start,comma==std::string::npos?comma:comma-start)));if(comma==std::string::npos)break;start=comma+1;}
    if(fields.size()!=columns)throw std::runtime_error("Wrong TS CSV column count: "+path);
    rows.push_back(std::move(fields));
  }
  return rows;
}
double number(const std::string& s) {
  size_t n=0;double x=std::stod(s,&n);
  if(n!=s.size()||!std::isfinite(x))throw std::runtime_error("Invalid TS CSV number: "+s);
  return x;
}
int integer(const std::string& s) {
  double x=number(s);
  if(x!=std::floor(x)||x<std::numeric_limits<int>::min()||x>std::numeric_limits<int>::max())throw std::runtime_error("Invalid TS CSV integer: "+s);
  return static_cast<int>(x);
}
}

void MOLLERTriggerScintillatorDigitizer::SetConfig(const Config& cfg) {
  if(cfg.detector_ids.empty())throw std::runtime_error("TS requires selected detector IDs");
  std::set<int> ids;
  for(int id:cfg.detector_ids)if(id<=0||!ids.insert(id).second)throw std::runtime_error("Invalid/duplicate TS ID");
  auto positive=[](double x){return std::isfinite(x)&&x>0;};
  auto nonnegative=[](double x){return std::isfinite(x)&&x>=0;};
  if(!positive(cfg.pe_per_mev)||!positive(cfg.gate_ns)||!positive(cfg.dt_ns)||!positive(cfg.v_range_volt)||!positive(cfg.R_ohm)||
     cfg.nbits<1||cfg.nbits>16||!std::isfinite(cfg.t_offset_ns)||!nonnegative(cfg.pedestal_mean)||!nonnegative(cfg.pedestal_sigma)||
     !positive(cfg.pmt_gain)||!nonnegative(cfg.scint_decay_ns)||!positive(cfg.pulse_tau_ns)||!nonnegative(cfg.sigma_time_ns)||
     !std::isfinite(cfg.transit_time_ns)||cfg.hit_time_mode<0||cfg.hit_time_mode>1)
    throw std::runtime_error("Invalid TS response/electronics configuration");
  double samples=cfg.gate_ns/cfg.dt_ns,top=std::pow(2.0,cfg.nbits)-1;
  if(!std::isfinite(samples)||samples<1||samples>100000||std::abs(samples-std::round(samples))>1e-8||samples*top>std::numeric_limits<int>::max()||cfg.pedestal_mean>top)
    throw std::runtime_error("Invalid TS waveform sample count/pedestal range");
  if((cfg.store_waveforms||cfg.store_pe_times)&&!cfg.enable_electronics)
    throw std::runtime_error("TS waveform/PE-time storage requires ts_enable_electronics 1");
  std::map<int,Calibration> calibration;
  if(!cfg.calibration_file.empty()) {
    for(const auto& row:csv(cfg.calibration_file,"detid,pedestal_mean_adc,pedestal_sigma_adc,pmt_gain",4)) {
      int id=integer(row[0]);Calibration c{number(row[1]),number(row[2]),number(row[3])};
      if(id<=0||c.pedestal_mean<0||c.pedestal_mean>top||c.pedestal_sigma<0||c.gain<=0||!calibration.emplace(id,c).second)
        throw std::runtime_error("Invalid/duplicate TS calibration channel");
    }
    for(int id:ids)if(!calibration.count(id))throw std::runtime_error("TS calibration missing selected channel");
  }
  std::map<int,Address> mapping;std::set<std::tuple<int,int,int>> addresses;
  if(!cfg.detector_map_file.empty()) {
    for(const auto& row:csv(cfg.detector_map_file,"detid,rocid,slot,channel,plane,sector",6)) {
      int id=integer(row[0]);Address a{integer(row[1]),integer(row[2]),integer(row[3]),integer(row[4]),integer(row[5])};
      if(id<=0||a.roc<0||a.roc>255||a.slot<0||a.slot>31||a.channel<0||a.channel>15||a.plane<0||a.plane>1||a.sector<1||a.sector>7||
         !mapping.emplace(id,a).second||!addresses.emplace(a.roc,a.slot,a.channel).second)
        throw std::runtime_error("Invalid/duplicate TS electronics address");
    }
    for(int id:ids)if(!mapping.count(id))throw std::runtime_error("TS map missing selected channel");
  }
  const double qlsb=cfg.v_range_volt/std::pow(2.0,cfg.nbits)*cfg.dt_ns*1e-9/cfg.R_ohm;
  const double gate_start=-0.5*cfg.gate_ns+cfg.t_offset_ns;
  if(!positive(qlsb)||!std::isfinite(gate_start))throw std::runtime_error("TS charge/time scale overflow");
  fCfg=cfg;fCalibration=std::move(calibration);fMap=std::move(mapping);
  fNChannels=static_cast<int>(cfg.detector_ids.size());
  fNSamples=static_cast<int>(std::round(samples));fGateStart=gate_start;fQLSB=qlsb;
}
void MOLLERTriggerScintillatorDigitizer::SetRandomSeed(unsigned int seed) {
  fRequestedSeed=seed;fRand.SetSeed(seed);
  unsigned int other=seed==0?0:(seed^0x9e3779b9U);if(seed!=0&&other==0)other=1;
  fElectronicsRand.SetSeed(other);
}
void MOLLERTriggerScintillatorDigitizer::BookBranches(TTree* tree) {
  if(!tree)throw std::runtime_error("Cannot book TS branches on null tree");
  tree->Branch("evnum",&fEvnum,"evnum/L");
  tree->Branch("pe_per_mev",&fCfg.pe_per_mev,"pe_per_mev/D");
  tree->Branch("physical_scoring_validated",&physical_scoring_validated,"physical_scoring_validated/I");
  tree->Branch("response_schema_version",&response_schema_version,"response_schema_version/I");
  tree->Branch("requested_seed",&fRequestedSeed,"requested_seed/i");
  tree->Branch("electronics_enabled",&fCfg.enable_electronics,"electronics_enabled/O");
  tree->Branch("readout_all_channels",&fCfg.readout_all_channels,"readout_all_channels/O");
  tree->Branch("nconfigured_channels",&fNChannels,"nconfigured_channels/I");
  tree->Branch("gate_start_ns",&fGateStart,"gate_start_ns/D");tree->Branch("dt_ns",&fCfg.dt_ns,"dt_ns/D");
  tree->Branch("nsamples",&fNSamples,"nsamples/I");tree->Branch("nbits",&fCfg.nbits,"nbits/I");
  tree->Branch("range_v",&fCfg.v_range_volt,"range_v/D");tree->Branch("resistance_ohm",&fCfg.R_ohm,"resistance_ohm/D");
  tree->Branch("qlsb_c",&fQLSB,"qlsb_c/D");tree->Branch("scint_decay_ns",&fCfg.scint_decay_ns,"scint_decay_ns/D");
  tree->Branch("pulse_tau_ns",&fCfg.pulse_tau_ns,"pulse_tau_ns/D");tree->Branch("sigma_time_ns",&fCfg.sigma_time_ns,"sigma_time_ns/D");
  tree->Branch("transit_time_ns",&fCfg.transit_time_ns,"transit_time_ns/D");tree->Branch("hit_time_mode",&fCfg.hit_time_mode,"hit_time_mode/I");
#define TS_BRANCH(x) tree->Branch(#x,&x)
  TS_BRANCH(detid);TS_BRANCH(copyid);TS_BRANCH(time_valid);TS_BRANCH(sum_valid);TS_BRANCH(npe_poiss);TS_BRANCH(input_present);
  TS_BRANCH(edep_mev);TS_BRANCH(meanpe);TS_BRANCH(first_hit_time_ns);
  TS_BRANCH(hit_detid);TS_BRANCH(hit_copyid);TS_BRANCH(hit_pid);TS_BRANCH(hit_time_ns);TS_BRANCH(hit_edep_mev);
  TS_BRANCH(rocid);TS_BRANCH(slot);TS_BRANCH(channel);TS_BRANCH(plane);TS_BRANCH(sector);TS_BRANCH(mapping_valid);
  TS_BRANCH(pedestal_mean_adc);TS_BRANCH(pedestal_sigma_adc);TS_BRANCH(pmt_gain);TS_BRANCH(qpe_c);
  TS_BRANCH(adc_int);TS_BRANCH(adc_int_pedsub);TS_BRANCH(waveform_valid);TS_BRANCH(waveform_saturated);TS_BRANCH(waveform_underflow);
  TS_BRANCH(charge_generated_pc);TS_BRANCH(charge_in_gate_pc);TS_BRANCH(timing_model);TS_BRANCH(first_pe_time_ns);TS_BRANCH(pe_time_valid);
  if(fCfg.store_waveforms){TS_BRANCH(wf_detid);TS_BRANCH(wf_copyid);TS_BRANCH(wf_rocid);TS_BRANCH(wf_slot);TS_BRANCH(wf_channel);TS_BRANCH(wf_samp);TS_BRANCH(wf_adc);}
  if(fCfg.store_pe_times){TS_BRANCH(pe_detid);TS_BRANCH(pe_copyid);TS_BRANCH(pe_time_ns);}
#undef TS_BRANCH
}
void MOLLERTriggerScintillatorDigitizer::DigitizeEvent(const std::vector<Hit>& hits,const std::vector<Deposit>& sums) {
#define TS_CLEAR(x) x.clear()
  TS_CLEAR(detid);TS_CLEAR(copyid);TS_CLEAR(time_valid);TS_CLEAR(sum_valid);TS_CLEAR(npe_poiss);TS_CLEAR(input_present);
  TS_CLEAR(edep_mev);TS_CLEAR(meanpe);TS_CLEAR(first_hit_time_ns);
  TS_CLEAR(hit_detid);TS_CLEAR(hit_copyid);TS_CLEAR(hit_pid);TS_CLEAR(hit_time_ns);TS_CLEAR(hit_edep_mev);
  TS_CLEAR(rocid);TS_CLEAR(slot);TS_CLEAR(channel);TS_CLEAR(plane);TS_CLEAR(sector);TS_CLEAR(mapping_valid);
  TS_CLEAR(pedestal_mean_adc);TS_CLEAR(pedestal_sigma_adc);TS_CLEAR(pmt_gain);TS_CLEAR(qpe_c);
  TS_CLEAR(adc_int);TS_CLEAR(adc_int_pedsub);TS_CLEAR(waveform_valid);TS_CLEAR(waveform_saturated);TS_CLEAR(waveform_underflow);
  TS_CLEAR(charge_generated_pc);TS_CLEAR(charge_in_gate_pc);TS_CLEAR(timing_model);TS_CLEAR(first_pe_time_ns);TS_CLEAR(pe_time_valid);
  TS_CLEAR(wf_detid);TS_CLEAR(wf_copyid);TS_CLEAR(wf_rocid);TS_CLEAR(wf_slot);TS_CLEAR(wf_channel);TS_CLEAR(wf_samp);TS_CLEAR(wf_adc);
  TS_CLEAR(pe_detid);TS_CLEAR(pe_copyid);TS_CLEAR(pe_time_ns);
#undef TS_CLEAR
  using Key=std::pair<int,int>;
  struct Volume {double edep=0,time=missing;bool summed=false,present=false;std::vector<std::pair<double,double>> steps;};
  std::map<Key,Volume> volumes;
  auto selected=[&](int d){return std::find(fCfg.detector_ids.begin(),fCfg.detector_ids.end(),d)!=fCfg.detector_ids.end();};
  for(const auto& s:sums)if(selected(s.det)) {
    if(!std::isfinite(s.edep_mev)||s.edep_mev<0)throw std::runtime_error("Invalid TS sum.edep");
    auto& v=volumes[{s.det,s.copy}];if(v.summed)throw std::runtime_error("Duplicate TS sum detector/copy");
    v.edep=s.edep_mev;v.summed=true;v.present=true;
  }
  for(const auto& h:hits)if(selected(h.det)) {
    if(!std::isfinite(h.edep_mev)||h.edep_mev<0||!std::isfinite(h.time_ns))throw std::runtime_error("Invalid TS hit energy/time");
    hit_detid.push_back(h.det);hit_copyid.push_back(h.copy);hit_pid.push_back(h.pid);hit_time_ns.push_back(h.time_ns);hit_edep_mev.push_back(h.edep_mev);
    auto& v=volumes[{h.det,h.copy}];v.present=true;
    if(!std::isfinite(v.time)||h.time_ns<v.time)v.time=h.time_ns;
    if(h.edep_mev>0)v.steps.emplace_back(h.time_ns,h.edep_mev);
  }
  if(fCfg.readout_all_channels) {
    // One effective channel per detector. Do not silently merge copy-ID mismatches.
    for(int id:fCfg.detector_ids) {
      auto a=volumes.lower_bound({id,std::numeric_limits<int>::min()});
      if(a==volumes.end()||a->first.first!=id)volumes.emplace(Key{id,0},Volume{});
      else {auto b=std::next(a);if(b!=volumes.end()&&b->first.first==id)throw std::runtime_error("Full TS readout requires one copy per detector; inspect hit.id/sum.vid");}
    }
  }
  const int top=(1<<fCfg.nbits)-1;
  for(const auto& item:volumes) {
    const int did=item.first.first,cid=item.first.second;const auto& v=item.second;
    const bool known=v.summed||!v.present; // No input record is a no-signal assumption, explicitly flagged.
    const double mu=known?v.edep*fCfg.pe_per_mev:0;
    if(!std::isfinite(mu)||mu>static_cast<double>(std::numeric_limits<int>::max())/4)throw std::runtime_error("TS PE mean outside supported range");
    const int npe=known?(mu>0?static_cast<int>(fRand.Poisson(mu)):0):-1;
    detid.push_back(did);copyid.push_back(cid);sum_valid.push_back(v.summed);input_present.push_back(v.present);
    edep_mev.push_back(known?v.edep:missing);meanpe.push_back(known?mu:missing);npe_poiss.push_back(npe);
    first_hit_time_ns.push_back(v.time);time_valid.push_back(std::isfinite(v.time));
    auto ci=fCalibration.find(did);
    const Calibration c=ci==fCalibration.end()?Calibration{fCfg.pedestal_mean,fCfg.pedestal_sigma,fCfg.pmt_gain}:ci->second;
    const double qpe=electron_charge_c*c.gain;
    if(!std::isfinite(qpe)||qpe<=0)throw std::runtime_error("Invalid TS calibrated charge per PE");
    pedestal_mean_adc.push_back(c.pedestal_mean);pedestal_sigma_adc.push_back(c.pedestal_sigma);pmt_gain.push_back(c.gain);qpe_c.push_back(qpe);
    auto mi=fMap.find(did);const bool mapped=mi!=fMap.end();
    Address address=mapped?mi->second:Address{-1,-1,-1,did>=400&&did<=413?(did-400)/7:-1,did>=400&&did<=413?(did-400)%7+1:-1};
    rocid.push_back(address.roc);slot.push_back(address.slot);channel.push_back(address.channel);plane.push_back(address.plane);sector.push_back(address.sector);mapping_valid.push_back(mapped);
    charge_generated_pc.push_back(known?npe*qpe*1e12:missing);
    bool valid=fCfg.enable_electronics&&known&&(npe==0||std::isfinite(v.time));
    int model=0;
    if(npe>0)model=!std::isfinite(v.time)?3:(fCfg.hit_time_mode==1&&!v.steps.empty()?1:2);
    timing_model.push_back(model);
    double firstpe=missing,gatecharge=0;std::vector<double> samples;
    if(fCfg.enable_electronics)samples.assign(fNSamples,0);
    if(fCfg.enable_electronics&&valid&&npe>0) {
      if(npe>1000000)throw std::runtime_error("TS waveform limit exceeded: >1000000 PE in one channel");
      std::vector<double> cumulative;double total=0;
      if(model==1){for(const auto& step:v.steps){total+=step.second;cumulative.push_back(total);}if(!std::isfinite(total))throw std::runtime_error("TS timing-weight overflow");}
      for(int k=0;k<npe;++k) {
        double anchor=v.time;
        if(model==1){const double r=fElectronicsRand.Uniform(total);auto i=std::upper_bound(cumulative.begin(),cumulative.end(),r);anchor=v.steps[std::min<size_t>(i-cumulative.begin(),v.steps.size()-1)].first;}
        double tpe=anchor+fCfg.transit_time_ns;
        if(fCfg.scint_decay_ns>0)tpe+=fElectronicsRand.Exp(fCfg.scint_decay_ns);
        if(fCfg.sigma_time_ns>0)tpe+=fElectronicsRand.Gaus(0,fCfg.sigma_time_ns);
        if(!std::isfinite(tpe))throw std::runtime_error("TS PE arrival time overflow");
        if(!std::isfinite(firstpe)||tpe<firstpe)firstpe=tpe;
        if(fCfg.store_pe_times){pe_detid.push_back(did);pe_copyid.push_back(cid);pe_time_ns.push_back(tpe);}
        for(int s=0;s<fNSamples;++s) {
          const double t1=fGateStart+s*fCfg.dt_ns,t2=t1+fCfg.dt_ns;
          if(t2<=tpe)continue;
          const double a=std::max(t1,tpe);
          samples[s]+=qpe*(std::exp(-(a-tpe)/fCfg.pulse_tau_ns)-std::exp(-(t2-tpe)/fCfg.pulse_tau_ns));
        }
      }
    }
    first_pe_time_ns.push_back(firstpe);pe_time_valid.push_back(std::isfinite(firstpe));
    int raw=0,saturated=0,underflow=0;double pedsub=0;
    if(fCfg.enable_electronics)for(int s=0;s<fNSamples;++s) {
      gatecharge+=samples[s];
      double value=samples[s]/fQLSB+c.pedestal_mean;
      if(c.pedestal_sigma>0)value+=fElectronicsRand.Gaus(0,c.pedestal_sigma);
      if(!std::isfinite(value))throw std::runtime_error("TS ADC overflow");
      // Bound before integer conversion; huge but finite gains cannot overflow llround.
      int adc=value<=0?0:value>=top?top:static_cast<int>(std::llround(value));
      if(adc==top)saturated=1;
      if(adc==0)underflow=1;
      raw+=adc;pedsub+=adc-c.pedestal_mean;
      if(fCfg.store_waveforms){wf_detid.push_back(did);wf_copyid.push_back(cid);wf_rocid.push_back(address.roc);wf_slot.push_back(address.slot);wf_channel.push_back(address.channel);wf_samp.push_back(s);wf_adc.push_back(static_cast<unsigned short>(adc));}
    }
    waveform_valid.push_back(valid);waveform_saturated.push_back(saturated);waveform_underflow.push_back(underflow);
    adc_int.push_back(fCfg.enable_electronics?raw:-1);adc_int_pedsub.push_back(fCfg.enable_electronics?pedsub:missing);
    charge_in_gate_pc.push_back(valid?gatecharge*1e12:missing);
  }
}
