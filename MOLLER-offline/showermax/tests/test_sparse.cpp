#include "SMWaveformData.h"
#include <cassert>
#include <iostream>
template<class F> void rejected(F f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}assert(threw);}
int main(){
 SMInput::ChannelMap map;for(int i=0;i<28;++i)map[73001+i]={{2,3+i/16,i%16}};
 SMInput::Event e;SMInput::Validate(e,map);
 SMInput::Channel c;c.detid=73004;c.roc=2;c.slot=3;c.channel=3;e.channels.push_back(c);
 SMInput::Validate(e,map); // Response without waveform is permitted.
 e.channels[0].samples.assign(100,300);e.channels[0].raw_integral=30000;
 SMInput::Validate(e,map); // A complete pedestal-only waveform is also permitted.
 auto bad=e;bad.channels[0].samples.pop_back();rejected([&]{SMInput::Validate(bad,map);});
 bad=e;bad.channels[0].raw_integral++;rejected([&]{SMInput::Validate(bad,map);});
 bad=e;bad.channels.push_back(bad.channels.front());rejected([&]{SMInput::Validate(bad,map);});
 bad=e;bad.channels[0].channel=9;rejected([&]{SMInput::Validate(bad,map);});
 bad=e;bad.channels[0].samples[0]=4096;rejected([&]{SMInput::Validate(bad,map);});
 std::cout<<"PASS sparse/empty/pedestal events and corrupt-input rejection\n";
}
