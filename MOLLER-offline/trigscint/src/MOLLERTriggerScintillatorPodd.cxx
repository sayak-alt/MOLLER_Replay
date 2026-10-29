#include "MOLLERTriggerScintillatorPodd.h"
#include "THaGlobals.h"
#include "THaVarList.h"
#include "THaSlotData.h"
#include <iostream>
ClassImp(MOLLERTriggerScintillatorRun)
ClassImp(MOLLERTriggerScintillatorDecoder)
MOLLERTriggerScintillatorRun::MOLLERTriggerScintillatorRun(const char* path,const char* map)
 :THaRunBase("MOLLER TS digitised simulation"),fPath(path),fMapPath(map){SetDataRequired(0);SetNumber(1);SetType(1);SetDate(TDatime());}
THaRunBase& MOLLERTriggerScintillatorRun::operator=(const THaRunBase& rhs) {
 if(this!=&rhs){auto src=dynamic_cast<const MOLLERTriggerScintillatorRun*>(&rhs);if(!src)throw std::runtime_error("Cannot assign non-TS run");
 Close();THaRunBase::operator=(rhs);fPath=src->fPath;fMapPath=src->fMapPath;fEntry=0;fOpened=false;}
 return *this;
}
Int_t MOLLERTriggerScintillatorRun::Open(){try{fReader.reset(new MOLLERTriggerScintillatorReader(fPath.c_str(),fMapPath.c_str()));fEntry=0;fOpened=true;return READ_OK;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return READ_FATAL;}}
Int_t MOLLERTriggerScintillatorRun::Close(){fReader.reset();fOpened=false;return READ_OK;}
Int_t MOLLERTriggerScintillatorRun::ReadEvent(){
 if(!fReader)return READ_FATAL;
 if(fEntry>=fReader->Entries())return READ_EOF;
 try{fEvent=fReader->Read(fEntry++);return READ_OK;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return READ_FATAL;}
}
const UInt_t* MOLLERTriggerScintillatorRun::GetEvBuffer()const{return reinterpret_cast<const UInt_t*>(&fEvent);}
Int_t MOLLERTriggerScintillatorRun::ReadInitInfo(Int_t){SetNumber(1);SetType(1);SetName("MOLLER_TS_mock_run");return READ_OK;}
MOLLERTriggerScintillatorDecoder::MOLLERTriggerScintillatorDecoder(){
 if(gHaVars){VarDef vars[]={
 {"ts.input.nwaveform_channels","Decoded TS waveform channels",kInt,0,&fChannelCount,nullptr},
 {"ts.input.nresponse_channels","Input TS response rows",kInt,0,&fResponseCount,nullptr},
 {"ts.input.nsamples","Total decoded TS ADC samples",kInt,0,&fSampleCount,nullptr},
 {"ts.input.adcsum","Sum of raw TS ADC counts",kDouble,0,&fAdcSum,nullptr},{}};
 gHaVars->DefineVariables(vars,"","MOLLERTriggerScintillatorDecoder");}
}
MOLLERTriggerScintillatorDecoder::~MOLLERTriggerScintillatorDecoder(){if(gHaVars)gHaVars->RemoveRegexp("ts.input.*");}
Int_t MOLLERTriggerScintillatorDecoder::LoadEvent(const UInt_t* ptr) {
 fInput=nullptr;if(!ptr)return HED_FATAL;
 if(first_decode){const auto status=Init();if(status!=HED_OK)return status;}
 buffer=ptr;
 const auto& event=*reinterpret_cast<const MOLLERTriggerScintillatorReader::Event*>(ptr);
 event_type=1;event_num=event.evnum;event_length=0;evt_time=0;evscaler=0;
 for(auto i:fTSSlots)crateslot[i]->clearEvent();
 fResponseCount=event.channels.size();fChannelCount=0;fSampleCount=0;fAdcSum=0;
 for(const auto& c:event.channels) {
  if(c.samples.empty())continue;
  if(!GoodCrateSlot(c.roc,c.slot)||c.channel<0||c.channel>=16)return HED_FATAL;
  const auto i=idx(c.roc,c.slot);
  if(fTSSlots.insert(i).second)crateslot[i]->define(c.roc,c.slot,16,16*event.electronics.nsamples,event.electronics.nsamples);
  for(auto value:c.samples){if(crateslot[i]->loadData("adc",c.channel,value,value)!=SD_OK)return HED_FATAL;++fSampleCount;fAdcSum+=value;}
  ++fChannelCount;
 }
 fInput=&event;return HED_OK;
}
