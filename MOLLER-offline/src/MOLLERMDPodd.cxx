#include "MOLLERMDPodd.h"
#include "THaGlobals.h"
#include "THaVarList.h"
#include <iostream>
#include "THaSlotData.h"
ClassImp(MOLLERMDRun)
ClassImp(MOLLERMDDecoder)
MOLLERMDRun::MOLLERMDRun(const char* path):THaRunBase("MOLLER MD digitised simulation"),fPath(path){
 SetDataRequired(0);SetNumber(1);SetType(1);SetDate(TDatime());
}
THaRunBase& MOLLERMDRun::operator=(const THaRunBase& rhs){
 if(this!=&rhs){const auto* src=dynamic_cast<const MOLLERMDRun*>(&rhs);
 if(!src)throw std::runtime_error("Cannot assign non-MOLLER run");
 Close();THaRunBase::operator=(rhs);fPath=src->fPath;fEntry=0;fOpened=false;}
 return *this;
}
Int_t MOLLERMDRun::Open(){try{fReader.reset(new MOLLERMDReader(fPath.c_str()));fEntry=0;fOpened=true;return READ_OK;}
 catch(const std::exception& e){std::cerr<<e.what()<<"\n";return READ_FATAL;}}
Int_t MOLLERMDRun::Close(){fReader.reset();fOpened=false;return READ_OK;}
Int_t MOLLERMDRun::ReadEvent(){
 if(!fReader)return READ_FATAL;if(fEntry>=fReader->Entries())return READ_EOF;
 try{fEvent=fReader->Read(fEntry++);return READ_OK;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return READ_FATAL;}
}
const UInt_t* MOLLERMDRun::GetEvBuffer()const{return reinterpret_cast<const UInt_t*>(&fEvent);}
MOLLERMDDecoder::MOLLERMDDecoder(){
 if(gHaVars){VarDef vars[]={
 {"md.input.nchannels","Decoded MD channel count",kInt,0,&fChannelCount,nullptr},
 {"md.input.nsamples","Decoded MD sample count",kInt,0,&fSampleCount,nullptr},
 {"md.input.adcsum","Sum of all raw MD ADC samples",kDouble,0,&fAdcSum,nullptr},
 {}};
 gHaVars->DefineVariables(vars,"","MOLLERMDDecoder");}
}
MOLLERMDDecoder::~MOLLERMDDecoder(){if(gHaVars)gHaVars->RemoveRegexp("md.input.*");}
Int_t MOLLERMDDecoder::LoadEvent(const UInt_t* ptr){
 if(!ptr)return HED_FATAL;
 buffer=ptr;
 const auto& event=*reinterpret_cast<const MOLLERMDReader::Event*>(ptr);
 event_type=1;event_num=event.evnum;event_length=0;evt_time=0;evscaler=0;
 for(auto ix:fMDSlots)crateslot[ix]->clearEvent();
 fChannelCount=event.channels.size();fSampleCount=0;fAdcSum=0;
 for(const auto& c:event.channels){
  if(!GoodCrateSlot(c.roc,c.slot)||c.channel<0||c.channel>=16)return HED_FATAL;
  // Non-const idx() creates the THaSlotData using Podd's own allocator.
  const auto ix=idx(c.roc,c.slot);
  if(fMDSlots.insert(ix).second)crateslot[ix]->define(c.roc,c.slot,16,1600,100);
  for(auto a:c.samples){
   if(crateslot[ix]->loadData("adc",c.channel,a,a)!=SD_OK)return HED_FATAL;
   ++fSampleCount;fAdcSum+=a;
  }
 }
 return HED_OK;
}

Int_t MOLLERMDRun::ReadInitInfo(Int_t){SetNumber(1);SetType(1);SetName("MOLLER_MD_mock_run");return READ_OK;}
