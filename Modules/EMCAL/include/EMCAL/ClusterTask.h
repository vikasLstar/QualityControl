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
/// \file   ClusterTask.h
/// \author Vivek Kumar Singh
///

#ifndef QC_MODULE_EMC_EMCCLUSTERTASK_H
#define QC_MODULE_EMC_EMCCLUSTERTASK_H

#include <array>
#include <iosfwd>
#include <unordered_map>
#include <string_view>

#include "QualityControl/TaskInterface.h"
#include <DataFormatsEMCAL/EventHandler.h>
#include <EMCALBase/ClusterFactory.h>
#include <EMCALReconstruction/Clusterizer.h>

class TH1;
class TH2;
class TLorentzVector;

using namespace o2::quality_control::core;

namespace o2::emcal
{
class AnalysisCluster;
class BadChannelMap;
class GainCalibrationFactors;
class Geometry;
class TimeCalibrationParams;
} // namespace o2::emcal

namespace o2::quality_control_modules::emcal
{

/// \class ClusterTask
/// \brief QC task analysing EMCAL clusters
/// \author Vivek Kumar Singh
class ClusterTask final : public TaskInterface
{
 public:
  /// \struct ClusterizerParams
  /// \brief Parameters used for clusterization
  struct ClusterizerParams {
    double mMaxTimeDeltaCells = 1000.; ///< Max. time difference between cells in cluster
    double mMinCellTime = -300.;       ///< Min. accepted cell time (in ns)
    double mMaxCellTime = 300.;        ///< Max. accepted cell time (in ns)
    double mSeedThreshold = 0.5;       ///< Min. energy of the seed cell (in GeV)
    double mCellThreshold = 0.1;       ///< Min. energy of cells attached to cluster (in GeV)
    double mGradientCut = 0.03;        ///< cut value for gradient cut (cluster splitting)
    bool mDoEnergyGradientCut = false; ///< Switch on/off gradientCut

    /// \brief Print params to output stream
    /// \param stream Stream used for printing
    void print(std::ostream& stream) const;
  };

  ///< \struct MesonClusterSelection
  ///< \brief Cluster selection for meson candidates
  struct MesonClusterSelection {
    double mMinE = 0.5;    ///< Min. Cluster E
    double mMaxTime = 25.; ///< Max cluster time relative to 0
    int mMinNCell = 2;     ///< Min. Number of cells in cluster
    bool mRejectExotics;   ///< Reject exotic clusters

    /// \brief Select cluster based on cluster cuts
    /// \param cluster Cluster to be checked
    /// \return true if the cluster is selected, false otherwise
    bool isSelected(const o2::emcal::AnalysisCluster& cluster) const;

    /// \brief Print cuts to output stream
    /// \param stream Stream used for printing
    void print(std::ostream& stream) const;
  };

  /// \struct MesonSelection
  /// \brief Cuts applied in meson candidate selection
  struct MesonSelection {
    double mMinPt = 2.; ///< Min. meson candidate pt

    /// \brief Select meson candidate based on topological and kinematic cuts
    /// \param mesonCandidate Meson candidate to be checked
    /// \return true if the meson candidate is selected, false otherwise
    bool isSelected(const TLorentzVector& mesonCandidate) const;

    /// \brief Print cuts to output stream
    /// \param stream Stream used for printing
    void print(std::ostream& stream) const;
  };

  /// \brief Constructor
  ClusterTask() = default;
  /// \brief Destructor
  ~ClusterTask() override;

  // Definition of the methods for the template method pattern
  void initialize(o2::framework::InitContext& ctx) override;
  void startOfActivity(Activity& activity) override;
  void startOfCycle() override;
  void monitorData(o2::framework::ProcessingContext& ctx) override;
  void endOfCycle() override;
  void endOfActivity(Activity& activity) override;
  void reset() override { resetHistograms(); }

 protected:
  /// \brief Reset all histograms
  void resetHistograms();

  /// \brief Fill all histograms for a given timeframe
  ///
  /// Fill all cluster based histograms using the input collections for cluster, indides and cells
  /// including the corresponding trigger records. Input collections can come from interanl or external
  /// clusterization.
  ///
  /// \param cells Input cells
  /// \param cellTriggerRecords Trigger records related to input cells
  /// \param clusters Input clusters
  /// \param clusterTriggerRecords Trigger records related to input clusters
  /// \param clusterIndices Indices of cells in event beloging to the same cluster
  /// \param cellIndexTriggerRecords Trigger records related to cell indices
  void analyseTimeframe(const gsl::span<const o2::emcal::Cell>& cells, const gsl::span<const o2::emcal::TriggerRecord>& cellTriggerRecords, const gsl::span<const o2::emcal::Cluster> clusters, const gsl::span<const o2::emcal::TriggerRecord> clusterTriggerRecords, const gsl::span<const int> clusterIndices, const gsl::span<const o2::emcal::TriggerRecord> cellIndexTriggerRecords);

