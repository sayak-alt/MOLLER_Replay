#ifndef MOLLER_SM_PODD_H
#define MOLLER_SM_PODD_H
#include "MOLLERShowerMaxReader.h"
#include "THaRunBase.h"
#include "THaEvData.h"
#include <memory>
#include <set>
// Simulation object pointer transport follows Podd's simulation-run convention.
// Samples enter standard Podd slot data; no CODA/fADC firmware words are fabricated.
class MOLLERShowerMaxRun : public THaRunBase {
public:
 MOLLERShowerMaxRun(const char* path="",const char* map="");
 ~MOLLERShowerMaxRun() override = default;
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
 std::unique_ptr<MOLLERShowerMaxReader> fReader; //!
 MOLLERShowerMaxReader::Event fEvent; //!
 Long64_t fEntry=0; //!
 ClassDefOverride(MOLLERShowerMaxRun,0)
};
class MOLLERShowerMaxDecoder : public THaEvData {
public:
 MOLLERShowerMaxDecoder();
 ~MOLLERShowerMaxDecoder() override;
 Int_t Init() override {return HED_OK;}
 Int_t LoadEvent(const UInt_t* ptr) override;
private:
 std::set<UInt_t> fSMSlots; //!
 Int_t fChannelCount=0,fResponseCount=0,fSampleCount=0;
 Double_t fAdcSum=0,fRate=0,fRateGHz=0;
 ClassDefOverride(MOLLERShowerMaxDecoder,0)
};
#endif
