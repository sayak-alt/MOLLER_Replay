#ifndef MOLLER_MD_PODD_H
#define MOLLER_MD_PODD_H
#include "MOLLERMDReader.h"
#include "THaRunBase.h"
#include "THaEvData.h"
#include <memory>
#include <set>
// Simulation object pointer transport follows Podd's simulation-run convention.
// Samples enter standard Podd slot data; no CODA/fADC firmware words are fabricated.
class MOLLERMDRun : public THaRunBase {
public:
 MOLLERMDRun(const char* path="");
 ~MOLLERMDRun() override = default;
 THaRunBase& operator=(const THaRunBase& rhs) override;
 Int_t Open() override;
 Int_t Close() override;
 Int_t ReadEvent() override;
 const UInt_t* GetEvBuffer()const override;
protected:
 Int_t ReadDatabase() override {return READ_OK;}
 Int_t ReadInitInfo(Int_t=0) override;
private:
 std::string fPath;
 std::unique_ptr<MOLLERMDReader> fReader; //!
 MOLLERMDReader::Event fEvent; //!
 Long64_t fEntry=0; //!
 ClassDefOverride(MOLLERMDRun,0)
};
class MOLLERMDDecoder : public THaEvData {
public:
 MOLLERMDDecoder();
 ~MOLLERMDDecoder() override;
 Int_t Init() override {return HED_OK;}
 Int_t LoadEvent(const UInt_t* ptr) override;
private:
 std::set<UInt_t> fMDSlots; //!
 Int_t fChannelCount=0,fSampleCount=0;
 Double_t fAdcSum=0;
 ClassDefOverride(MOLLERMDDecoder,0)
};
#endif
