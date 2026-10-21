#include "MOLLERShowerMaxPodd.h"
#include "THaGlobals.h"
#include "THaVarList.h"
#include <iostream>
#include "THaSlotData.h"
ClassImp(MOLLERShowerMaxRun)
ClassImp(MOLLERShowerMaxDecoder)
MOLLERShowerMaxRun::MOLLERShowerMaxRun(const char* path,const char* map):THaRunBase("MOLLER Shower-Max digitised simulation"),fPath(path),fMapPath(map){
 SetDataRequired(0);SetNumber(1);SetType(1);SetDate(TDatime());
}
THaRunBase& MOLLERShowerMaxRun::operator=(const THaRunBase& rhs){
 if(this!=&rhs){const auto* src=dynamic_cast<const MOLLERShowerMaxRun*>(&rhs);
 if(!src)throw std::runtime_error("Cannot assign non-MOLLER run");
 Close();THaRunBase::operator=(rhs);fPath=src->fPath;fMapPath=src->fMapPath;fEntry=0;fOpened=false;}
 return *this;
}
Int_t MOLLERShowerMaxRun::Open(){try{fReader.reset(new MOLLERShowerMaxReader(fPath.c_str(),fMapPath.c_str()));fEntry=0;fOpened=true;return READ_OK;}
 catch(const std::exception& e){std::cerr<<e.what()<<"\n";return READ_FATAL;}}
Int_t MOLLERShowerMaxRun::Close(){fReader.reset();fOpened=false;return READ_OK;}
Int_t MOLLERShowerMaxRun::ReadEvent(){
 if(!fReader)return READ_FATAL;if(fEntry>=fReader->Entries())return READ_EOF;
 try{fEvent=fReader->Read(fEntry++);return READ_OK;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return READ_FATAL;}
}
const UInt_t* MOLLERShowerMaxRun::GetEvBuffer()const{return reinterpret_cast<const UInt_t*>(&fEvent);}
MOLLERShowerMaxDecoder::MOLLERShowerMaxDecoder(){
 if(gHaVars){VarDef vars[]={
 {"sm.input.nwaveform_channels","Decoded SM channel count",kInt,0,&fChannelCount,nullptr},
 {"sm.input.nresponse_channels","Input response channel count",kInt,0,&fResponseCount,nullptr},
 {"sm.input.event_rate_hz","Input event rate [Hz]",kDouble,0,&fRate,nullptr},
 {"sm.input.rate_GHz","Input normalized rate [GHz]",kDouble,0,&fRateGHz,nullptr},
 {"sm.input.nsamples","Decoded SM sample count",kInt,0,&fSampleCount,nullptr},
 {"sm.input.adcsum","Sum of all raw SM ADC samples",kDouble,0,&fAdcSum,nullptr},
 {}};
 gHaVars->DefineVariables(vars,"","MOLLERShowerMaxDecoder");}
}
MOLLERShowerMaxDecoder::~MOLLERShowerMaxDecoder(){if(gHaVars)gHaVars->RemoveRegexp("sm.input.*");}
Int_t MOLLERShowerMaxDecoder::LoadEvent(const UInt_t* ptr){
 if(!ptr)return HED_FATAL;
 buffer=ptr;
 const auto& event=*reinterpret_cast<const MOLLERShowerMaxReader::Event*>(ptr);
 event_type=1;event_num=event.evnum;event_length=0;evt_time=0;evscaler=0;
 for(auto ix:fSMSlots)crateslot[ix]->clearEvent();
 fResponseCount=event.channels.size();fChannelCount=0;fSampleCount=0;fAdcSum=0;
 fRate=event.event_rate_hz;fRateGHz=event.rate_GHz;
 for(const auto& c:event.channels){
  if(c.samples.empty())continue;
  ++fChannelCount;
  if(!GoodCrateSlot(c.roc,c.slot)||c.channel<0||c.channel>=16)return HED_FATAL;
  // Non-const idx() creates the THaSlotData using Podd's own allocator.
  const auto ix=idx(c.roc,c.slot);
  if(fSMSlots.insert(ix).second)crateslot[ix]->define(c.roc,c.slot,16,1600,100);
  for(auto a:c.samples){
   if(crateslot[ix]->loadData("adc",c.channel,a,a)!=SD_OK)return HED_FATAL;
   ++fSampleCount;fAdcSum+=a;
  }
 }
 return HED_OK;
}

Int_t MOLLERShowerMaxRun::ReadInitInfo(Int_t){SetNumber(1);SetType(1);SetName("MOLLER_SM_mock_run");return READ_OK;}
