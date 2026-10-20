#ifndef MOLLER_MD_READER_H
#define MOLLER_MD_READER_H
#include <TFile.h>
#include <TTree.h>
#include <vector>
#include <map>
#include <tuple>
#include <memory>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>

// ROOT-only input component for later MOLLER/Podd simulation decoding.
// No peak finding, calibration of charge, or simulated event generation.
class MOLLERMDReader {
public:
 struct Channel {
  int detid,roc,slot,channel;
  double pedestal_mean,pedestal_sigma;
  std::vector<unsigned short> samples;
 };
 struct Event {int evnum;std::vector<Channel> channels;};
 explicit MOLLERMDReader(const char* path,int expected_channels=224,int expected_samples=100)
 :file(TFile::Open(path,"READ")),nchannels(expected_channels),nsamples(expected_samples) {
  if(!file||file->IsZombie())throw std::runtime_error("Cannot open MD file");
  file->GetObject("pmt_digi",tree);if(!tree)throw std::runtime_error("Missing pmt_digi");
  bind("evnum",&evnum);bind("detid",&ids);bind("rocid",&roc);bind("slot",&slot);bind("channel",&chan);
  bind("mapping_valid",&valid);bind("pedestal_mean_adc",&ped);bind("pedestal_sigma_adc",&sigma);
  bind("adc_int",&raw);bind("adc_int_pedsub",&sub);
  bind("wf_detid",&wid);bind("wf_rocid",&wroc);bind("wf_slot",&wslot);bind("wf_channel",&wchan);
  bind("wf_mapping_valid",&wvalid);bind("wf_samp",&index);bind("wf_adc",&adc);
 }
 ~MOLLERMDReader(){if(tree)tree->ResetBranchAddresses();}
 MOLLERMDReader(const MOLLERMDReader&)=delete;
 MOLLERMDReader& operator=(const MOLLERMDReader&)=delete;
 Long64_t Entries()const{return tree->GetEntries();}
 Event Read(Long64_t entry){
  if(entry<0||entry>=Entries())throw std::runtime_error("Entry out of range");
  if(tree->GetEntry(entry)<=0)throw std::runtime_error("Cannot read entry");
  auto fail=[&](const std::string& msg){throw std::runtime_error("MD entry "+std::to_string(entry)+": "+msg);};
  size_t n=ids->size();
  if(n!=static_cast<size_t>(nchannels))fail("unexpected detector count");
  if(roc->size()!=n||slot->size()!=n||chan->size()!=n||valid->size()!=n||ped->size()!=n||sigma->size()!=n||raw->size()!=n||sub->size()!=n)fail("unaligned channel vectors");
  size_t nw=adc->size();
  if(wid->size()!=nw||wroc->size()!=nw||wslot->size()!=nw||wchan->size()!=nw||wvalid->size()!=nw||index->size()!=nw)fail("unaligned waveform vectors");
  Event out{evnum,{}};out.channels.reserve(n);
  std::map<int,size_t> positions;
  std::map<std::tuple<int,int,int>,int> addresses;
  std::vector<std::vector<bool>> seen(n,std::vector<bool>(nsamples,false));
  for(size_t j=0;j<n;++j){
   if(!positions.emplace(ids->at(j),j).second)fail("duplicate detector ID");
   if(valid->at(j)!=1||roc->at(j)<0||slot->at(j)<1||chan->at(j)<0)fail("invalid channel mapping");
   if(!addresses.emplace(std::make_tuple(roc->at(j),slot->at(j),chan->at(j)),ids->at(j)).second)fail("duplicate electronics address");
   if(!std::isfinite(ped->at(j))||!std::isfinite(sigma->at(j))||sigma->at(j)<0)fail("invalid pedestal constants");
   out.channels.push_back({ids->at(j),roc->at(j),slot->at(j),chan->at(j),ped->at(j),sigma->at(j),std::vector<unsigned short>(nsamples)});
  }
  for(size_t k=0;k<nw;++k){
   auto it=positions.find(wid->at(k));if(it==positions.end())fail("unknown waveform detector");
   size_t j=it->second;int i=index->at(k);const auto& c=out.channels[j];
   if(wvalid->at(k)!=1||wroc->at(k)!=c.roc||wslot->at(k)!=c.slot||wchan->at(k)!=c.channel)fail("waveform address mismatch");
   if(i<0||i>=nsamples||seen[j][i])fail("bad or duplicate sample index");
   if(adc->at(k)>4095)fail("ADC above configured 12-bit range");
   seen[j][i]=true;out.channels[j].samples[i]=adc->at(k);
  }
  for(size_t j=0;j<n;++j){
   if(std::find(seen[j].begin(),seen[j].end(),false)!=seen[j].end())fail("missing samples");
   long long sum=0;for(auto a:out.channels[j].samples)sum+=a;
   if(sum!=raw->at(j))fail("raw integral mismatch");
   if(std::abs(sum-nsamples*ped->at(j)-sub->at(j))>1e-6)fail("pedestal-subtracted integral mismatch");
  }
  return out;
 }
private:
 template<class T>void bind(const char* name,T address){
  if(!tree->GetBranch(name))throw std::runtime_error(std::string("Missing branch: ")+name);
  if(tree->SetBranchAddress(name,address)<0)throw std::runtime_error(std::string("Branch type mismatch: ")+name);
 }
 std::unique_ptr<TFile> file;TTree* tree=nullptr;int nchannels,nsamples,evnum=0;
 std::vector<int>* ids=nullptr,*roc=nullptr,*slot=nullptr,*chan=nullptr,*valid=nullptr,*raw=nullptr;
 std::vector<double>* ped=nullptr,*sigma=nullptr,*sub=nullptr;
 std::vector<int>* wid=nullptr,*wroc=nullptr,*wslot=nullptr,*wchan=nullptr,*wvalid=nullptr,*index=nullptr;
 std::vector<unsigned short>* adc=nullptr;
};
#endif