  /// \brief Run internal clusterization
  ///
  /// Run internal clusterization and perform fill output collections with clusters. Settings of the clusterization
  /// are steered via the clusterizerParams.
  ///
  /// \param [in] cells Input cells for clusterization
  /// \param [in] cellTriggerRecords Trigger records of cell collection
  /// \param [out] clusters Reconstructed clusters
  /// \param [out] clusterTriggerRecords Trigger records of cluster collection
  /// \param [out] clusterIndices Indices of cells belonging to cluster
  /// \param [out] clusterIndexTriggerRecords Trigger record of cluster-cell indices
  void findClustersInternal(const gsl::span<const o2::emcal::Cell>& cells, const gsl::span<const o2::emcal::TriggerRecord>& cellTriggerRecords, std::vector<o2::emcal::Cluster>& clusters, std::vector<o2::emcal::TriggerRecord>& clusterTriggerRecords, std::vector<int>& clusterIndices, std::vector<o2::emcal::TriggerRecord>& clusterIndexTriggerRecords);

  /// \brief Build Pi0 mesons and fill histograms
  ///
  /// Function runs per event. Cluster selection is applied internally. Pi0 candidates
  /// are created per subdetector (EMCAL and DCAL). Histograms monitoring the invariant
  /// mass with respect to certain observables are filled internally.
  ///
  /// \param fullclusters List of all analysis clusters for the given event
  /// \param isEMCAL Check whether the pi0 candidates are created in the EMCAL or DCAL acceptance
  void buildAndAnalysePiOs(const gsl::span<const TLorentzVector> fullclusters, bool isEMCAL);

  /// \brief Retrieve lorentz vector for cluster
  ///
  /// Vertex position unknown in the QC, assuming the vertex to be at
  /// (0,0,0)
  ///
  /// \param fullcluster Full analysis cluster
  /// \return Lorentz vector for cluster
  TLorentzVector buildClusterVector(const o2::emcal::AnalysisCluster& fullcluster) const;

  /// \brief Perform calibration at cell level
  ///
  /// Calibrate cell energy and cell time using the CCDB objects cached in the task,
  /// and remove bad channels.
  ///
  /// \param [in] cells Uncalibrated input cells
  /// \param [in] triggerRecords Trigger records for uncalibrated cells
  /// \param [out] calibratedCells Cells per timeframe after calibration
  /// \param [out] calibratedTriggerRecords [out] Trigger records after calibration
  void getCalibratedCells(const gsl::span<const o2::emcal::Cell>& cells, const gsl::span<const o2::emcal::TriggerRecord>& triggerRecords, std::vector<o2::emcal::Cell>& calibratedCells, std::vector<o2::emcal::TriggerRecord>& calibratedTriggerRecords);

  /// \brief Configure clusterization settings for the internal clusterizer based on the task parameters
  void configureClusterizerSettings();

  /// \brief Configure meson selection (cluster and pair cuts) for meson candidate histograms
  void configureMesonSelection();

  /// \brief Check for config value in taskParameter list
  /// \param key Key to check
  /// \return true if the key is found in the taskParameters, false otherwise
  bool hasConfigValue(const std::string_view key);

  /// \brief Get a configuration value from the taskParameter list (case sensitive)
  /// \param key Key to check
  /// \return Value assinged to key if present, empty string otherwise
  std::string getConfigValue(const std::string_view key);

  /// \brief Get a configuration value from the taskParameter list (case sensitive lower case)
  /// \param key Key to check
  /// \return Value assinged to key in lower letters if present, empty string otherwise
  std::string getConfigValueLower(const std::string_view key);

 private:
  o2::emcal::Geometry* mGeometry = nullptr;                                    ///< EMCAL geometry
  std::unique_ptr<o2::emcal::EventHandler<o2::emcal::Cell>> mEventHandler;     ///< Event handler for event loop
  std::unique_ptr<o2::emcal::ClusterFactory<o2::emcal::Cell>> mClusterFactory; ///< Cluster factory for cluster kinematics
  std::unique_ptr<o2::emcal::Clusterizer<o2::emcal::Cell>> mClusterizer;       ///< Internal clusterizer
  ClusterizerParams mClusterizerSettings;                                      ///< Settings for internal clusterizer
  MesonClusterSelection mMesonClusterCuts;                                     ///< Cuts used in the meson selection
  MesonSelection mMesonCuts;                                                   ///< Cuts applied in meson selection

