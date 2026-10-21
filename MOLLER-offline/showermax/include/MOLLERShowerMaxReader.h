#ifndef MOLLER_SHOWERMAX_READER_H
#define MOLLER_SHOWERMAX_READER_H
#include "SMWaveformData.h"
#include <TFile.h>
#include <TTree.h>
#include <memory>
#include <algorithm>
class MOLLERShowerMaxReader {
public:
 using Event=SMInput::Event;
 MOLLERShowerMaxReader(const char* path,const char* csv):fFile(TFile::Open(path,"READ")),fMap(SMInput::LoadMap(csv)){
  if(!fFile||fFile->IsZombie())throw std::runtime_error("Cannot open SM digitised ROOT file");
  fFile->GetObject("showermax_digi",fTree);if(!fTree)throw std::runtime_error("Missing showermax_digi tree");
  Bind("evnum",&fEvnum);Bind("event_rate_hz",&fRate);Bind("rate_GHz",&fRateGHz);
  Bind("detid",&ids);Bind("rocid",&roc);Bind("slot",&slot);Bind("channel",&chan);Bind("mapping_valid",&valid);Bind("adc_int",&raw);
  Bind("wf_detid",&wid);Bind("wf_rocid",&wroc);Bind("wf_slot",&wslot);Bind("wf_channel",&wchan);
  Bind("wf_mapping_valid",&wvalid);Bind("wf_samp",&index);Bind("wf_adc",&adc);
 }
 ~MOLLERShowerMaxReader(){if(fTree)fTree->ResetBranchAddresses();}
 MOLLERShowerMaxReader(const MOLLERShowerMaxReader&)=delete;
 MOLLERShowerMaxReader& operator=(const MOLLERShowerMaxReader&)=delete;
 Long64_t Entries()const{return fTree->GetEntries();}
 Event Read(Long64_t entry){
  if(entry<0||entry>=Entries()||fTree->GetEntry(entry)<=0)throw std::runtime_error("Cannot read SM event");
  const size_t n=ids->size(),nw=adc->size();
  if(n>28||roc->size()!=n||slot->size()!=n||chan->size()!=n||valid->size()!=n||raw->size()!=n)
   throw std::runtime_error("Unaligned SM channel vectors");
  if(wid->size()!=nw||wroc->size()!=nw||wslot->size()!=nw||wchan->size()!=nw||wvalid->size()!=nw||index->size()!=nw)
   throw std::runtime_error("Unaligned SM waveform vectors");
  Event ev;ev.evnum=fEvnum;ev.event_rate_hz=fRate;ev.rate_GHz=fRateGHz;
  std::map<int,size_t> positions;std::vector<std::vector<bool>> seen(n,std::vector<bool>(100,false));
  for(size_t j=0;j<n;++j){
   if(valid->at(j)!=1||!positions.emplace(ids->at(j),j).second)throw std::runtime_error("Invalid/duplicate SM detector mapping");
   SMInput::Channel c;c.detid=ids->at(j);c.roc=roc->at(j);c.slot=slot->at(j);c.channel=chan->at(j);c.raw_integral=raw->at(j);ev.channels.push_back(c);
  }
  for(size_t k=0;k<nw;++k){
   auto pos=positions.find(wid->at(k));if(pos==positions.end())throw std::runtime_error("Unknown SM waveform detector");
   size_t j=pos->second;auto& c=ev.channels[j];int i=index->at(k);
   if(wvalid->at(k)!=1||wroc->at(k)!=c.roc||wslot->at(k)!=c.slot||wchan->at(k)!=c.channel||i<0||i>=100||seen[j][i])
    throw std::runtime_error("Bad SM waveform address/sample index");
   if(c.samples.empty())c.samples.resize(100);
   c.samples[i]=adc->at(k);seen[j][i]=true;
  }
  for(size_t j=0;j<n;++j)if(!ev.channels[j].samples.empty()&&std::find(seen[j].begin(),seen[j].end(),false)!=seen[j].end())
   throw std::runtime_error("Missing SM ADC samples");
  SMInput::Validate(ev,fMap);return ev;
 }
private:
 template<class T>void Bind(const char* name,T ptr){
  if(!fTree->GetBranch(name)||fTree->SetBranchAddress(name,ptr)<0)throw std::runtime_error(std::string("Missing/wrong SM branch: ")+name);
 }
 std::unique_ptr<TFile> fFile;TTree* fTree=nullptr;SMInput::ChannelMap fMap;
 int fEvnum=0;double fRate=0,fRateGHz=0;
 // Owned storage avoids ROOT-allocated vector ownership ambiguity.
 #define SM_VEC(type,name) std::vector<type> name##_storage;std::vector<type>* name=&name##_storage
 SM_VEC(int,ids);SM_VEC(int,roc);SM_VEC(int,slot);SM_VEC(int,chan);SM_VEC(int,valid);SM_VEC(int,raw);
 SM_VEC(int,wid);SM_VEC(int,wroc);SM_VEC(int,wslot);SM_VEC(int,wchan);SM_VEC(int,wvalid);SM_VEC(int,index);
 SM_VEC(unsigned short,adc);
 #undef SM_VEC
};
#endif
