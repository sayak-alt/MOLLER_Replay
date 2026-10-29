#ifndef TS_WAVEFORM_DATA_H
#define TS_WAVEFORM_DATA_H
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// ADC/electronics transport only: no PE, injected baseline, hit time or MC-validity fields.
namespace TSInput {
struct Address {int roc=0,slot=0,channel=0,plane=0,sector=0;};
using ChannelMap=std::map<int,Address>;
struct Electronics {int nsamples=100,bits=12;double dt_ns=4,gate_start_ns=-170,range_v=1,resistance_ohm=50;};
struct Channel {int detid=0,copyid=0,roc=0,slot=0,channel=0,plane=-1,sector=-1;int raw_integral=0;std::vector<unsigned short> samples;};
struct Event {std::int64_t evnum=0;Electronics electronics;std::vector<Channel> channels;};
inline std::string Trim(const std::string& s) {auto a=s.find_first_not_of(" \t\r\n");return a==std::string::npos?std::string():s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline int ParseInt(const std::string& s) {auto v=Trim(s);size_t used=0;long long x=std::stoll(v,&used);if(used!=v.size()||x<std::numeric_limits<int>::min()||x>std::numeric_limits<int>::max())throw std::runtime_error("Invalid TS map integer");return static_cast<int>(x);}
inline ChannelMap LoadMap(const std::string& path) {
 std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open TS map: "+path);
 std::string line;if(!std::getline(f,line)||Trim(line)!="detid,rocid,slot,channel,plane,sector")throw std::runtime_error("Wrong TS map CSV header");
 ChannelMap map;std::set<std::tuple<int,int,int>> used;std::set<std::pair<int,int>> modules;
 while(std::getline(f,line)) {
  line=Trim(line);if(line.empty()||line[0]=='#')continue;
  std::vector<int> v;size_t start=0;
  while(true){auto comma=line.find(',',start);v.push_back(ParseInt(line.substr(start,comma==std::string::npos?comma:comma-start)));if(comma==std::string::npos)break;start=comma+1;}
  if(v.size()!=6)throw std::runtime_error("TS map must have six columns");
  Address a{v[1],v[2],v[3],v[4],v[5]};
  if(v[0]<400||v[0]>413||a.roc<0||a.roc>=256||a.slot<1||a.slot>=32||a.channel<0||a.channel>=16||a.plane<0||a.plane>1||a.sector<1||a.sector>7||
     !map.emplace(v[0],a).second||!used.emplace(a.roc,a.slot,a.channel).second||!modules.emplace(a.plane,a.sector).second)
   throw std::runtime_error("Invalid/duplicate TS map ID, module or address");
 }
 if(map.size()!=14)throw std::runtime_error("TS replay requires a complete 14-channel map");
 return map;
}
inline void ValidateElectronics(const Electronics& e) {
 if(e.nsamples<3||e.nsamples>4096||e.bits<1||e.bits>16||!std::isfinite(e.dt_ns)||e.dt_ns<=0||!std::isfinite(e.gate_start_ns)||
    !std::isfinite(e.range_v)||e.range_v<=0||!std::isfinite(e.resistance_ohm)||e.resistance_ohm<=0)
  throw std::runtime_error("Unsupported TS waveform electronics metadata");
}
inline void Validate(const Event& event,const ChannelMap& map) {
 ValidateElectronics(event.electronics);
 if(event.evnum<0||event.evnum>std::numeric_limits<int>::max())throw std::runtime_error("TS event ID exceeds supported Podd adapter range");
 const unsigned int top=(1u<<event.electronics.bits)-1u;
 std::set<int> seen;
 for(const auto& c:event.channels) {
  auto i=map.find(c.detid);
  if(i==map.end()||!seen.insert(c.detid).second||i->second.roc!=c.roc||i->second.slot!=c.slot||i->second.channel!=c.channel||i->second.plane!=c.plane||i->second.sector!=c.sector)
   throw std::runtime_error("TS input ID/electronics mapping mismatch or duplicate detector copies");
  if(c.samples.empty()){if(c.raw_integral!=0)throw std::runtime_error("Nonzero TS raw integral with absent waveform");continue;}
  if(c.samples.size()!=static_cast<size_t>(event.electronics.nsamples))throw std::runtime_error("Incomplete TS waveform");
  long long sum=0;for(auto sample:c.samples){if(sample>top)throw std::runtime_error("TS ADC exceeds bit depth");sum+=sample;}
  if(sum!=c.raw_integral)throw std::runtime_error("TS ADC integral differs from sample sum");
 }
}
}
#endif
