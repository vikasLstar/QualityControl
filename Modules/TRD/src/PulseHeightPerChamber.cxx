#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLine.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TProfile.h>
#include <TProfile2D.h>
#include <TStopwatch.h>

#include "DataFormatsTRD/Constants.h"
#include "DataFormatsTRD/Digit.h"
#include "DataFormatsTRD/Tracklet64.h"
#include "DataFormatsTRD/TriggerRecord.h"
#include "QualityControl/ObjectsManager.h"
#include "QualityControl/TaskInterface.h"
#include "QualityControl/QcInfoLogger.h"
#include "TRD/PulseHeightPerChamber.h"
#include <Framework/InputRecord.h>
#include <Framework/InputRecordWalker.h>
#include <gsl/span>
#include <map>
#include <tuple>
#include "CCDB/BasicCCDBManager.h"
namespace o2::quality_control_modules::trd
{
PulseHeightPerChamber::~PulseHeightPerChamber()
{
}
  
void PulseHeightPerChamber::retrieveCCDBSettings()
{
  if (auto param = mCustomParameters.find("ccdbtimestamp"); param != mCustomParameters.end()) {
    mTimestamp = std::stol(mCustomParameters["ccdbtimestamp"]);
    ILOG(Info, Support) << "configure() : using ccdbtimestamp = " << mTimestamp << ENDM;
  } else {
    mTimestamp = o2::ccdb::getCurrentTimestamp();
    ILOG(Info, Support) << "configure() : using default timestam of now = " << mTimestamp << ENDM;
  }
  auto& mgr = o2::ccdb::BasicCCDBManager::instance();
  mgr.setTimestamp(mTimestamp);
  mNoiseMap = mgr.get<o2::trd::NoiseStatusMCM>("/TRD/Calib/NoiseMapMCM");
  if (mNoiseMap == nullptr) {
    ILOG(Info, Support) << "mNoiseMap is null, no noisy mcm reduction" << ENDM;
  }
  mChamberStatus = mgr.get<o2::trd::HalfChamberStatusQC>("/TRD/Calib/HalfChamberStatusQC");
  if (mChamberStatus == nullptr) {
    ILOG(Info, Support) << "mChamberStatus is null, no chamber status to display" << ENDM;
  }
}

void PulseHeightPerChamber::buildChamberIgnoreBP()
{
  mChambersToIgnoreBP.reset();
  // Vector of string to save tokens
  std::vector<std::string> tokens;
  // stringstream class check1
  std::stringstream check1(mChambersToIgnore);
  std::string intermediate;
  // Tokenizing w.r.t. space ','
  while (getline(check1, intermediate, ',')) {
    tokens.push_back(intermediate);
  }
  // Printing the token vector
  for (auto& token : tokens) {
    //token now holds something like 16_3_0
    std::vector<std::string> parts;
    std::stringstream indexcheck(token);
    std::string tokenpart;
    // Tokenizing w.r.t. space ','
    while (getline(indexcheck, tokenpart, '_')) {
      parts.push_back(tokenpart);
    }
    // now flip the bit related to the sector:stack:layer stored in parts.
    int sector = std::stoi(parts[0]);
    int stack = std::stoi(parts[1]);
    int layer = std::stoi(parts[2]);
    mChambersToIgnoreBP.set(sector * o2::trd::constants::NSTACK * o2::trd::constants::NLAYER + stack * o2::trd::constants::NLAYER + layer);
  }
}

void PulseHeightPerChamber::drawLinesOnPulseHeight(TH1F* h)
{
  TLine* lmin = new TLine(mPulseHeightPeakRegion.first, 0, mPulseHeightPeakRegion.first, 1e9);
  TLine* lmax = new TLine(mPulseHeightPeakRegion.second, 0, mPulseHeightPeakRegion.second, 1e9);
  lmin->SetLineStyle(2);
  lmax->SetLineStyle(2);
  lmin->SetLineColor(kRed);
  lmax->SetLineColor(kRed);
  h->GetListOfFunctions()->Add(lmin);
  h->GetListOfFunctions()->Add(lmax);
}
  
void PulseHeightPerChamber::buildHistograms()
{
  //mDigitHCID.reset(new TH1F("digithcid", "Digit distribution over Halfchambers", 1080, 0, 1080));
  //getObjectsManager()->startPublishing(mDigitHCID.get());
  mDigitsPerEvent.reset(new TH1F("digitsperevent", "Digits per Event", 10000, 0, 10000));
  getObjectsManager()->startPublishing(mDigitsPerEvent.get());

  mDigitsSizevsTrackletSize.reset(new TH2F("digitsvstracklets", "Tracklets Count vs Digits Count per event; Number of Tracklets;Number Of Digits", 2500, 0, 2500, 2500, 0, 2500));
  getObjectsManager()->startPublishing(mDigitsSizevsTrackletSize.get());

  mPulseHeightpro.reset(new TProfile("PulseHeight/mPulseHeightpro", "Pulse height profile  plot;Timebins;Counts", 30, -0.5, 29.5));
  mPulseHeightpro.get()->Sumw2();
  getObjectsManager()->startPublishing(mPulseHeightpro.get());

  mPulseHeightpro_ths.reset(new TProfile("PulseHeight/mPulseHeightpro_ths", "Pulse height profile  plot_with_threshold;Timebins;Counts", 30, -0.5, 29.5));
  mPulseHeightpro_ths.get()->Sumw2();
  getObjectsManager()->startPublishing(mPulseHeightpro_ths.get());

   mPulseHeightpro_conc.reset(new TProfile("PulseHeight/mPulseHeightpro_conc", "Pulse height profile  plot_with_consecutive;Timebins;Counts", 30, -0.5, 29.5));
  mPulseHeightpro_conc.get()->Sumw2();
  getObjectsManager()->startPublishing(mPulseHeightpro_conc.get());

  /*  int cn = 0;
  int sm = 0;

  for (int count = 0; count < 540; ++count) {
    sm = count / 30;
    std::string label = fmt::format("PulseHeightPerChamber/pulseheight_{0:02d}_{1}_{2}", sm, cn / 6, cn % 6);
    std::string title = fmt::format("{0:02d}_{1}_{2};Timebin;Chamber", sm, cn / 6, cn % 6);
    TH1F* h = new TH1F(label.c_str(), title.c_str(), 30, -0.5, 29.5);
    mPulseHeightPerChamber_1D[count].reset(h);
    getObjectsManager()->startPublishing(h);
    cn++;
    if (cn > 29)
      cn = 0;
  }
*/
}

void PulseHeightPerChamber::initialize(o2::framework::InitContext& /*ctx*/)
{
  ILOG(Info) << "initialize TRDDigitQcTask" << ENDM; // QcInfoLogger is used. FairMQ logs will go to there as well.

  // this is how to get access to custom parameters defined in the config file at qc.tasks.<task_name>.taskParameters
  if (auto param = mCustomParameters.find("driftregionstart"); param != mCustomParameters.end()) {
    mDriftRegion.first = stof(param->second);
    ILOG(Info, Support) << "configure() : using peakregionstart = " << mDriftRegion.first << ENDM;
  } else {
    mDriftRegion.first = 7.0;
    ILOG(Info, Support) << "configure() : using default dritfregionstart = " << mDriftRegion.first << ENDM;
  }
  if (auto param = mCustomParameters.find("driftregionend"); param != mCustomParameters.end()) {
    mDriftRegion.second = stof(param->second);
    ILOG(Info, Support) << "configure() : using peakregionstart = " << mDriftRegion.second << ENDM;
  } else {
    mDriftRegion.second = 20.0;
    ILOG(Info, Support) << "configure() : using default dritfregionstart = " << mDriftRegion.second << ENDM;
  }
  if (auto param = mCustomParameters.find("pulseheightpeaklower"); param != mCustomParameters.end()) {
    mPulseHeightPeakRegion.first = stof(param->second);
    ILOG(Info, Support) << "configure() : using pulsehheightlower	= " << mPulseHeightPeakRegion.first << ENDM;
  } else {
    mPulseHeightPeakRegion.first = 0.0;
    ILOG(Info, Support) << "configure() : using default pulseheightlower = " << mPulseHeightPeakRegion.first << ENDM;
  }
  if (auto param = mCustomParameters.find("pulseheightpeakupper"); param != mCustomParameters.end()) {
    mPulseHeightPeakRegion.second = stof(param->second);
    ILOG(Info, Support) << "configure() : using pulsehheightupper	= " << mPulseHeightPeakRegion.second << ENDM;
  } else {
    mPulseHeightPeakRegion.second = 5.0;
    ILOG(Info, Support) << "configure() : using default pulseheightupper = " << mPulseHeightPeakRegion.second << ENDM;
  }
  if (auto param = mCustomParameters.find("skippedshareddigits"); param != mCustomParameters.end()) {
    mSkipSharedDigits = stod(param->second);
    ILOG(Info, Support) << "configure() : using skip shared digits 	= " << mSkipSharedDigits << ENDM;
  } else {
    mSkipSharedDigits = false;
    ILOG(Info, Support) << "configure() : using default skip shared digits = " << mSkipSharedDigits << ENDM;
  }
  if (auto param = mCustomParameters.find("pulseheightthreshold"); param != mCustomParameters.end()) {
    mPulseHeightThreshold = stod(param->second);
    ILOG(Info, Support) << "configure() : using skip shared digits 	= " << mPulseHeightThreshold << ENDM;
  } else {
    mPulseHeightThreshold = 400;
    ILOG(Info, Support) << "configure() : using default skip shared digits = " << mPulseHeightThreshold << ENDM;
  }
  if (auto param = mCustomParameters.find("chamberstoignore"); param != mCustomParameters.end()) {
    mChambersToIgnore = param->second;
    ILOG(Info, Support) << "configure() : chamberstoignore = " << mChambersToIgnore << ENDM;
  } else {
    mChambersToIgnore = "16_3_0";
    ILOG(Info, Support) << "configure() : chambers to ignore for pulseheight calculations = " << mChambersToIgnore << ENDM;
  }
  buildChamberIgnoreBP();

  retrieveCCDBSettings();
  buildHistograms();

}

void PulseHeightPerChamber::startOfActivity(Activity& /*activity*/)
{
  ILOG(Info) << "startOfActivity" << ENDM;
} // set stats/stacs

void PulseHeightPerChamber::startOfCycle()
{
  ILOG(Info) << "startOfCycle" << ENDM;
}

bool digitIndexCompare_PH(unsigned int A, unsigned int B, const std::vector<o2::trd::Digit>& originalDigits)
{
  // sort into ROC:padrow
  const o2::trd::Digit *a, *b;
  a = &originalDigits[A];
  b = &originalDigits[B];
  if (a->getDetector() < b->getDetector()) {
    return 1;
  }
  if (a->getDetector() > b->getDetector()) {
    return 0;
  }
  if (a->getPadRow() < b->getPadRow()) {
    return 1;
  }
  if (a->getPadRow() > b->getPadRow()) {
    return 0;
  }
  if (a->getPadCol() < b->getPadCol()) {
    return 1;
  }
   return 0;
   }

bool PulseHeightPerChamber::isChamberToBeIgnored(unsigned int sm, unsigned int stack, unsigned int layer)
{
  // just to make the calling method a bit cleaner
  return mChambersToIgnoreBP.test(sm * o2::trd::constants::NLAYER * o2::trd::constants::NSTACK + stack * o2::trd::constants::NLAYER + layer);
}

void PulseHeightPerChamber::monitorData(o2::framework::ProcessingContext& ctx)
{
  for (auto&& input : ctx.inputs()) {
    if (input.header != nullptr && input.payload != nullptr) {
      

      auto digits = ctx.inputs().get<gsl::span<o2::trd::Digit>>("digits");
      std::vector<o2::trd::Digit> digitv(digits.begin(), digits.end());

      if (digitv.size() == 0)
        continue;
      auto tracklets = ctx.inputs().get<gsl::span<o2::trd::Tracklet64>>("tracklets"); // still deciding if we will ever need the tracklets here.
      auto triggerrecords = ctx.inputs().get<gsl::span<o2::trd::TriggerRecord>>("triggers");
      // create a vector indexing into the digits in question
      std::vector<unsigned int> digitsIndex(digitv.size());
      std::iota(digitsIndex.begin(), digitsIndex.end(), 0);
      // we now have sorted digits, can loop sequentially and be going over det/row/pad
      for (auto& trigger : triggerrecords) {
        uint64_t numtracklets = trigger.getNumberOfTracklets();
        uint64_t numdigits = trigger.getNumberOfDigits();
        if (trigger.getNumberOfDigits() == 0)
          continue; // bail if we have no digits in this trigger
        // now sort digits to det,row,pad
	 std::sort(std::begin(digitsIndex) + trigger.getFirstDigit(), std::begin(digitsIndex) + trigger.getFirstDigit() + trigger.getNumberOfDigits(),[&digitv](unsigned int i, unsigned int j) { return digitIndexCompare_PH(i, j, digitv); });

        ///////////////////////////////////////////////////
        // Go through all chambers (using digits)
        const int adcThresh = 13;
        const int clsCutoff = 1000;
        const int startRow[5] = { 0, 16, 32, 44, 60 };
        if (trigger.getNumberOfDigits() > 10000) {
          mDigitsPerEvent->Fill(9999);
        } else {
          mDigitsPerEvent->Fill(trigger.getNumberOfDigits());
        }
        int trackletnumfix = trigger.getNumberOfTracklets();
        int digitnumfix = trigger.getNumberOfDigits();
        if (trigger.getNumberOfTracklets() > 2499)
          trackletnumfix = 2499;
        if (trigger.getNumberOfDigits() > 24999)
          trackletnumfix = 24999;
        mDigitsSizevsTrackletSize->Fill(trigger.getNumberOfTracklets(), trigger.getNumberOfDigits());
        int tbmax = 0;
        int tbhi = 0;
        int tblo = 0;

        int det = 0;
        int row = 0;
        int pad = 0;
        int channel = 0;

        for (int currentdigit = trigger.getFirstDigit() + 1; currentdigit < trigger.getFirstDigit() + trigger.getNumberOfDigits() - 1; ++currentdigit) { // -1 and +1 as we are looking for consecutive digits pre and post the current one indexed.
          if (digits[digitsIndex[currentdigit]].getChannel() > 21)
            continue;
          int detector = digits[digitsIndex[currentdigit]].getDetector();
          if (detector < 0 || detector >= 540) {
            // for some reason online the sm value causes an error below and digittrask crashes.
            // the only possibility is detector number is invalid
            ILOG(Info) << "Bad detector number from digit : " << detector << " for digit index of " << digitsIndex[currentdigit] << ENDM;
            continue;
          }
          int sm = detector / 30;
          int detLoc = detector % 30;
          int layer = detector % 6;
          int stack = detLoc / 6;
          int chamber = sm * 30 + stack * o2::trd::constants::NLAYER + layer;
          int nADChigh = 0;
          int stackoffset = stack * o2::trd::constants::NROWC1;
          int row = 0, col = 0;
          if (stack >= 2)
            stackoffset -= 4; // account for stack 2 having 4 less.
          // for now the if statement is commented as there is a problem finding isShareDigit, will come back to that.
	  //  if (!digits[digitsIndex[currentdigit]].isSharedDigit() && !mSkipSharedDigits.second) {
          // after updating the 2 above histograms the first and last digits are of no use, as we are looking for 3 neighbouring digits after this.

          auto adcs = digits[digitsIndex[currentdigit]].getADC();
          row = digits[digitsIndex[currentdigit]].getPadRow();
          pad = digits[digitsIndex[currentdigit]].getPadCol();
          int sector = detector / 30;
          // do we have 3 digits next to each other:
          std::tuple<unsigned int, unsigned int, unsigned int> aa, ba, ca;
          aa = std::make_tuple(digits[digitsIndex[currentdigit - 1]].getDetector(), digits[digitsIndex[currentdigit - 1]].getPadRow(), digits[digitsIndex[currentdigit - 1]].getPadCol());
          ba = std::make_tuple(digits[digitsIndex[currentdigit]].getDetector(), digits[digitsIndex[currentdigit]].getPadRow(), digits[digitsIndex[currentdigit]].getPadCol());
          ca = std::make_tuple(digits[digitsIndex[currentdigit + 1]].getDetector(), digits[digitsIndex[currentdigit + 1]].getPadRow(), digits[digitsIndex[currentdigit + 1]].getPadCol());
          auto [det1, row1, col1] = aa;
          auto [det2, row2, col2] = ba;
          auto [det3, row3, col3] = ca;
          // do we have 3 digits next to each other:
          // check we 3 consecutive adc
          bool consecutive = false;
          if (det1 == det2 && det2 == det3 && row1 == row2 && row2 == row3 && col1 + 1 == col2 && col2 + 1 == col3) {
            consecutive = true;
          }
          // illumination
	  //mNClsLayer[layer]->Fill(sm - 0.5 + col / 144., startRow[stack] + row);
          int digitindex = digitsIndex[currentdigit];
          int digitindexbelow = digitsIndex[currentdigit - 1];
          int digitindexabove = digitsIndex[currentdigit + 1];

          const o2::trd::Digit* mid = &digits[digitsIndex[currentdigit]];
          const o2::trd::Digit* before = &digits[digitsIndex[currentdigit - 1]];
          const o2::trd::Digit* after = &digits[digitsIndex[currentdigit + 1]];
          uint32_t suma = before->getADCsum();
          uint32_t sumb = mid->getADCsum();
          uint32_t sumc = after->getADCsum();

	    if (!isChamberToBeIgnored(sm, stack, layer)) {
	     if (consecutive) {
                    int phVal = 0;
                    for (int tb = 0; tb < 30; tb++) {
                      phVal = (mid->getADC()[tb] + before->getADC()[tb] + after->getADC()[tb]);
		      mPulseHeightpro_conc->Fill(tb, phVal);
                      //mPulseHeightPerChamber_1D[det1]->Fill(tb, phVal);
                    }
                  }
                }

         if (!isChamberToBeIgnored(sm, stack, layer)) {
	   //  if (consecutive) {
              if (sumb > suma && sumb > sumc) {
                if (suma > sumc) {
                  tbmax = sumb;
                  tbhi = suma;
                  tblo = sumc;
                  if (tblo > mPulseHeightThreshold) {
                    int phVal = 0;
                    for (int tb = 0; tb < 30; tb++) {
                      phVal = (mid->getADC()[tb] + before->getADC()[tb] + after->getADC()[tb]);
		      mPulseHeightpro_ths->Fill(tb, phVal);
                      //mPulseHeightPerChamber_1D[det1]->Fill(tb, phVal);
                    }
                  }
                }  
              } // end else
	      // }   // end if 3 pads next to each other
          }
          if (!isChamberToBeIgnored(sm, stack, layer)) {
            if (consecutive) {
              if (sumb > suma && sumb > sumc) {
                if (suma > sumc) {
                  tbmax = sumb;
                  tbhi = suma;
                  tblo = sumc;
                  if (tblo > mPulseHeightThreshold) {
                    int phVal = 0;
                    for (int tb = 0; tb < 30; tb++) {
                      phVal = (mid->getADC()[tb] + before->getADC()[tb] + after->getADC()[tb]);
		      mPulseHeightpro->Fill(tb, phVal);
                      //mPulseHeightPerChamber_1D[det1]->Fill(tb, phVal);
                    }
                  }
                } else {
                  tbmax = sumb;
                  tblo = suma;
                  tbhi = sumc;
                  if (tblo > mPulseHeightThreshold) {
                    int phVal = 0;
                    for (int tb = 0; tb < 30; tb++) {
                      phVal = (mid->getADC()[tb] + before->getADC()[tb] + after->getADC()[tb]);
		      mPulseHeightpro->Fill(tb, phVal);
                    }
                  }
                }
              } // end else
	       }   // end if 3 pads next to each other
          }     // end for c
        }
    }       // end for r
  } // end for d
  } // for loop over digits
} // loop over triggers

void PulseHeightPerChamber::endOfCycle()
{
  ILOG(Info) << "endOfCycle" << ENDM;
  /* for (Int_t det = 0; det < 540; det++) {
    mClsAmpCh->Fill(mClusterAmplitudeChamber->Integral(1, -1, det, det + 1)); //prof->GetBinContent(det));
  }
  double scale = mPulseHeight->GetEntries() / 30;
  for (int i = 0; i < 30; ++i)
    mPulseHeightScaled->SetBinContent(i, mPulseHeight->GetBinContent(i));
  mPulseHeightScaled->Scale(1 / scale);

  mClsChargeTbCycle.get()->Reset();*/
}

void PulseHeightPerChamber::endOfActivity(Activity& /*activity*/)
{
  ILOG(Info) << "endOfActivity" << ENDM;
}

void PulseHeightPerChamber::reset()
{
  // clean all the monitor objects here
  ILOG(Info) << "Resetting the histogram" << ENDM;
  mDigitsPerEvent->Reset();
  //mDigitHCID->Reset();
  //mTotalPulseHeight2D->Reset();
  //mPulseHeight->Reset();

  mDigitsPerEvent.get()->Reset();
  mPulseHeightpro.get()->Reset();
  mPulseHeightpro_conc.get()->Reset();
  mPulseHeightpro_ths.get()->Reset();
  
  //  }; // ph2DSM;
}
} // namespace o2::quality_control_modules::trd
