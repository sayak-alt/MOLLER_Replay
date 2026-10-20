#include "MOLLERMDDetector.h"
#include "THaEvData.h"
#include <set>
#include <tuple>
#include <utility>
#include "VarDef.h"
#include <fstream>
#include <sstream>
#include <iostream>
ClassImp(MOLLERMDDetector)
MOLLERMDDetector::MOLLERMDDetector(const char* name,THaApparatus* app)
 :THaDetector(name,"MOLLER MD waveform detector",app){}
Int_t MOLLERMDDetector::ReadDatabase(const TDatime& date){
 FILE* file=OpenFile(date);if(!file)return kFileError;
 const DBRequest req[]={
 {"channel_map",&fMap,kIntV},
 {"nchannels",&fExpectedChannels,kInt},{"nsamples",&fExpectedSamples,kInt},
 {"baseline_samples",&fConfig.baseline_samples,kInt},
 {"NSB",&fConfig.nsb,kInt},{"NSA",&fConfig.nsa,kInt},
 {"dt_ns",&fConfig.dt_ns,kDouble},{"gate_start_ns",&fConfig.gate_start_ns,kDouble},
 {"range_v",&fConfig.range_v,kDouble},{"resistance_ohm",&fConfig.resistance_ohm,kDouble},
 {"bits",&fConfig.bits,kInt},{"threshold_adc",&fConfig.threshold_adc,kDouble},
 {"threshold_sigma",&fConfig.threshold_sigma,kDouble},
 {"integration_reference",&fConfig.integration_reference,kInt,0,true},
 {"timing_reference",&fConfig.timing_reference,kInt,0,true},
 {"threshold_samples",&fConfig.threshold_samples,kInt,0,true},{}};
 int status=LoadDB(file,date,req,fPrefix);fclose(file);if(status)return status;
 try{
  fConfig.Validate();
  fTimingReference=fConfig.timing_reference;
  fIntegrationReference=fConfig.integration_reference;
  fThresholdSamples=fConfig.threshold_samples;
  if(fExpectedChannels!=224||fExpectedSamples!=100||fConfig.bits!=12||
     fConfig.baseline_samples>=fExpectedSamples||fMap.size()!=4u*fExpectedChannels)
   throw std::runtime_error("This input reader requires 224 channels, 100 samples and 12 bits");
  std::set<int> ids;std::set<std::tuple<int,int,int>> addresses;
  for(size_t i=0;i<fMap.size();i+=4){
   if(fMap[i]<=0||fMap[i+1]<0||fMap[i+1]>=Decoder::MAXROC||fMap[i+2]<1||fMap[i+2]>=Decoder::MAXSLOT||fMap[i+3]<0||fMap[i+3]>=16||
      !ids.insert(fMap[i]).second||
      !addresses.emplace(fMap[i+1],fMap[i+2],fMap[i+3]).second)
    throw std::runtime_error("Invalid/duplicate MD database map");
  }
 }catch(const std::exception& e){Error("ReadDatabase","%s",e.what());return kInitError;}
 return kOK;
}
MOLLERMDDetector::~MOLLERMDDetector(){RemoveVariables();}
void MOLLERMDDetector::Clear(Option_t* opt){THaDetector::Clear(opt);fWaveforms.clear();fNchannels=0;
 fDetid.clear();fRoc.clear();fSlot.clear();fChannel.clear();fFound.clear();fPeak.clear();fFirst.clear();fLast.clear();fClipped.clear();fSaturated.clear();
 fBaseline.clear();fNoise.clear();fHeight.clear();fTime.clear();fIntegral.clear();fCharge.clear();
 fPeakTime.clear();fThresholdTime.clear();fHalfHeightTime.clear();fAppliedThreshold.clear();
 fThresholdSample.clear();fThresholdTimeValid.clear();fHalfHeightTimeValid.clear();fTimeValid.clear();fTimeQuality.clear();}
