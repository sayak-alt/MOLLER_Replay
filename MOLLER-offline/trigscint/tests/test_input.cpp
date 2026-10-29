#include "TSWaveformData.h"
#include <filesystem>
#include <chrono>
#include <iostream>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"Bad TS data was accepted");}
int main(int argc,char** argv) {
 require(argc==2,"Supply TS map path");auto map=TSInput::LoadMap(argv[1]);require(map.size()==14,"Map size");
 TSInput::Event e;e.evnum=7;auto a=map.at(400);
 TSInput::Channel c;c.detid=400;c.copyid=9;c.roc=a.roc;c.slot=a.slot;c.channel=a.channel;c.plane=a.plane;c.sector=a.sector;
 c.samples.assign(100,800);c.raw_integral=80000;e.channels.push_back(c);TSInput::Validate(e,map);
 e.channels.clear();TSInput::Validate(e,map); // Entirely absent readout remains distinguishable.
 c.samples.clear();c.raw_integral=0;e.channels={c};TSInput::Validate(e,map);
 c.samples.assign(100,0);e.channels={c};TSInput::Validate(e,map); // Measured zero samples are not missing.
 e.channels.push_back(c);rejects([&]{TSInput::Validate(e,map);});
 e.channels={c};e.channels[0].samples.pop_back();rejects([&]{TSInput::Validate(e,map);});
 e.channels={c};e.channels[0].raw_integral=1;rejects([&]{TSInput::Validate(e,map);});
 e.channels={c};e.channels[0].samples[0]=4096;e.channels[0].raw_integral=4096;rejects([&]{TSInput::Validate(e,map);});
 e.channels={c};e.channels[0].channel=15;rejects([&]{TSInput::Validate(e,map);});
 e.channels={c};e.evnum=-1;rejects([&]{TSInput::Validate(e,map);});
 e.evnum=0;e.electronics.dt_ns=0;rejects([&]{TSInput::Validate(e,map);});
 auto path=std::filesystem::temp_directory_path()/("moller_ts_bad_map_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".csv");
 {std::ofstream out(path);out<<"wrong_header\n";}
 rejects([&]{TSInput::LoadMap(path.string());});std::filesystem::remove(path);
 std::cout<<"PASS TS sparse/measured-zero distinction, mappings, waveforms, ADC and metadata checks\n";
}
