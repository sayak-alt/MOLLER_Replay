#ifndef MOLLER_TS_READER_H
#define MOLLER_TS_READER_H
#include "TSWaveformData.h"
#include <TFile.h>
#include <TTree.h>
#include <algorithm>
#include <memory>

class MOLLERTriggerScintillatorReader {
public:
 using Event=TSInput::Event;
 MOLLERTriggerScintillatorReader(const char* path,const char* csv):fFile(TFile::Open(path,"READ")),fMap(TSInput::LoadMap(csv)) {
  if(!fFile||fFile->IsZombie())throw std::runtime_error("Cannot open TS digitised ROOT file");
  fFile->GetObject("trigscint_digi",fTree);if(!fTree)throw std::runtime_error("Missing trigscint_digi");
  Bind("evnum",&fEvnum);Bind("electronics_enabled",&fElectronics);
  Bind("nsamples",&fMetadata.nsamples);Bind("nbits",&fMetadata.bits);Bind("dt_ns",&fMetadata.dt_ns);Bind("gate_start_ns",&fMetadata.gate_start_ns);
  Bind("range_v",&fMetadata.range_v);Bind("resistance_ohm",&fMetadata.resistance_ohm);
  Bind("detid",&ids);Bind("copyid",&copies);Bind("rocid",&roc);Bind("slot",&slot);Bind("channel",&chan);Bind("mapping_valid",&mapped);Bind("adc_int",&raw);
  Bind("wf_detid",&wid);Bind("wf_copyid",&wcopy);Bind("wf_rocid",&wroc);Bind("wf_slot",&wslot);Bind("wf_channel",&wchan);Bind("wf_samp",&index);Bind("wf_adc",&adc);
 }
 ~MOLLERTriggerScintillatorReader(){if(fTree)fTree->ResetBranchAddresses();}
 MOLLERTriggerScintillatorReader(const MOLLERTriggerScintillatorReader&)=delete;
 MOLLERTriggerScintillatorReader& operator=(const MOLLERTriggerScintillatorReader&)=delete;
 Long64_t Entries()const{return fTree->GetEntries();}
 Event Read(Long64_t entry) {
  if(entry<0||entry>=Entries()||fTree->GetEntry(entry)<=0)throw std::runtime_error("Cannot read TS event");
  if(!fElectronics)throw std::runtime_error("TS replay requires electronics-enabled waveform input");
  TSInput::ValidateElectronics(fMetadata);
  const size_t n=ids->size(),nw=adc->size();
  if(n>14||copies->size()!=n||roc->size()!=n||slot->size()!=n||chan->size()!=n||mapped->size()!=n||raw->size()!=n)throw std::runtime_error("Unaligned TS response arrays");
  if(wid->size()!=nw||wcopy->size()!=nw||wroc->size()!=nw||wslot->size()!=nw||wchan->size()!=nw||index->size()!=nw)throw std::runtime_error("Unaligned TS waveform arrays");
  Event event;event.evnum=fEvnum;event.electronics=fMetadata;
  std::map<int,size_t> positions;std::vector<std::vector<bool>> seen(n,std::vector<bool>(fMetadata.nsamples,false));
  for(size_t k=0;k<n;++k) {
   if((*mapped)[k]!=1||!positions.emplace((*ids)[k],k).second)throw std::runtime_error("TS needs unique mapped detector channels");
   auto address=fMap.find((*ids)[k]);if(address==fMap.end())throw std::runtime_error("Unknown TS detector ID");
   TSInput::Channel c;c.plane=address->second.plane;c.sector=address->second.sector;c.detid=(*ids)[k];c.copyid=(*copies)[k];c.roc=(*roc)[k];c.slot=(*slot)[k];c.channel=(*chan)[k];c.raw_integral=(*raw)[k];event.channels.push_back(c);
  }
  for(size_t k=0;k<nw;++k) {
   auto i=positions.find((*wid)[k]);if(i==positions.end())throw std::runtime_error("Unknown TS waveform detector");
   const auto p=i->second;auto& c=event.channels[p];const int s=(*index)[k];
   if((*wcopy)[k]!=c.copyid||(*wroc)[k]!=c.roc||(*wslot)[k]!=c.slot||(*wchan)[k]!=c.channel||s<0||s>=fMetadata.nsamples||seen[p][s])throw std::runtime_error("TS waveform address/copy/index mismatch");
   if(c.samples.empty())c.samples.resize(fMetadata.nsamples);
   c.samples[s]=(*adc)[k];seen[p][s]=true;
  }
  for(size_t k=0;k<n;++k)if(!event.channels[k].samples.empty()&&std::find(seen[k].begin(),seen[k].end(),false)!=seen[k].end())throw std::runtime_error("Missing TS samples");
  TSInput::Validate(event,fMap);return event;
 }
private:
 template<class T>void Bind(const char* name,T p){if(!fTree->GetBranch(name)||fTree->SetBranchAddress(name,p)<0)throw std::runtime_error(std::string("Missing/wrong TS waveform branch: ")+name);}
 std::unique_ptr<TFile> fFile;TTree* fTree=nullptr;TSInput::ChannelMap fMap;TSInput::Electronics fMetadata;
 Long64_t fEvnum=0;bool fElectronics=false;
 #define TS_VEC(type,name) std::vector<type> name##_storage;std::vector<type>* name=&name##_storage
 TS_VEC(int,ids);TS_VEC(int,copies);TS_VEC(int,roc);TS_VEC(int,slot);TS_VEC(int,chan);TS_VEC(int,mapped);TS_VEC(int,raw);
 TS_VEC(int,wid);TS_VEC(int,wcopy);TS_VEC(int,wroc);TS_VEC(int,wslot);TS_VEC(int,wchan);TS_VEC(int,index);TS_VEC(unsigned short,adc);
 #undef TS_VEC
};
#endif