Int_t MOLLERMDDetector::DefineVariables(EMode mode){
 const RVarDef vars[]={
 {"evnum","Original digitised event ID","fEvnum"},
 {"nchannels","Readout channels","fNchannels"},
 {"detid","Detector ID","fDetid"},{"roc","ROC","fRoc"},{"slot","Slot","fSlot"},{"channel","Channel","fChannel"},
 {"found","Pulse passed threshold","fFound"},{"peak_sample","Peak sample","fPeak"},
 {"window_start","First integration sample","fFirst"},{"window_end","Last integration sample","fLast"},
 {"clipped","Integration truncated by gate","fClipped"},{"saturated","Any ADC sample saturated","fSaturated"},
 {"baseline","Estimated baseline [ADC counts]","fBaseline"},{"noise","Baseline sample RMS [ADC counts]","fNoise"},
 {"height","Peak above baseline [ADC counts]","fHeight"},{"time_ns","Selected time reference [ns]; check time_valid","fTime"},
 {"integral","Window sum above estimated baseline [ADC-sum counts]","fIntegral"},{"charge_pc","Window charge [pC]","fCharge"},
 {"peak_time_ns","Peak sample centre [ns]","fPeakTime"},
 {"threshold_time_ns","Interpolated detection-threshold crossing [ns]","fThresholdTime"},
 {"half_height_time_ns","Interpolated rising half-height crossing [ns]","fHalfHeightTime"},
 {"applied_threshold_adc","Applied threshold above baseline [ADC counts]","fAppliedThreshold"},
 {"threshold_sample","First sample above threshold in selected pulse (zero based)","fThresholdSample"},
 {"threshold_time_valid","Threshold interpolation available","fThresholdTimeValid"},
 {"half_height_time_valid","Half-height interpolation available","fHalfHeightTimeValid"},
 {"time_valid","Selected time_ns is valid","fTimeValid"},
 {"time_quality","Software timing flags (not fADC hardware bits)","fTimeQuality"},
 {"timing_reference","time_ns selector: 0 peak,1 threshold,2 half height","fTimingReference"},
 {"integration_reference","Window selector: 0 peak,1 threshold sample","fIntegrationReference"},
 {"threshold_samples","Required consecutive samples (actual count)","fThresholdSamples"},{}};
 return DefineVarsFromList(vars,mode);
}
Int_t MOLLERMDDetector::Decode(const THaEvData& ev){
 Clear();fEvnum=ev.GetEvNum();
 for(size_t i=0;i<fMap.size();i+=4){
  int roc=fMap[i+1],slot=fMap[i+2],channel=fMap[i+3];
  if(ev.GetNumHits(roc,slot,channel)!=static_cast<UInt_t>(fExpectedSamples)){
   throw std::runtime_error("Missing/incomplete waveform for detid "+std::to_string(fMap[i]));
  }
  std::vector<unsigned short> waveform;waveform.reserve(fExpectedSamples);
  for(int k=0;k<fExpectedSamples;++k){
   const auto sample=ev.GetData(roc,slot,channel,k);
   if(sample>4095)throw std::runtime_error("Invalid 12-bit ADC sample");
   waveform.push_back(static_cast<unsigned short>(sample));
  }
  fDetid.push_back(fMap[i]);fRoc.push_back(roc);fSlot.push_back(slot);fChannel.push_back(channel);
  fWaveforms.push_back(std::move(waveform));
 }
 fNchannels=fWaveforms.size();return fNchannels;
}
Int_t MOLLERMDDetector::Reconstruct(){
 try{for(const auto& waveform:fWaveforms){auto r=ReconstructMDPulse(waveform,fConfig);
 fFound.push_back(r.found);fPeak.push_back(r.peak);fFirst.push_back(r.first);fLast.push_back(r.last);fClipped.push_back(r.clipped);fSaturated.push_back(r.saturated);
 fBaseline.push_back(r.baseline);fNoise.push_back(r.noise);fHeight.push_back(r.height);fTime.push_back(r.time_ns);fIntegral.push_back(r.integral);fCharge.push_back(r.charge_pc);
 fPeakTime.push_back(r.peak_time_ns);fThresholdTime.push_back(r.threshold_time_ns);
 fHalfHeightTime.push_back(r.half_height_time_ns);fAppliedThreshold.push_back(r.applied_threshold_adc);
 fThresholdSample.push_back(r.threshold_sample);fThresholdTimeValid.push_back(r.threshold_time_valid);
 fHalfHeightTimeValid.push_back(r.half_height_time_valid);fTimeValid.push_back(r.time_valid);fTimeQuality.push_back(r.time_quality);
 }}catch(const std::exception& e){throw std::runtime_error(std::string("MD reconstruction: ")+e.what());}
 return 0;
}
