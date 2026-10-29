#include <TFile.h>
#include <TTree.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Structural/electronics validation only; no physical-scoring certification.
void check_ts_waveforms(const char* filename,Long64_t maxevents=100) {
  if(maxevents==0||maxevents<-1)throw std::runtime_error("Use -1 or a positive event count");
  TFile file(filename);if(file.IsZombie())throw std::runtime_error("Cannot open TS waveform file");
  TTree* t=nullptr;file.GetObject("trigscint_digi",t);if(!t)throw std::runtime_error("Missing trigscint_digi");
  bool electronics=false,full=false;int nsamples=0,nconfigured=0;double yield=0;
  std::vector<int> *ids=nullptr,*copies=nullptr,*sv=nullptr,*present=nullptr,*npe=nullptr,*valid=nullptr,*raw=nullptr;
  std::vector<double> *energy=nullptr,*mu=nullptr,*ped=nullptr,*sub=nullptr,*generated=nullptr,*gate=nullptr,*qpe=nullptr;
  std::vector<int> *wids=nullptr,*wcopy=nullptr,*samp=nullptr;
  std::vector<unsigned short>* adc=nullptr;
  auto branch=[&](const char* name,auto p){if(t->SetBranchAddress(name,p)<0)throw std::runtime_error(std::string("Missing/wrong TS branch: ")+name);};
  branch("electronics_enabled",&electronics);branch("readout_all_channels",&full);branch("nsamples",&nsamples);branch("nconfigured_channels",&nconfigured);branch("pe_per_mev",&yield);
  branch("detid",&ids);branch("copyid",&copies);branch("sum_valid",&sv);branch("input_present",&present);branch("npe_poiss",&npe);branch("waveform_valid",&valid);branch("adc_int",&raw);
  branch("edep_mev",&energy);branch("meanpe",&mu);branch("pedestal_mean_adc",&ped);branch("adc_int_pedsub",&sub);branch("charge_generated_pc",&generated);branch("charge_in_gate_pc",&gate);branch("qpe_c",&qpe);
  branch("wf_detid",&wids);branch("wf_copyid",&wcopy);branch("wf_samp",&samp);branch("wf_adc",&adc);
  const Long64_t count=maxevents<0?t->GetEntries():std::min(maxevents,t->GetEntries());
  if(count==0)throw std::runtime_error("No TS events to check");
  Long64_t channels=0,samples=0,positive=0,noinput=0,unknown=0,invalidsignal=0;
  for(Long64_t ev=0;ev<count;++ev) {
    t->GetEntry(ev);
    auto require=[&](bool ok,const char* message){if(!ok)throw std::runtime_error("TS entry "+std::to_string(ev)+": "+message);};
    require(electronics,"Electronics were not enabled");require(nsamples>0&&nsamples<=100000,"Invalid sample count");
    const size_t n=ids->size();
    require(copies->size()==n&&sv->size()==n&&present->size()==n&&npe->size()==n&&valid->size()==n&&raw->size()==n&&energy->size()==n&&mu->size()==n&&ped->size()==n&&sub->size()==n&&generated->size()==n&&gate->size()==n&&qpe->size()==n,"Unaligned channel arrays");
    if(full)require(n==static_cast<size_t>(nconfigured),"Full-readout channel count mismatch");
    require(wids->size()==wcopy->size()&&wids->size()==samp->size()&&wids->size()==adc->size(),"Unaligned waveform arrays");
    using Key=std::pair<int,int>;std::map<Key,size_t> index;
    std::vector<long long> sums(n,0);std::vector<std::vector<bool>> seen(n,std::vector<bool>(nsamples,false));
    for(size_t k=0;k<n;++k) {
      require(index.emplace(Key{(*ids)[k],(*copies)[k]},k).second,"Duplicate detector/copy");
      if((*sv)[k])require(std::isfinite((*energy)[k])&&(*energy)[k]>=0&&std::abs((*mu)[k]-yield*(*energy)[k])<1e-9*std::max(1.,std::abs((*mu)[k])),"Energy/PE mean mismatch");
      if(!(*present)[k]){++noinput;require(!(*sv)[k]&&(*npe)[k]==0&&(*mu)[k]==0,"Bad no-input pedestal row");}
      if((*present)[k]&&!(*sv)[k]){++unknown;require((*npe)[k]==-1&&std::isnan((*mu)[k])&&!(*valid)[k],"Unknown energy represented as measured zero");}
      if((*npe)[k]>0){++positive;if(!(*valid)[k])++invalidsignal;}
      if((*npe)[k]>=0)require(std::isfinite((*generated)[k])&&std::abs((*generated)[k]-(*npe)[k]*(*qpe)[k]*1e12)<1e-9*std::max(1.,(*generated)[k]),"Generated charge mismatch");
      if((*valid)[k])require(std::isfinite((*gate)[k])&&(*gate)[k]>=0&&(*gate)[k]<=(*generated)[k]+1e-8*std::max(1.,(*generated)[k]),"Gate charge exceeds generated charge");
      else require(std::isnan((*gate)[k]),"Invalid signal has a fabricated gate-charge truth");
    }
    for(size_t k=0;k<adc->size();++k) {
      auto it=index.find({(*wids)[k],(*wcopy)[k]});require(it!=index.end(),"Orphan waveform channel");
      int s=(*samp)[k];require(s>=0&&s<nsamples,"Waveform sample out of range");
      auto c=it->second;require(!seen[c][s],"Duplicate sample");seen[c][s]=true;sums[c]+=(*adc)[k];
    }
    for(size_t k=0;k<n;++k) {
      require(std::all_of(seen[k].begin(),seen[k].end(),[](bool x){return x;}),"Missing waveform samples");
      require(sums[k]==(*raw)[k],"Raw integral differs from waveform sum");
      require(std::abs((*sub)[k]-(sums[k]-nsamples*(*ped)[k]))<1e-5,"Pedestal-subtracted integral mismatch");
    }
    channels+=n;samples+=adc->size();
  }
  t->ResetBranchAddresses();
  std::cout<<"PASS TS waveform consistency: "<<count<<" events, "<<channels<<" channel-events, "<<samples<<" samples\n"
           <<"Positive-PE channels: "<<positive<<"; no-input pedestal rows: "<<noinput<<"; unknown-energy rows: "<<unknown<<"\n"
           <<"Positive-PE channels without modeled signal time: "<<invalidsignal<<" (check waveform_valid)\n"
           <<"Checks cover data consistency, not calibrated light yield or physical scoring.\n";
}
