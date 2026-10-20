#include <TFile.h>
#include <TTree.h>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <iostream>
#include <stdexcept>
void check_sm_full_readout(const char* input,Long64_t nevents=100,int nsamples=100){
 TFile f(input);if(f.IsZombie())throw std::runtime_error("Cannot open input");
 TTree* t=nullptr;f.GetObject("showermax_digi",t);if(!t)throw std::runtime_error("Missing showermax_digi");
 std::vector<int> *id=nullptr,*npe=nullptr,*raw=nullptr,*wid=nullptr,*index=nullptr;
 std::vector<double> *ped=nullptr,*sigma=nullptr,*sub=nullptr;
 std::vector<unsigned short>* adc=nullptr;
 auto bind=[&](const char* n,auto a){if(t->SetBranchAddress(n,a)<0)throw std::runtime_error(std::string("Wrong/missing branch: ")+n);};
 bind("detid",&id);bind("npe_poiss",&npe);bind("adc_int",&raw);bind("pedestal_mean_adc",&ped);
 bind("pedestal_sigma_adc",&sigma);bind("adc_int_pedsub",&sub);bind("wf_detid",&wid);bind("wf_samp",&index);bind("wf_adc",&adc);
 Long64_t n=nevents<0?t->GetEntries():std::min(nevents,t->GetEntries()),zeros=0;
 for(Long64_t i=0;i<n;++i){if(t->GetEntry(i)<=0)throw std::runtime_error("Read error");
  if(id->size()!=28||npe->size()!=28||raw->size()!=28||ped->size()!=28||sigma->size()!=28||sub->size()!=28||
     adc->size()!=size_t(28*nsamples)||wid->size()!=adc->size()||index->size()!=adc->size())
   throw std::runtime_error("Wrong channel/sample vector size");
  std::map<int,long long> sum;std::map<int,std::set<int>> seen;
  for(size_t k=0;k<adc->size();++k){int did=wid->at(k),j=index->at(k);
   if(did<73001||did>73028||j<0||j>=nsamples||!seen[did].insert(j).second)throw std::runtime_error("Bad waveform ID/index");
   sum[did]+=adc->at(k);}
  for(int j=0;j<28;++j){int did=id->at(j);
   if(did!=73001+j||seen[did].size()!=size_t(nsamples)||sum[did]!=raw->at(j)||
      std::abs(sub->at(j)-(sum[did]-nsamples*ped->at(j)))>1e-6)
    throw std::runtime_error("Waveform/integral consistency failed");
   if(npe->at(j)==0)++zeros;
  }
 }
 t->ResetBranchAddresses();
 std::cout<<"PASS: "<<n<<" events; 28 channels/event; "<<28*nsamples<<" samples/event; "<<zeros<<" zero-PE channel waveforms checked\n";
}
