#ifndef MOLLER_TS_DETECTOR_H
#define MOLLER_TS_DETECTOR_H
#include "THaDetector.h"
#include "MDPulse.h"
#include <vector>
class MOLLERTriggerScintillatorDetector:public THaDetector {
public:
 MOLLERTriggerScintillatorDetector(const char* name="ts",THaApparatus* app=nullptr);
 ~MOLLERTriggerScintillatorDetector() override;
 Int_t Decode(const THaEvData&) override;
 Int_t Reconstruct();
 void Clear(Option_t* opt="") override;
protected:
 Int_t DefineVariables(EMode mode=kDefine) override;
 Int_t ReadDatabase(const TDatime&) override;
 Int_t ReadRunDatabase(const TDatime&) override {return 0;}
private:
 MDPulseConfig fConfig; //!
 std::vector<int> fMap; //! detid, ROC, slot, channel, plane, sector per row
 std::vector<std::vector<unsigned short>> fWaveforms; //!
 Int_t fExpectedChannels=14,fExpectedSamples=100;
 Int_t fEvnum=0,fNchannels=0,fNreadout=0;
 std::vector<int> fReadout;
 Int_t fTimingReference=0,fIntegrationReference=0,fThresholdSamples=1;
 std::vector<int> fDetid,fCopyid,fRoc,fSlot,fChannel,fPlane,fSector,fFound,fPeak,fFirst,fLast,fClipped,fSaturated;
 std::vector<double> fRawIntegral,fBaseline,fNoise,fHeight,fTime,fIntegral,fCharge;
 std::vector<double> fPeakTime,fThresholdTime,fHalfHeightTime,fAppliedThreshold;
 std::vector<int> fThresholdSample,fThresholdTimeValid,fHalfHeightTimeValid,fTimeValid,fTimeQuality;
 ClassDefOverride(MOLLERTriggerScintillatorDetector,0)
};
#endif
