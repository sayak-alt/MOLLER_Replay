#include "../../MOLLER-offline/trigscint/include/MOLLERTriggerScintillatorReader.h"
#include <TSystem.h>
#include <algorithm>
#include <iostream>
void check_trigscint_reader(const char* input,Long64_t maxevents=100,const char* map="") {
 if(maxevents==0||maxevents<-1)throw std::runtime_error("Use -1 or a positive event count");
 std::string csv=map?map:"";
 if(csv.empty()) {
  auto p=gSystem->Getenv("MOLLER_REPLAY");if(!p)throw std::runtime_error("Set MOLLER_REPLAY or supply the map filename");
  csv=std::string(p)+"/DB_trigscint/MOLLER_trigscint_electronics_map.csv";
 }
 MOLLERTriggerScintillatorReader reader(input,csv.c_str());
 auto n=maxevents<0?reader.Entries():std::min(maxevents,reader.Entries());
 if(n==0)throw std::runtime_error("No TS events to check");
 Long64_t channels=0,samples=0;
 for(Long64_t i=0;i<n;++i){auto event=reader.Read(i);for(const auto& c:event.channels)if(!c.samples.empty()){++channels;samples+=c.samples.size();}}
 std::cout<<"PASS TS reader: "<<n<<" events, "<<channels<<" complete channel waveforms, "<<samples<<" samples\n"
 <<"No simulation PE, pedestal truth, hit times or waveform_valid flags were used.\n";
}
