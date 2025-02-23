// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   ITSFeeCheck.cxx
/// \author LiAng Zhang
/// \author Jian Liu
/// \author Antonio Palasciano
///

#include "ITS/ITSFeeCheck.h"
#include "QualityControl/MonitorObject.h"
#include "QualityControl/Quality.h"

#include <fairlogger/Logger.h>
#include <TList.h>
#include <TH2.h>
#include <iostream>

namespace o2::quality_control_modules::its
{

Quality ITSFeeCheck::check(std::map<std::string, std::shared_ptr<MonitorObject>>* moMap)
{
  Quality result = Quality::Null;
  bool badStaveCount, badStaveIB, badStaveML, badStaveOL;

  for (auto& [moName, mo] : *moMap) {
    (void)moName;
    for (int iflag = 0; iflag < NFlags; iflag++) {
      if (mo->getName() == Form("LaneStatus/laneStatusFlag%s", mLaneStatusFlag[iflag].c_str())) {
        result = Quality::Good;
        auto* h = dynamic_cast<TH2I*>(mo->getObject());
        if (h->GetMaximum() > 0) {
          result.set(Quality::Bad);
        }
      }
      if (mo->getName() == Form("LaneStatus/laneStatusOverviewFlag%s", mLaneStatusFlag[iflag].c_str())) {
        result.set(Quality::Good);
        auto* hp = dynamic_cast<TH2Poly*>(mo->getObject());
        badStaveIB = false;
        badStaveML = false;
        badStaveOL = false;
        // Initialization of metaData for IB, ML, OL
        result.addMetadata("IB", "good");
        result.addMetadata("ML", "good");
        result.addMetadata("OL", "good");
        for (int ilayer = 0; ilayer < NLayer; ilayer++) {
          int countStave = 0;
          badStaveCount = false;
          for (int ibin = StaveBoundary[ilayer] + 1; ibin <= StaveBoundary[ilayer + 1]; ++ibin) {
            if (ibin <= StaveBoundary[3]) {
              // Check if there are staves in the IB with lane in Bad (bins are filled with %)
              if (hp->GetBinContent(ibin) > std::stod(mCustomParameters["maxbadchipsIB"]) / 9.) {
                badStaveIB = true;
                result.updateMetadata("IB", "medium");
                countStave++;
              }
            } else if (ibin <= StaveBoundary[5]) {
              // Check if there are staves in the MLs with at least 4 lanes in Bad (bins are filled with %)
              if (hp->GetBinContent(ibin) > std::stod(mCustomParameters["maxbadlanesML"]) / NLanePerStaveLayer[ilayer]) {
                badStaveML = true;
                result.updateMetadata("ML", "medium");
                countStave++;
              }
            } else if (ibin <= StaveBoundary[7]) {
              // Check if there are staves in the OLs with at least 7 lanes in Bad (bins are filled with %)
              if (hp->GetBinContent(ibin) > std::stod(mCustomParameters["maxbadlanesOL"]) / NLanePerStaveLayer[ilayer]) {
                badStaveOL = true;
                result.updateMetadata("OL", "medium");
                countStave++;
              }
            }
          } // end loop bins (staves)
          // Initialize metadata for the 7 layers
          result.addMetadata(Form("Layer%d", ilayer), "good");
          // Check if there are more than 25% staves in Bad per layer
          if (countStave > 0.25 * NStaves[ilayer]) {
            badStaveCount = true;
            result.updateMetadata(Form("Layer%d", ilayer), "bad");
          }
        } // end loop over layers
        if (badStaveIB || badStaveML || badStaveOL) {
          result.set(Quality::Medium);
        }
        if (badStaveCount) {
          result.set(Quality::Bad);
        }
      } // end lanestatusOverview
    }
    // Adding summary Plots Checks (General)
    if (mo->getName() == "LaneStatusSummary/LaneStatusSummaryGlobal") {
      result = Quality::Good;
      auto* h = dynamic_cast<TH1I*>(mo->getObject());
      result.addMetadata("SummaryGlobal", "good");
      if (h->GetBinContent(1) + h->GetBinContent(2) + h->GetBinContent(3) > std::stod(mCustomParameters["maxfractionbadlanes"]) * 3816) {
        result.updateMetadata("SummaryGlobal", "bad");
        result.set(Quality::Bad);
      }
    } // end summary loop
    if (mo->getName() == Form("RDHSummary")) {
      result = Quality::Good;
      auto* h = dynamic_cast<TH2I*>(mo->getObject());
      if (h->GetMaximum() > 0) {
        result.set(Quality::Bad);
      }
    }

    if (mo->getName() == "TriggerVsFeeid") {
      result.set(Quality::Good);
      auto* h = dynamic_cast<TH2I*>(mo->getObject());
      int counttrgflags[NTrg] = { 0 };
      int cutvalue[NTrg] = { 432, 432, 0, 0, 432, 0, 0, 0, 0, 432, 0, 432, 0 };
      std::vector<int> skipbins = convertToIntArray(mCustomParameters["skipbinstrg"]);
      std::vector<int> skipfeeid = convertToIntArray(mCustomParameters["skipfeeids"]);
      for (int itrg = 1; itrg <= h->GetNbinsY(); itrg++) {
        result.addMetadata(h->GetYaxis()->GetBinLabel(itrg), "good");
        for (int ifee = 1; ifee <= h->GetNbinsX(); ifee++) {
          if (h->GetBinContent(ifee, itrg) > 0) {
            counttrgflags[itrg - 1]++;
          }
        }
      }

      for (int itrg = 0; itrg < h->GetNbinsY(); itrg++) {
        if (std::find(skipbins.begin(), skipbins.end(), itrg + 1) != skipbins.end()) {
          continue;
        }
        if ((itrg == 0 || itrg == 1 || itrg == 4 || itrg == 9 || itrg == 11) && counttrgflags[itrg] < cutvalue[itrg] - (int)skipfeeid.size()) {
          result.updateMetadata(h->GetYaxis()->GetBinLabel(itrg + 1), "bad");
          result.set(Quality::Bad);
        } else if ((itrg == 2 || itrg == 3 || itrg == 5 || itrg == 6 || itrg == 7 || itrg == 8 || itrg == 10 || itrg == 12) && counttrgflags[itrg] > cutvalue[itrg]) {
          result.updateMetadata(h->GetYaxis()->GetBinLabel(itrg + 1), "bad");
          result.set(Quality::Bad);
        }
      }
    }

    if (mo->getName() == "PayloadSize") {
      auto* h = dynamic_cast<TH2F*>(mo->getObject());
      result.set(Quality::Good);
      result.addMetadata("CheckTechnicals", "good");
      result.addMetadata("CheckTechnicalsFeeid", "good");
      std::vector<int> skipfeeid = convertToIntArray(mCustomParameters["skipfeeids"]);
      if (h->Integral(1, 432, h->GetYaxis()->FindBin(1000), h->GetYaxis()->FindBin(20000)) > 0) {
        result.set(Quality::Bad);
        result.updateMetadata("CheckTechnicals", "bad");
      }
      int countFeeids = 0;
      for (int ix = 1; ix <= h->GetNbinsX(); ix++) {
        for (int iy = 1; iy <= h->GetNbinsY(); iy++) {
          if (h->GetBinContent(ix, iy) > 0) {
            countFeeids++;
            break;
          }
        }
      }
      if (countFeeids < 432 - (int)skipfeeid.size()) {
        result.set(Quality::Bad);
        result.updateMetadata("CheckTechnicalsFeeid", "bad");
      }
    }
  } // end loop on MOs
  return result;
} // end check

std::string ITSFeeCheck::getAcceptedType() { return "TH2I, TH2Poly"; }

void ITSFeeCheck::beautify(std::shared_ptr<MonitorObject> mo, Quality checkResult)
{
  TString status;
  int textColor;

  for (int iflag = 0; iflag < NFlags; iflag++) {
    if (mo->getName() == Form("LaneStatus/laneStatusFlag%s", mLaneStatusFlag[iflag].c_str())) {
      auto* h = dynamic_cast<TH2I*>(mo->getObject());
      if (checkResult == Quality::Good) {
        status = "Quality::GOOD";
        textColor = kGreen;
      } else if (checkResult == Quality::Bad) {
        status = "Quality::BAD (call expert)";
        textColor = kRed;
      }
      tInfo = std::make_shared<TLatex>(0.12, 0.835, Form("#bf{%s}", status.Data()));
      tInfo->SetTextColor(textColor);
      tInfo->SetTextSize(0.04);
      tInfo->SetTextFont(43);
      tInfo->SetNDC();
      h->GetListOfFunctions()->Add(tInfo->Clone());
    }
    if (mo->getName() == Form("LaneStatus/laneStatusOverviewFlag%s", mLaneStatusFlag[iflag].c_str())) {
      auto* hp = dynamic_cast<TH2Poly*>(mo->getObject());
      if (checkResult == Quality::Good) {
        status = "Quality::GOOD";
        textColor = kGreen;
      } else if (checkResult == Quality::Bad || checkResult == Quality::Medium) {
        status = "Quality::BAD (call expert)";
        textColor = kRed;
        if (strcmp(checkResult.getMetadata("IB").c_str(), "medium") == 0) {
          status = "Quality::Medium (do not call, inform expert on MM)";
          textColor = kOrange;
          tInfoIB = std::make_shared<TLatex>(0.40, 0.55, Form("Inner Barrel has stave(s) with >%s chips in %s", mCustomParameters["maxbadchipsIB"].c_str(), mLaneStatusFlag[iflag].c_str()));
          tInfoIB->SetTextColor(kOrange);
          tInfoIB->SetTextSize(0.03);
          tInfoIB->SetTextFont(43);
          tInfoIB->SetNDC();
          hp->GetListOfFunctions()->Add(tInfoIB->Clone());
        }
        if (strcmp(checkResult.getMetadata("ML").c_str(), "medium") == 0) {
          status = "Quality::Medium (do not call, inform expert on MM)";
          textColor = kOrange;
          tInfoML = std::make_shared<TLatex>(0.42, 0.62, Form("ML have stave(s) with >%s lanes in %s", mCustomParameters["maxbadlanesML"].c_str(), mLaneStatusFlag[iflag].c_str()));
          tInfoML->SetTextColor(kOrange);
          tInfoML->SetTextSize(0.03);
          tInfoML->SetTextFont(43);
          tInfoML->SetNDC();
          hp->GetListOfFunctions()->Add(tInfoML->Clone());
        }
        if (strcmp(checkResult.getMetadata("OL").c_str(), "medium") == 0) {
          status = "Quality::Medium (do not call, inform expert on MM)";
          textColor = kOrange;
          tInfoOL = std::make_shared<TLatex>(0.415, 0.78, Form("OL have staves with >%s lanes in %s", mCustomParameters["maxbadlanesOL"].c_str(), mLaneStatusFlag[iflag].c_str()));
          tInfoOL->SetTextColor(kOrange);
          tInfoOL->SetTextSize(0.03);
          tInfoOL->SetTextFont(43);
          tInfoOL->SetNDC();
          hp->GetListOfFunctions()->Add(tInfoOL->Clone());
        }
        for (int ilayer = 0; ilayer < NLayer; ilayer++) {
          if (strcmp(checkResult.getMetadata(Form("Layer%d", ilayer)).c_str(), "bad") == 0) {
            status = "Quality::Bad (call expert)";
            textColor = kRed;
            std::string cut = ilayer < 3 ? mCustomParameters["maxbadchipsIB"] : ilayer < 5 ? mCustomParameters["maxbadlanesML"]
                                                                                           : mCustomParameters["maxbadlanesOL"];
            tInfoLayers[ilayer] = std::make_shared<TLatex>(0.37, minTextPosY[ilayer], Form("Layer %d has > 25%% staves with >%s %s in %s", ilayer, cut.c_str(), ilayer < 3 ? "chips" : "lanes", mLaneStatusFlag[iflag].c_str()));
            tInfoLayers[ilayer]->SetTextColor(kRed);
            tInfoLayers[ilayer]->SetTextSize(0.03);
            tInfoLayers[ilayer]->SetTextFont(43);
            tInfoLayers[ilayer]->SetNDC();
            hp->GetListOfFunctions()->Add(tInfoLayers[ilayer]->Clone());
          } // end check result over layer
        }   // end of loop over layers
      }
      tInfo = std::make_shared<TLatex>(0.12, 0.835, Form("#bf{%s}", status.Data()));
      tInfo->SetTextColor(textColor);
      tInfo->SetTextSize(0.03);
      tInfo->SetTextFont(43);
      tInfo->SetNDC();
      hp->GetListOfFunctions()->Add(tInfo->Clone());
    }
  } // end flags
  if (mo->getName() == "LaneStatusSummary/LaneStatusSummaryGlobal") {
    auto* h = dynamic_cast<TH1I*>(mo->getObject());
    if (checkResult == Quality::Good) {
      status = "Quality::GOOD";
      textColor = kGreen;
    } else if (checkResult == Quality::Bad) {
      status = "Quality::BAD (call expert)";
      textColor = kRed;
      if (strcmp(checkResult.getMetadata("SummaryGlobal").c_str(), "bad") == 0) {
        tInfoSummary = std::make_shared<TLatex>(0.12, 0.5, Form(">%.0f %% of the lanes are bad", std::stod(mCustomParameters["maxfractionbadlanes"]) * 100));
        tInfoSummary->SetTextColor(kRed);
        tInfoSummary->SetTextSize(0.05);
        tInfoSummary->SetTextFont(43);
        tInfoSummary->SetNDC();
        h->GetListOfFunctions()->Add(tInfoSummary->Clone());
      }
    }
    tInfo = std::make_shared<TLatex>(0.12, 0.835, Form("#bf{%s}", status.Data()));
    tInfo->SetTextColor(textColor);
    tInfo->SetTextSize(0.04);
    tInfo->SetTextFont(43);
    tInfo->SetNDC();
    h->GetListOfFunctions()->Add(tInfo->Clone());
  }
  if (mo->getName() == Form("RDHSummary")) {
    auto* h = dynamic_cast<TH2I*>(mo->getObject());
    if (checkResult == Quality::Good) {
      status = "Quality::GOOD";
      textColor = kGreen;
    } else if (checkResult == Quality::Bad) {
      status = "Quality::BAD (call expert)";
      textColor = kRed;
    }
    tInfo = std::make_shared<TLatex>(0.12, 0.835, Form("#bf{%s}", status.Data()));
    tInfo->SetTextColor(textColor);
    tInfo->SetTextSize(0.04);
    tInfo->SetTextFont(43);
    tInfo->SetNDC();
    h->GetListOfFunctions()->Add(tInfo->Clone());
  }

  // trigger plot
  if (mo->getName() == "TriggerVsFeeid") {
    auto* h = dynamic_cast<TH2I*>(mo->getObject());
    if (checkResult == Quality::Good) {
      status = "Quality::GOOD";
      textColor = kGreen;
    } else if (checkResult == Quality::Bad) {
      status = "Quality::BAD (call expert)";
      textColor = kRed;
      for (int itrg = 1; itrg <= h->GetNbinsY(); itrg++) {
        LOG(info) << checkResult.getMetadata(h->GetYaxis()->GetBinLabel(itrg)).c_str();
        if (strcmp(checkResult.getMetadata(h->GetYaxis()->GetBinLabel(itrg)).c_str(), "bad") == 0) {
          std::string extraText = (!strcmp(h->GetYaxis()->GetBinLabel(itrg), "PHYSICS")) ? "(OK if it's COSMICS/SYNTHETIC)" : "";
          tInfoTrg[itrg - 1] = std::make_shared<TLatex>(0.3, 0.1 + 0.05 * (itrg - 1), Form("Trigger flag %s of bad quality %s", h->GetYaxis()->GetBinLabel(itrg), extraText.c_str()));
          tInfoTrg[itrg - 1]->SetTextColor(kRed);
          tInfoTrg[itrg - 1]->SetTextSize(0.03);
          tInfoTrg[itrg - 1]->SetTextFont(43);
          tInfoTrg[itrg - 1]->SetNDC();
          h->GetListOfFunctions()->Add(tInfoTrg[itrg - 1]->Clone());
        }
      }
    }
    tInfo = std::make_shared<TLatex>(0.12, 0.835, Form("#bf{%s}", status.Data()));
    tInfo->SetTextColor(textColor);
    tInfo->SetTextSize(0.04);
    tInfo->SetTextFont(43);
    tInfo->SetNDC();
    h->GetListOfFunctions()->Add(tInfo->Clone());
  }

  // payload size plot
  if (mo->getName() == "PayloadSize") {
    auto* h = dynamic_cast<TH2F*>(mo->getObject());
    if (checkResult == Quality::Good) {
      status = "Quality::GOOD";
      textColor = kGreen;
    } else {
      status = "Quality::Bad (call expert)";
      textColor = kRed;
      if (strcmp(checkResult.getMetadata("CheckTechnicals").c_str(), "bad") == 0) {
        tInfoPL[0] = std::make_shared<TLatex>(0.3, 0.6, "Payload size too large for technical runs");
        tInfoPL[0]->SetNDC();
        tInfoPL[0]->SetTextFont(43);
        tInfoPL[0]->SetTextSize(0.04);
        tInfoPL[0]->SetTextColor(kRed);
        h->GetListOfFunctions()->Add(tInfoPL[0]->Clone());
      }
      if (strcmp(checkResult.getMetadata("CheckTechnicalsFeeid").c_str(), "bad") == 0) {
        tInfoPL[1] = std::make_shared<TLatex>(0.3, 0.55, "Payload size is missing for some FeeIDs");
        tInfoPL[1]->SetNDC();
        tInfoPL[1]->SetTextFont(43);
        tInfoPL[1]->SetTextSize(0.04);
        tInfoPL[1]->SetTextColor(kRed);
        h->GetListOfFunctions()->Add(tInfoPL[1]->Clone());
      }
    }
    tInfo = std::make_shared<TLatex>(0.12, 0.75, Form("#bf{%s}", status.Data()));
    tInfo->SetTextColor(textColor);
    tInfo->SetTextSize(0.04);
    tInfo->SetTextFont(43);
    tInfo->SetNDC();
    h->GetListOfFunctions()->Add(tInfo->Clone());
  }
} // end beautify

std::vector<int> ITSFeeCheck::convertToIntArray(std::string input)
{
  std::replace(input.begin(), input.end(), ',', ' ');
  std::istringstream stringReader{ input };

  std::vector<int> result;
  int number;
  while (stringReader >> number) {
    result.push_back(number);
  }

  return result;
}

} // namespace o2::quality_control_modules::its
