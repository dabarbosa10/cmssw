#include <iostream>
#include <cmath>

#include "SimTracker/SiPhase2Digitizer/plugins/PSPDigitizerAlgorithm.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

// Geometry
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "Geometry/CommonDetUnit/interface/PixelGeomDetUnit.h"

using namespace edm;

void PSPDigitizerAlgorithm::init(const edm::EventSetup& es) { es.get<TrackerDigiGeometryRecord>().get(geom_); }
PSPDigitizerAlgorithm::PSPDigitizerAlgorithm(const edm::ParameterSet& conf)
    : Phase2TrackerDigitizerAlgorithm(conf.getParameter<ParameterSet>("AlgorithmCommon"),
                                      conf.getParameter<ParameterSet>("PSPDigitizerAlgorithm")) {
  pixelFlag_ = false;
  LogDebug("PSPDigitizerAlgorithm") << "Algorithm constructed "
                                    << "Configuration parameters:"
                                    << "Threshold/Gain = "
                                    << "threshold in electron Endcap = " << theThresholdInE_Endcap_
                                    << "threshold in electron Barrel = " << theThresholdInE_Barrel_ << " "
                                    << theElectronPerADC_ << " " << theAdcFullScale_ << " The delta cut-off is set to "
                                    << tMax_ << " pix-inefficiency " << addPixelInefficiency_;
}
PSPDigitizerAlgorithm::~PSPDigitizerAlgorithm() { LogDebug("PSPDigitizerAlgorithm") << "Algorithm deleted"; }
void PSPDigitizerAlgorithm::accumulateSimHits(std::vector<PSimHit>::const_iterator inputBegin,
                                              std::vector<PSimHit>::const_iterator inputEnd,
                                              const size_t inputBeginGlobalIndex,
                                              const uint32_t tofBin,
                                              const Phase2TrackerGeomDetUnit* pixdet,
                                              const GlobalVector& bfield) {
  // produce SignalPoint's for all SimHit's in detector
  // Loop over hits
  uint32_t detId = pixdet->geographicalId().rawId();
  size_t simHitGlobalIndex = inputBeginGlobalIndex;  // This needs to be stored to create the digi-sim link later

  // find the relevant hits
  std::vector<PSimHit> matchedSimHits;
  std::copy_if(inputBegin, inputEnd, std::back_inserter(matchedSimHits), [detId](auto const& hit) -> bool {
    return hit.detUnitId() == detId;
  });
  // loop over a much reduced set of SimHits
  for (auto const& hit : matchedSimHits) {
    LogDebug("PSPDigitizerAlgorithm") << hit.particleType() << " " << hit.pabs() << " " << hit.energyLoss() << " "
                                      << hit.tof() << " " << hit.trackId() << " " << hit.processType() << " "
                                      << hit.detUnitId() << hit.entryPoint() << " " << hit.exitPoint();

    std::vector<DigitizerUtility::EnergyDepositUnit> ionization_points;
    std::vector<DigitizerUtility::SignalPoint> collection_points;

    double signalScale = 1.0;
    // fill collection_points for this SimHit, indpendent of topology
    if (select_hit(hit, (pixdet->surface().toGlobal(hit.localPosition()).mag() * c_inv), signalScale)) {
      primary_ionization(hit, ionization_points);  // fills ionization_points

      // transforms ionization_points -> collection_points
      drift(hit, pixdet, bfield, ionization_points, collection_points);

      // compute induced signal on readout elements and add to _signal
      // hit needed only for SimHit<-->Digi link
      induce_signal(hit, simHitGlobalIndex, tofBin, pixdet, collection_points);
    }
    ++simHitGlobalIndex;
  }
}
//
// -- Select the Hit for Digitization (sigScale will be implemented in future)
//
bool PSPDigitizerAlgorithm::select_hit(const PSimHit& hit, double tCorr, double& sigScale) {
  double toa = hit.tof() - tCorr;
  return (toa > theTofLowerCut_ && toa < theTofUpperCut_);
}
//
// -- Compare Signal with Threshold
//
bool PSPDigitizerAlgorithm::isAboveThreshold(const DigitizerUtility::SimHitInfo* hitInfo, float charge, float thr) {
  return (charge >= thr);
}
