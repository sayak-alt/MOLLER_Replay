/* Replay script for analysis of mock MOLLER MD PMT data.
 * Load libmoller.so first (the existing replay.C launcher does this).
 * Existing launcher: replay_moller_md(input, output, nevents).
 * GEM-style use:     replay_moller_md(filebase, nevents).
 */
#include "MOLLERMDPodd.h"
#include "MOLLERApparatus.h"
#include "TSystem.h"
#include "TList.h"
#include "THaRunBase.h"
#include "THaAnalyzer.h"
#include "THaInterface.h"
#include "THaGlobals.h"
#include "TString.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void replay_moller_md(const char* filebase, const char* output,
                      Long64_t nevents=-1)
{
  if(!filebase || !*filebase || !output || !*output)
    throw std::runtime_error("Input and output filenames are required");
  if(nevents==0 || nevents < -1)
    throw std::runtime_error("Use a positive event count or -1 for all events");
  if(gHaApps->GetSize() || gHaPhysics->GetSize())
    throw std::runtime_error("Start a fresh analyzer session for MD replay");

  // Locate the digitised simulation input.
  // A filename base gets .root appended; a full .root filename is also accepted.
  TString run_file(filebase);
  if(!run_file.EndsWith(".root")) run_file += ".root";
  const char* data_dir = gSystem->Getenv("DATA_DIR");
  if(data_dir && *data_dir && !run_file.Contains("/"))
    run_file = TString::Format("%s/%s", data_dir, run_file.Data());
  gSystem->ExpandPathName(run_file);
  if(gSystem->AccessPathName(run_file))
    throw std::runtime_error(std::string("Input file does not exist: ")+run_file.Data());

  // Locate the output. Explicit paths override OUT_DIR.
  TString outfilename(output);
  const char* out_dir = gSystem->Getenv("OUT_DIR");
  if(out_dir && *out_dir && !outfilename.Contains("/"))
    outfilename = TString::Format("%s/%s", out_dir, outfilename.Data());
  gSystem->ExpandPathName(outfilename);
  if(std::filesystem::weakly_canonical(run_file.Data()) ==
     std::filesystem::weakly_canonical(outfilename.Data()))
    throw std::runtime_error("Input and output refer to the same file");
  if(gSystem->AccessPathName(gSystem->DirName(outfilename)))
    throw std::runtime_error("Output directory does not exist");

  // Output definitions and detector database are provided by MOLLER-replay.
  const char* replay_dir = gSystem->Getenv("MOLLER_REPLAY");
  if(!replay_dir || !*replay_dir)
    throw std::runtime_error("Source setup.csh before starting analyzer");
  TString odef_filename = TString::Format("%s/replay/replay_moller_md.odef", replay_dir);
  if(gSystem->AccessPathName(odef_filename))
    throw std::runtime_error("Missing replay_moller_md.odef");
  if(!gSystem->Getenv("DB_DIR"))
    gSystem->Setenv("DB_DIR", TString::Format("%s/DB", replay_dir));

  // Select the MD simulation decoder. It populates Podd slot data directly.
  // A sim_cratemap is not consumed by the current MD decoder.
  THaInterface::SetDecoder(MOLLERMDDecoder::Class());

  // Create apparatus and MD detector.
  // MOLLERApparatus's constructor already calls AddDetector() for its MD.
  // Do not add a second MD detector here.
  MOLLERApparatus* moller = new MOLLERApparatus("moller");
  gHaApps->Add(moller);

  THaRunBase* run = new MOLLERMDRun(run_file.Data());
  run->SetFirstEvent(0);
  if(nevents > 0) run->SetLastEvent(nevents - 1);
  run->SetDataRequired(0);

  THaAnalyzer* analyzer = new THaAnalyzer;
  Long64_t status = -1;
  try {
    analyzer->SetVerbosity(2);
    analyzer->SetMarkInterval(100);
    analyzer->EnableBenchmarks();

    // Define output, summary log and variables to write.
    analyzer->SetOutFile(outfilename.Data());
    TString logfilename(outfilename);
    if(logfilename.EndsWith(".root")) logfilename.Remove(logfilename.Length()-5);
    logfilename += ".log";
    analyzer->SetSummaryFile(logfilename.Data());
    analyzer->SetOdefFile(odef_filename.Data());

    std::cout << "MOLLER MD replay input: " << run_file << '\n'
              << "Output: " << outfilename << '\n'
              << "Summary: " << logfilename << '\n'
              << "About to process " << nevents << " events (-1 means all)\n";

    // Process initializes the simulation run and the apparatus/detector.
    // This retains the initialization sequence validated with the MD adapter.
    status = analyzer->Process(run);
    analyzer->Close();
  }
  catch(...) {
    analyzer->Close();
    delete analyzer;
    delete run;
    gHaApps->Remove(moller);
    delete moller;
    throw;
  }

  // Clean up only the objects owned by this replay.
  delete analyzer;
  delete run;
  gHaApps->Remove(moller);
  delete moller;
  if(status < 0) throw std::runtime_error("MD replay failed; inspect the analyzer output");
}

// Same two-argument calling convention as the uploaded GEM replay macro.
void replay_moller_md(const char* filebase, Long64_t nevents=-1)
{
  replay_moller_md(filebase, "moller_md_dig_replayed.root", nevents);
}
