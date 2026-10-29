#include "MOLLERTriggerScintillatorDetector.h"
#include "THaEvData.h"
#include "MOLLERTriggerScintillatorPodd.h"
#include <set>
#include <tuple>
#include <utility>
#include "VarDef.h"
#include <fstream>
#include <sstream>
#include <iostream>
ClassImp(MOLLERTriggerScintillatorDetector)
MOLLERTriggerScintillatorDetector::MOLLERTriggerScintillatorDetector(const char* name,THaApparatus* app)
 :THaDetector(name,"MOLLER Trigger Scintillator waveform detector",app){}
Int_t MOLLERTriggerScintillatorDetector::ReadDatabase(const TDatime& date){
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
  if(fExpectedChannels!=14||fExpectedSamples<3||fExpectedSamples>4096||
     fConfig.baseline_samples>=fExpectedSamples||fMap.size()!=6u*fExpectedChannels)
   throw std::runtime_error("TS replay requires 14 mapped channels and 3..4096 samples");
  std::set<int> ids;std::set<std::tuple<int,int,int>> addresses;std::set<std::pair<int,int>> modules;
  for(size_t i=0;i<fMap.size();i+=6){
   if(fMap[i]<400||fMap[i]>413||fMap[i+4]<0||fMap[i+4]>1||fMap[i+5]<1||fMap[i+5]>7||fMap[i+1]<0||fMap[i+1]>=Decoder::MAXROC||fMap[i+2]<1||fMap[i+2]>=Decoder::MAXSLOT||fMap[i+3]<0||fMap[i+3]>=16||
      !ids.insert(fMap[i]).second||
      !addresses.emplace(fMap[i+1],fMap[i+2],fMap[i+3]).second||!modules.emplace(fMap[i+4],fMap[i+5]).second)
    throw std::runtime_error("Invalid/duplicate TS database map");
  }
 }catch(const std::exception& e){Error("ReadDatabase","%s",e.what());return kInitError;}
 return kOK;
}
MOLLERTriggerScintillatorDetector::~MOLLERTriggerScintillatorDetector(){RemoveVariables();}
void MOLLERTriggerScintillatorDetector::Clear(Option_t* opt){THaDetector::Clear(opt);fWaveforms.clear();fNchannels=0;fNreadout=0;fReadout.clear();
 fDetid.clear();fCopyid.clear();fPlane.clear();fSector.clear();fRoc.clear();fSlot.clear();fChannel.clear();fFound.clear();fPeak.clear();fFirst.clear();fLast.clear();fClipped.clear();fSaturated.clear();
 fRawIntegral.clear();fBaseline.clear();fNoise.clear();fHeight.clear();fTime.clear();fIntegral.clear();fCharge.clear();
 fPeakTime.clear();fThresholdTime.clear();fHalfHeightTime.clear();fAppliedThreshold.clear();
 fThresholdSample.clear();fThresholdTimeValid.clear();fHalfHeightTimeValid.clear();fTimeValid.clear();fTimeQuality.clear();}
