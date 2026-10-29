#ifndef MOLLER_TS_PODD_H
#define MOLLER_TS_PODD_H
#include "MOLLERTriggerScintillatorReader.h"
#include "THaRunBase.h"
#include "THaEvData.h"
#include <memory>
#include <set>
// Simulation object pointer transport follows Podd's simulation-run convention.
// Samples enter standard Podd slot data; no CODA/fADC firmware words are fabricated.
class MOLLERTriggerScintillatorRun : public THaRunBase {
public:
 MOLLERTriggerScintillatorRun(const char* path="",const char* map="");
 ~MOLLERTriggerScintillatorRun() override = default;
 THaRunBase& operator=(const THaRunBase& rhs) override;
 Int_t Open() override;
 Int_t Close() override;
 Int_t ReadEvent() override;
 const UInt_t* GetEvBuffer()const override;
protected:
 Int_t ReadDatabase() override {return READ_OK;}
 Int_t ReadInitInfo(Int_t=0) override;
private:
 std::string fPath,fMapPath;
 std::unique_ptr<MOLLERTriggerScintillatorReader> fReader; //!
 MOLLERTriggerScintillatorReader::Event fEvent; //!
 Long64_t fEntry=0; //!
 ClassDefOverride(MOLLERTriggerScintillatorRun,0)
};
class MOLLERTriggerScintillatorDecoder : public THaEvData {
public:
 MOLLERTriggerScintillatorDecoder();
 ~MOLLERTriggerScintillatorDecoder() override;
Int_t Init() override {
  return THaEvData::Init();
}
 Int_t LoadEvent(const UInt_t* ptr) override;
 const TSInput::Event* InputEvent()const{return fInput;}
private:
 std::set<UInt_t> fTSSlots; //!
 Int_t fChannelCount=0,fResponseCount=0,fSampleCount=0;
 Double_t fAdcSum=0;
 const TSInput::Event* fInput=nullptr; //!
 ClassDefOverride(MOLLERTriggerScintillatorDecoder,0)
};
#endif
