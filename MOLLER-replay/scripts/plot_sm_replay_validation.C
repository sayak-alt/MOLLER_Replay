#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TGraphAsymmErrors.h>
#include <TCanvas.h>
#include <TLine.h>
#include <TStyle.h>
#include <TColor.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void plot_sm_replay_validation(
    const char* rootfile = "SM_all_validation.root",
    const char* csvfile  = "SM_all_validation.csv",
    const char* prefix  = "SM_replay")
{
  TFile file(rootfile, "READ");

  if (file.IsZombie())
    throw std::runtime_error("Cannot open validation ROOT file");

  TH2* correlation = nullptr;
  TH1* residual = nullptr;
  TH1* fractional = nullptr;
  TGraphAsymmErrors* efficiency = nullptr;

  file.GetObject("smq_correlation", correlation);
  file.GetObject("smq_residual", residual);
  file.GetObject("smq_fractional", fractional);
  file.GetObject("smq_efficiency", efficiency);

  if (!correlation || !residual || !fractional || !efficiency)
    throw std::runtime_error(
        "Missing objects: use the output of validate_sm_charge.C");

  gStyle->SetOptStat(0);
  gStyle->SetPalette(kBird);

  auto save = [&](TCanvas& canvas, const char* suffix) {
    const std::string name = std::string(prefix) + suffix;
    canvas.SaveAs((name + ".png").c_str());
    canvas.SaveAs((name + ".pdf").c_str());
  };

  // 1. Reconstructed charge versus generated charge.
  {
    TCanvas canvas("sm_charge_correlation", "Charge correlation", 900, 750);
    canvas.SetLeftMargin(0.14);
    canvas.SetRightMargin(0.16);
    canvas.SetBottomMargin(0.14);

    correlation->SetTitle(
        "Detected, clean positive-PE pulses;"
        "Generated charge [pC];Replayed charge [pC]");

    correlation->Draw("COLZ");

    const double maximum =
        std::min(correlation->GetXaxis()->GetXmax(),
                 correlation->GetYaxis()->GetXmax());

    TLine equality(0, 0, maximum, maximum);
    equality.SetLineColor(kRed + 1);
    equality.SetLineStyle(2);
    equality.SetLineWidth(2);
    equality.Draw();

    save(canvas, "_01_charge_correlation");
  }

  // 2. Absolute charge residual.
  {
    TCanvas canvas("sm_charge_residual", "Charge residual", 900, 650);
    canvas.SetLeftMargin(0.14);
    canvas.SetBottomMargin(0.15);

    residual->SetTitle(
        "Detected, clean positive-PE pulses;"
        "Q_{replay}-Q_{generated} [pC];Selected channel-events");

    residual->SetLineColor(kBlue + 1);
    residual->SetLineWidth(2);
    residual->Draw("HIST");

    TLine zero(0, 0, 0, residual->GetMaximum());
    zero.SetLineColor(kRed + 1);
    zero.SetLineStyle(2);
    zero.Draw();

    save(canvas, "_02_charge_residual");
  }

  // 3. Fractional charge residual.
  {
    TCanvas canvas("sm_fractional_residual",
                   "Fractional residual", 900, 650);

    canvas.SetLeftMargin(0.14);
    canvas.SetBottomMargin(0.15);

    fractional->SetTitle(
        "Detected, clean positive-PE pulses;"
        "100 (Q_{replay}-Q_{generated})/Q_{generated} [%];"
        "Selected channel-events");

    fractional->SetLineColor(kBlue + 1);
    fractional->SetLineWidth(2);
    fractional->Draw("HIST");

    save(canvas, "_03_fractional_residual");
  }

  // 4. Detection efficiency, using the original validation denominator.
  {
    TCanvas canvas("sm_detection_efficiency",
                   "Detection efficiency", 900, 650);

    canvas.SetLeftMargin(0.14);
    canvas.SetBottomMargin(0.15);

    efficiency->SetTitle(
        "Matched positive-PE channels;"
        "Generated charge [pC];Pulse-detection efficiency");

    efficiency->SetMinimum(0.0);
    efficiency->SetMaximum(1.05);
    efficiency->SetMarkerStyle(20);
    efficiency->SetMarkerColor(kBlue + 1);
    efficiency->SetLineColor(kBlue + 1);
    efficiency->Draw("AP");

    save(canvas, "_04_detection_efficiency");
  }

  // Read individual charge comparisons for bias/spread versus charge.
  std::ifstream csv(csvfile);

  if (!csv)
    throw std::runtime_error(
        "Cannot open validation CSV; the first four plots were exported");

  std::string line;
  std::getline(csv, line);

  if (!line.empty() && line.back() == '\r')
    line.pop_back();

  const std::string expectedHeader =
      "evnum,detid,npe_poiss,generated_charge_pc,"
      "replayed_charge_pc,residual_pc";

  if (line != expectedHeader)
    throw std::runtime_error("Unexpected validation CSV header");

  struct Comparison {
    double truth;
    double residual;
  };

  std::vector<Comparison> comparisons;
  double largestCharge = 0.0;

  while (std::getline(csv, line)) {
    if (line.find_first_not_of(" \t\r") == std::string::npos)
      continue;

    std::stringstream stream(line);
    std::string cell;
    std::vector<std::string> cells;

    while (std::getline(stream, cell, ','))
      cells.push_back(cell);

    if (cells.size() != 6)
      throw std::runtime_error("Malformed validation CSV row");

    const double truth = std::stod(cells[3]);
    const double delta = std::stod(cells[5]);

    if (!std::isfinite(truth) || truth <= 0 ||
        !std::isfinite(delta))
      throw std::runtime_error("Invalid CSV charge or residual");

    comparisons.push_back({truth, delta});
    largestCharge = std::max(largestCharge, truth);
  }

  if (comparisons.empty()) {
    std::cout << "Exported four plots. No selected charge comparisons "
                 "for bias/spread plots.\n";
    return;
  }

  constexpr int numberOfBins = 15;
  constexpr int minimumEntries = 30;

  struct Bin {
    int count = 0;
    double meanCharge = 0;
    double meanResidual = 0;
    double m2 = 0;

    void Add(double charge, double delta)
    {
      ++count;

      meanCharge += (charge - meanCharge) / count;

      const double difference = delta - meanResidual;
      meanResidual += difference / count;
      m2 += difference * (delta - meanResidual);
    }

    double StandardDeviation() const
    {
      return count > 1
          ? std::sqrt(std::max(0.0, m2 / (count - 1)))
          : 0.0;
    }
  };

  std::vector<Bin> bins(numberOfBins);

  for (const auto& comparison : comparisons) {
    int index = static_cast<int>(
        comparison.truth / largestCharge * numberOfBins);

    index = std::min(numberOfBins - 1, index);
    bins[index].Add(comparison.truth, comparison.residual);
  }

  TGraphErrors bias;
  TGraph spread;

  double biasMinimum = 0.0;
  double biasMaximum = 0.0;

  for (const auto& bin : bins) {
    if (bin.count < minimumEntries)
      continue;

    const double sigma = bin.StandardDeviation();
    const double standardError = sigma / std::sqrt(bin.count);

    const int point = bias.GetN();

    bias.SetPoint(point, bin.meanCharge, bin.meanResidual);
    bias.SetPointError(point, 0.0, standardError);
    spread.SetPoint(point, bin.meanCharge, sigma);

    biasMinimum = std::min(
        biasMinimum, bin.meanResidual - standardError);

    biasMaximum = std::max(
        biasMaximum, bin.meanResidual + standardError);
  }

  if (bias.GetN() == 0) {
    std::cout << "Exported four plots. No charge bin contains at least "
              << minimumEntries << " selected comparisons.\n";
    return;
  }

  // 5. Mean residual versus generated charge.
  {
    TCanvas canvas("sm_charge_bias", "Charge bias", 900, 650);
    canvas.SetLeftMargin(0.14);
    canvas.SetBottomMargin(0.15);

    bias.SetTitle(
        "Detected, clean positive-PE pulses;"
        "Generated charge [pC];Mean charge residual [pC]");

    bias.SetMarkerStyle(20);
    bias.SetMarkerColor(kBlue + 1);
    bias.SetLineColor(kBlue + 1);

    const double padding =
        std::max(0.05, 0.15 * (biasMaximum - biasMinimum));

    bias.SetMinimum(biasMinimum - padding);
    bias.SetMaximum(biasMaximum + padding);
    bias.Draw("AP");
    bias.GetXaxis()->SetLimits(0.0, largestCharge * 1.05);

    TLine zero(0, 0, largestCharge, 0);
    zero.SetLineColor(kRed + 1);
    zero.SetLineStyle(2);
    zero.Draw();

    save(canvas, "_05_charge_bias");
  }

  // 6. Residual sample spread versus generated charge.
  {
    TCanvas canvas("sm_charge_spread", "Charge residual spread", 900, 650);
    canvas.SetLeftMargin(0.15);
    canvas.SetBottomMargin(0.15);

    spread.SetTitle(
        "Detected, clean positive-PE pulses;"
        "Generated charge [pC];Residual standard deviation [pC]");

    spread.SetMarkerStyle(20);
    spread.SetMarkerColor(kBlue + 1);
    spread.SetLineColor(kBlue + 1);
    spread.SetMinimum(0.0);
    spread.Draw("AP");
    spread.GetXaxis()->SetLimits(0.0, largestCharge * 1.05);

    save(canvas, "_06_charge_spread");
  }

  std::cout
      << "Exported six plots as PNG and PDF.\n"
      << "Populated charge bins: " << bias.GetN() << "\n"
      << "Minimum comparisons per charge bin: " << minimumEntries << "\n"
      << "Bias error bars: standard errors of bin means.\n"
      << "Spread: sample standard deviation, without a Gaussian fit.\n"
      << "Charge statistics remain conditional on pulse detection.\n"
      << "Generated charge includes charge outside the integration window.\n";
}
