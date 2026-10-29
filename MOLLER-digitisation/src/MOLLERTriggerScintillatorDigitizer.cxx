#include "MOLLERTriggerScintillatorDigitizer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

void MOLLERTriggerScintillatorDigitizer::SetConfig(const Config& cfg) {
 if(cfg.detector_ids.empty())throw std::runtime_error("Trigger scintillator requires ts_det_ids in config");
 if(!std::isfinite(cfg.pe_per_mev)||cfg.pe_per_mev<=0)
  throw std::runtime_error("Trigger scintillator requires a positive ts_pe_per_mev calibration");
 std::set<int> ids;
 for(int id:cfg.detector_ids)
  if(id<=0||!ids.insert(id).second)throw std::runtime_error("Invalid/duplicate trigger scintillator detector ID");
 fCfg=cfg;
}
void MOLLERTriggerScintillatorDigitizer::BookBranches(TTree* tree) {
 if(!tree)throw std::runtime_error("Cannot book trigger scintillator branches on null tree");
 tree->Branch("evnum",&fEvnum,"evnum/L");
 tree->Branch("pe_per_mev",&fCfg.pe_per_mev,"pe_per_mev/D");
 tree->Branch("physical_scoring_validated",&physical_scoring_validated,"physical_scoring_validated/I");
 tree->Branch("detid",&detid);tree->Branch("copyid",&copyid);
 tree->Branch("edep_mev",&edep_mev);tree->Branch("meanpe",&meanpe);tree->Branch("npe_poiss",&npe_poiss);
 tree->Branch("first_hit_time_ns",&first_hit_time_ns);tree->Branch("time_valid",&time_valid);tree->Branch("sum_valid",&sum_valid);
 tree->Branch("hit_detid",&hit_detid);tree->Branch("hit_copyid",&hit_copyid);tree->Branch("hit_pid",&hit_pid);
 tree->Branch("hit_time_ns",&hit_time_ns);tree->Branch("hit_edep_mev",&hit_edep_mev);
}
void MOLLERTriggerScintillatorDigitizer::DigitizeEvent(const std::vector<Hit>& hits,const std::vector<Deposit>& sums) {
 detid.clear();copyid.clear();edep_mev.clear();meanpe.clear();npe_poiss.clear();
 first_hit_time_ns.clear();time_valid.clear();sum_valid.clear();hit_detid.clear();hit_copyid.clear();
 hit_pid.clear();hit_time_ns.clear();hit_edep_mev.clear();
 using Key=std::pair<int,int>;
 struct Volume {double edep=0;double time=std::numeric_limits<double>::quiet_NaN();bool summed=false;};
 std::map<Key,Volume> volumes;
 auto selected=[&](int d){return std::find(fCfg.detector_ids.begin(),fCfg.detector_ids.end(),d)!=fCfg.detector_ids.end();};
 for(const auto& s:sums)if(selected(s.det)){
  if(!std::isfinite(s.edep_mev)||s.edep_mev<0)throw std::runtime_error("Invalid TS sum.edep");
  auto& v=volumes[{s.det,s.copy}];
  if(v.summed)throw std::runtime_error("Duplicate TS sum for detector/copy ID");
  v.edep=s.edep_mev;v.summed=true;
 }
 for(const auto& h:hits)if(selected(h.det)){
  if(!std::isfinite(h.edep_mev)||h.edep_mev<0||!std::isfinite(h.time_ns))
   throw std::runtime_error("Invalid TS hit edep/time");
  hit_detid.push_back(h.det);hit_copyid.push_back(h.copy);hit_pid.push_back(h.pid);
  hit_time_ns.push_back(h.time_ns);hit_edep_mev.push_back(h.edep_mev);
  auto& v=volumes[{h.det,h.copy}];
  if(!std::isfinite(v.time)||h.time_ns<v.time)v.time=h.time_ns;
 }
 for(const auto& item:volumes){
  const auto& key=item.first;const auto& v=item.second;
  double mu=v.summed ? v.edep*fCfg.pe_per_mev : 0.0;
  if(!std::isfinite(mu)||mu>static_cast<double>(std::numeric_limits<int>::max())/4.)
   throw std::runtime_error("TS PE mean is outside supported range");
  detid.push_back(key.first);copyid.push_back(key.second);edep_mev.push_back(v.summed ? v.edep : std::numeric_limits<double>::quiet_NaN());
  meanpe.push_back(v.summed ? mu : std::numeric_limits<double>::quiet_NaN());npe_poiss.push_back(v.summed ? static_cast<int>(fRand.Poisson(mu)) : -1);
  first_hit_time_ns.push_back(v.time);time_valid.push_back(std::isfinite(v.time)?1:0);sum_valid.push_back(v.summed?1:0);
 }
}
