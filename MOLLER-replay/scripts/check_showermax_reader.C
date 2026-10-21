#include "MOLLERShowerMaxReader.h"
#include <TSystem.h>
#include <iostream>
void check_showermax_reader(const char* input,Long64_t n=100) {
 const char* db=gSystem->Getenv("DB_DIR");
 if(!db)throw std::runtime_error("Set DB_DIR to DB_showermax");
 std::string csv=std::string(db)+"/MOLLER_showermax_electronics_map.csv";
 MOLLERShowerMaxReader reader(input,csv.c_str());
 Long64_t count=n<0?reader.Entries():std::min(n,reader.Entries()),waves=0,empty=0;
 for(Long64_t i=0;i<count;++i){auto e=reader.Read(i);int nw=0;
  for(auto& c:e.channels)if(!c.samples.empty()){++waves;++nw;}
  if(!nw)++empty;
 }
 std::cout<<"PASS: "<<count<<" events, "<<waves<<" complete waveforms, "<<empty<<" events without waveforms\n";
}
