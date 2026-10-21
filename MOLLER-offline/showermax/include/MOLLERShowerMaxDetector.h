#ifndef MOLLER_SM_DETECTOR_H
#define MOLLER_SM_DETECTOR_H
#include "THaDetector.h"
#include "MDPulse.h"
#include <vector>
class MOLLERShowerMaxDetector:public THaDetector {
public:
 MOLLERShowerMaxDetector(const char* name="sm",THaApparatus* app=nullptr);
 ~MOLLERShowerMaxDetector() override;
 Int_t Decode(const THaEvData&) override;
 Int_t Reconstruct();
 void Clear(Option_t* opt="") override;
protected:
 Int_t DefineVariables(EMode mode=kDefine) override;
 Int_t ReadDatabase(const TDatime&) override;
 Int_t ReadRunDatabase(const TDatime&) override {return 0;}
private:
 MDPulseConfig fConfig; //!
 std::vector<int> fMap; //! detid, ROC, slot, channel per row
 std::vector<std::vector<unsigned short>> fWaveforms; //!
 Int_t fExpectedChannels=28,fExpectedSamples=100;
 Int_t fEvnum=0,fNchannels=0,fNreadout=0;
 std::vector<int> fReadout;
 Int_t fTimingReference=0,fIntegrationReference=0,fThresholdSamples=1;
 std::vector<int> fDetid,fRoc,fSlot,fChannel,fFound,fPeak,fFirst,fLast,fClipped,fSaturated;
 std::vector<double> fBaseline,fNoise,fHeight,fTime,fIntegral,fCharge;
 std::vector<double> fPeakTime,fThresholdTime,fHalfHeightTime,fAppliedThreshold;
 std::vector<int> fThresholdSample,fThresholdTimeValid,fHalfHeightTimeValid,fTimeValid,fTimeQuality;
 ClassDefOverride(MOLLERShowerMaxDetector,0)
};
#endif
