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
/// \file   PulseHeightPerChamber.h
/// \author My Name
///

#ifndef QC_MODULE_TRD_TRDPULSEHEIGHTPERCHAMBER_H
#define QC_MODULE_TRD_TRDPULSEHEIGHTPERCHAMBER_H

#include "QualityControl/TaskInterface.h"
#include <array>
#include "DataFormatsTRD/NoiseCalibration.h"
#include "DataFormatsTRD/Digit.h"
#include "TRDQC/StatusHelper.h"

class TH1F;
class TH2F;
class TH1D;
class TH2D;
class TLine;
class TProfile;
class TProfile2D;

using namespace o2::quality_control::core;

namespace o2::quality_control_modules::trd
{

/// \brief Example Quality Control DPL Task
/// \author My Name
class PulseHeightPerChamber final : public TaskInterface
{
 public:
  /// \brief Constructor
  PulseHeightPerChamber() = default;
  /// Destructor
  ~PulseHeightPerChamber() override;

  // Definition of the methods for the template method pattern
  void initialize(o2::framework::InitContext& ctx) override;
  void startOfActivity(Activity& activity) override;
  void startOfCycle() override;
  void monitorData(o2::framework::ProcessingContext& ctx) override;
  void endOfCycle() override;
  void endOfActivity(Activity& activity) override;
  void reset() override;
  void buildHistograms();
  void drawLinesMCM(TH2F* histo);
  void drawTrdLayersGrid(TH2F* hist);
  void retrieveCCDBSettings();
  void drawLinesOnPulseHeight(TH1F* h);
  void fillLinesOnHistsPerLayer(int iLayer);
  void drawHashOnLayers(int layer, int hcid, int col, int rowstart, int rowend);
  void buildChamberIgnoreBP();
  bool isChamberToBeIgnored(unsigned int sm, unsigned int stack, unsigned int layer);
  // bool digitIndexCompare(unsigned int A, unsigned int B, const std::vector<o2::trd::Digit>& originalDigits);
 private:
  //limits
  bool mSkipSharedDigits;
  unsigned int mPulseHeightThreshold;
  std::pair<float, float> mDriftRegion;
  std::pair<float, float> mPulseHeightPeakRegion;
  long int mTimestamp;

  std::shared_ptr<TH1F> mDigitsPerEvent;
  std::shared_ptr<TH2F> mDigitsSizevsTrackletSize;
  std::shared_ptr<TProfile> mPulseHeightpro = nullptr;
  std::shared_ptr<TProfile> mPulseHeightpro_conc = nullptr;
  std::shared_ptr<TProfile> mPulseHeightpro_ths = nullptr;
  std::shared_ptr<TH1F> mPulseHeight = nullptr;

// information pulled from ccdb
  o2::trd::NoiseStatusMCM* mNoiseMap = nullptr;
  o2::trd::HalfChamberStatusQC* mChamberStatus = nullptr;
  std::string mChambersToIgnore;
  std::bitset<o2::trd::constants::MAXCHAMBER> mChambersToIgnoreBP;
};

} // namespace o2::quality_control_modules::trd

#endif // QC_MODULE_TRD_TRDPULSEHEIGHTPERCHAMBER_H