  ///////////////////////////////////////////////////////////////////////////////
  /// Calibration objects (for recalibration in case of interal clusterizer)  ///
  ///////////////////////////////////////////////////////////////////////////////
  o2::emcal::BadChannelMap* mBadChannelMap = nullptr;        ///< EMCAL channel map
  o2::emcal::TimeCalibrationParams* mTimeCalib = nullptr;    ///< EMCAL time calib
  o2::emcal::GainCalibrationFactors* mEnergyCalib = nullptr; ///< EMCAL energy calib factors

  bool mInternalClusterizer = false; ///< Use run internal clusterizer, do not subscribe to external cluster collection
  bool mCalibrate = false;           ///< Perform recalibration
  bool mFillInvMassMeson = false;    ///< Fill invariant mass of meson candidates

  TH1* mHistNclustPerTF = nullptr;  ///< Histogram number of clusters per timeframe
  TH1* mHistNclustPerEvt = nullptr; ///< Histogram number of clusters per event
  TH2* mHistClustEtaPhi = nullptr;  ///< Histogram cluster acceptance as function of eta and phi

  TH2* mHistTime_EMCal = nullptr;         ///< Histogram cluster time vs energy EMCAL clusters
  TH1* mHistClustE_EMCal = nullptr;       ///< Histogram cluster energy EMCAL clusters
  TH1* mHistNCells_EMCal = nullptr;       ///< Histogram number of cells per cluster for EMCAL clusters
  TH1* mHistM02_EMCal = nullptr;          ///< Histogram M02 per cluster for EMCAL clusters
  TH1* mHistM20_EMCal = nullptr;          ///< Histogram M20 per cluster for EMCAL clusters
  TH2* mHistM02VsClustE__EMCal = nullptr; ///< Histogram M02 vs. cluster energy for EMCAL clusters
  TH2* mHistM20VsClustE__EMCal = nullptr; ///< Histogram M20 vs. cluster energy for EMCAL clusters

  TH2* mHistTime_DCal = nullptr;         ///< Histogram cluster time vs energy DCAL clusters
  TH1* mHistClustE_DCal = nullptr;       ///< Histogram cluster energy DCAL clusters
  TH1* mHistNCells_DCal = nullptr;       ///< Histogram number of cells per cluster for DCAL clusters
  TH1* mHistM02_DCal = nullptr;          ///< Histogram M02 per cluster for DCAL clusters
  TH1* mHistM20_DCal = nullptr;          ///< Histogram M20 per cluster for DCAL clusters
  TH2* mHistM02VsClustE__DCal = nullptr; ///< Histogram M02 vs. cluster energy for DCAL clusters
  TH2* mHistM20VsClustE__DCal = nullptr; ///< Histogram M20 vs. cluster energy for DCAL clusters

  TH1* mHistMassDiphoton_EMCAL = nullptr;   ///< Histogram diphoton mass integrated for meson candidates in EMCAL
  TH1* mHistMassDiphoton_DCAL = nullptr;    ///< Histogram diphoton mass integrated for meson candidates in DCAL
  TH2* mHistMassDiphotonPt_EMCAL = nullptr; ///< Histogram diphoton mass integrated vs. pt for meson candidates in EMCAL
  TH2* mHistMassDiphotonPt_DCAL = nullptr;  ///< Histogram diphoton mass integrated vs. pt for meson candidates in DCAL
};

/// \brief Output stream operator for clusterizer parameters in the ClusterTask
/// \param stream Stream used for printing the clusterizer param object
/// \param params ClusterizerParams to be printed
/// \return Stream after printing the params
std::ostream& operator<<(std::ostream& stream, const ClusterTask::ClusterizerParams& params);

/// \brief Output stream operator for meson cluster selection cuts in the ClusterTask
/// \param stream Stream used for printing the meson cluster selection cuts object
/// \param cuts Meson cluster cuts to be printed
/// \return Stream after printing the cuts
std::ostream& operator<<(std::ostream& stream, const ClusterTask::MesonClusterSelection& cuts);

/// \brief Output stream operator for meson candidate selection cuts in the ClusterTask
/// \param stream Stream used for printing the meson candidate selecton cuts object
/// \param cuts Meson candidate cuts to be printed
/// \return Stream after printing the cuts
std::ostream& operator<<(std::ostream& stream, const ClusterTask::MesonSelection& cuts);

} // namespace o2::quality_control_modules::emcal

#endif // QC_MODULE_EMC_EMCCLUSTERTASK_H
