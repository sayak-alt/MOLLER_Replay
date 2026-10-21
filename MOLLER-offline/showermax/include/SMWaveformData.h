#ifndef SM_WAVEFORM_DATA_H
#define SM_WAVEFORM_DATA_H
#include <vector>
#include <map>
#include <array>
#include <set>
#include <tuple>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cmath>
namespace SMInput {
using Address=std::array<int,3>;
using ChannelMap=std::map<int,Address>;
struct Channel {int detid=0,roc=0,slot=0,channel=0;int raw_integral=0;std::vector<unsigned short> samples;};
struct Event {int evnum=0;double event_rate_hz=0,rate_GHz=0;std::vector<Channel> channels;};
inline ChannelMap LoadMap(const std::string& path){
 std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open SM map: "+path);
 std::string line;std::getline(f,line);ChannelMap map;std::set<Address> addresses;
 while(std::getline(f,line)){
  if(line.empty()||line=="\r")continue;
  std::istringstream row(line);std::vector<std::string> fields;std::string value;
  while(std::getline(row,value,','))fields.push_back(value);
  if(fields.size()!=10)throw std::runtime_error("Malformed SM CSV row");
  int id=std::stoi(fields[0]);Address a{{std::stoi(fields[6]),std::stoi(fields[7]),std::stoi(fields[8])}};
  if(id<73001||id>73028||a[0]<0||a[1]<1||a[2]<0||a[2]>15||!map.emplace(id,a).second||!addresses.insert(a).second)
   throw std::runtime_error("Invalid/duplicate SM mapping");
 }
 if(map.size()!=28)throw std::runtime_error("SM map must contain 28 modules");
 return map;
}
inline void Validate(const Event& event,const ChannelMap& map,int nsamples=100){
 if(!std::isfinite(event.event_rate_hz)||!std::isfinite(event.rate_GHz))throw std::runtime_error("Nonfinite SM event rate");
 std::set<int> ids;
 for(const auto& c:event.channels){
  auto pos=map.find(c.detid);
  if(pos==map.end()||!ids.insert(c.detid).second||pos->second!=Address{{c.roc,c.slot,c.channel}})
   throw std::runtime_error("SM detector/electronics mapping mismatch");
  if(c.samples.empty()){
   if(c.raw_integral!=0)throw std::runtime_error("Nonzero SM raw integral without a waveform");
   continue; // Missing readout is not a measured pedestal waveform.
  }
  if(c.samples.size()!=static_cast<size_t>(nsamples))throw std::runtime_error("Incomplete SM waveform");
  long long sum=0;for(auto a:c.samples){if(a>4095)throw std::runtime_error("SM ADC exceeds 12 bits");sum+=a;}
  if(sum!=c.raw_integral)throw std::runtime_error("SM raw integral does not match ADC sample sum");
 }
}
}
#endif