Int_t MOLLERTriggerScintillatorDetector::DefineVariables(EMode mode){
 const RVarDef vars[]={
 {"evnum","Original digitised event ID","fEvnum"},
 {"nchannels","Configured TS module count","fNchannels"},
 {"nreadout","Modules with recorded waveforms","fNreadout"},
 {"readout","Recorded waveform available (1/0)","fReadout"},
 {"detid","Detector ID","fDetid"},{"copyid","Input copy ID; -1 if channel absent","fCopyid"},{"plane","Map plane: 0 upstream,1 downstream","fPlane"},{"sector","Map sector 1..7","fSector"},{"roc","ROC","fRoc"},{"slot","Slot","fSlot"},{"channel","Channel","fChannel"},
 {"found","Pulse passed threshold","fFound"},{"peak_sample","Peak sample","fPeak"},
 {"window_start","First integration sample","fFirst"},{"window_end","Last integration sample","fLast"},
 {"clipped","Integration truncated by gate","fClipped"},{"saturated","Any ADC sample saturated","fSaturated"},
 {"raw_integral","Full decoded waveform ADC sum (NaN if absent)","fRawIntegral"},{"baseline","Estimated baseline [ADC counts]","fBaseline"},{"noise","Baseline sample RMS [ADC counts]","fNoise"},
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
Int_t MOLLERTriggerScintillatorDetector::Decode(const THaEvData& ev){
 Clear();fEvnum=ev.GetEvNum();
 const auto* decoder=dynamic_cast<const MOLLERTriggerScintillatorDecoder*>(&ev);
 if(!decoder||!decoder->InputEvent())throw std::runtime_error("TS detector requires MOLLERTriggerScintillatorDecoder");
 const auto& input=*decoder->InputEvent();const auto& meta=input.electronics;
 auto equal=[](double a,double b){return std::abs(a-b)<=1e-9*std::max(1.,std::max(std::abs(a),std::abs(b)));};
 if(meta.nsamples!=fExpectedSamples||meta.bits!=fConfig.bits||!equal(meta.dt_ns,fConfig.dt_ns)||!equal(meta.gate_start_ns,fConfig.gate_start_ns)||!equal(meta.range_v,fConfig.range_v)||!equal(meta.resistance_ohm,fConfig.resistance_ohm))
  throw std::runtime_error("TS digitizer electronics metadata does not match replay DB");
 std::map<int,const TSInput::Channel*> source;
 for(const auto& c:input.channels)source.emplace(c.detid,&c);
 for(size_t i=0;i<fMap.size();i+=6){
  int roc=fMap[i+1],slot=fMap[i+2],channel=fMap[i+3];
  auto src=source.find(fMap[i]);
  if(src!=source.end()&&(src->second->roc!=roc||src->second->slot!=slot||src->second->channel!=channel||src->second->plane!=fMap[i+4]||src->second->sector!=fMap[i+5]))throw std::runtime_error("TS replay DB and input CSV maps disagree");
  fCopyid.push_back(src==source.end()?-1:src->second->copyid);fPlane.push_back(fMap[i+4]);fSector.push_back(fMap[i+5]);
  const auto nhits=ev.GetNumHits(roc,slot,channel);
  if(nhits==0){
   fDetid.push_back(fMap[i]);fRoc.push_back(roc);fSlot.push_back(slot);fChannel.push_back(channel);
   fReadout.push_back(0);fRawIntegral.push_back(std::numeric_limits<double>::quiet_NaN());fWaveforms.emplace_back();continue;
  }
  if(nhits!=static_cast<UInt_t>(fExpectedSamples))throw std::runtime_error("Incomplete TS waveform in slot data");
  fReadout.push_back(1);++fNreadout;
  std::vector<unsigned short> waveform;waveform.reserve(fExpectedSamples);double rawsum=0;
  for(int k=0;k<fExpectedSamples;++k){
   const auto sample=ev.GetData(roc,slot,channel,k);
   if(sample>static_cast<UInt_t>((1u<<fConfig.bits)-1u))throw std::runtime_error("ADC sample exceeds TS DB bit depth");
   waveform.push_back(static_cast<unsigned short>(sample));rawsum+=sample;
  }
  fDetid.push_back(fMap[i]);fRoc.push_back(roc);fSlot.push_back(slot);fChannel.push_back(channel);
  fRawIntegral.push_back(rawsum);fWaveforms.push_back(std::move(waveform));
 }
 fNchannels=fWaveforms.size();return fNchannels;
}
Int_t MOLLERTriggerScintillatorDetector::Reconstruct(){
 try{for(const auto& waveform:fWaveforms){
 MDPulseResult r;
 if(waveform.empty()){
  const double nan=std::numeric_limits<double>::quiet_NaN();
  r.baseline=r.noise=r.height=r.integral=r.charge_pc=nan;
  r.time_quality=32; // Software flag: waveform absent.
 }else r=ReconstructMDPulse(waveform,fConfig);
 fFound.push_back(r.found);fPeak.push_back(r.peak);fFirst.push_back(r.first);fLast.push_back(r.last);fClipped.push_back(r.clipped);fSaturated.push_back(r.saturated);
 fBaseline.push_back(r.baseline);fNoise.push_back(r.noise);fHeight.push_back(r.height);fTime.push_back(r.time_ns);fIntegral.push_back(r.integral);fCharge.push_back(r.charge_pc);
 fPeakTime.push_back(r.peak_time_ns);fThresholdTime.push_back(r.threshold_time_ns);
 fHalfHeightTime.push_back(r.half_height_time_ns);fAppliedThreshold.push_back(r.applied_threshold_adc);
 fThresholdSample.push_back(r.threshold_sample);fThresholdTimeValid.push_back(r.threshold_time_valid);
 fHalfHeightTimeValid.push_back(r.half_height_time_valid);fTimeValid.push_back(r.time_valid);fTimeQuality.push_back(r.time_quality);
 }}catch(const std::exception& e){throw std::runtime_error(std::string("TS reconstruction: ")+e.what());}
 return 0;
}
