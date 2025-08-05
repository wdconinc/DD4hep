//==========================================================================
//  AIDA Detector description implementation
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
// Author     : Daniel Murnane
//
//==========================================================================

// Framework include files
#include "Geant4FullTruthParticleHandler.h"
#include <DDG4/Geant4Particle.h>
#include <DDG4/Factories.h>
#include "Geant4UserParticleHandlerHelper.h" // Include helper for setSimulatorStatus if needed

// Geant4 include files
#include <G4Track.hh>
#include <G4Event.hh>

// C++ includes
#include <cmath>
#include <DD4hep/Primitives.h> // For PropertyMask

using namespace dd4hep::sim;
using PropertyMask = dd4hep::detail::ReferenceBitMask<int>; // Add this for easier use

DECLARE_GEANT4ACTION(Geant4FullTruthParticleHandler)

/// Standard constructor
Geant4FullTruthParticleHandler::Geant4FullTruthParticleHandler(Geant4Context* ctxt, const std::string& nam)
: Geant4UserParticleHandler(ctxt,nam)
{
  // Declare properties for configuration from Python
  declareProperty("TrackingVolume_Zmin", m_zTrackerMin);
  declareProperty("TrackingVolume_Zmax", m_zTrackerMax);
  declareProperty("TrackingVolume_Rmax", m_rTracker);
}

/// Post-track action callback
void Geant4FullTruthParticleHandler::end(const G4Track* track, Particle& p)
{
  PropertyMask reasonMask(p.reason);
  PropertyMask statusMask(p.status);

  // If it's primary, or already marked to keep always, do nothing here.
  // The main handler's logic will ensure it's kept.
  if (reasonMask.isSet(G4PARTICLE_PRIMARY) || reasonMask.isSet(G4PARTICLE_KEEP_ALWAYS)) {
    return; 
  }

  // 1. Determine particle origin and end points relative to tracker volume
  double r_prod = std::sqrt(p.vsx*p.vsx + p.vsy*p.vsy);
  double z_prod = p.vsz;
  bool created_in_tracker = isInTracker(r_prod, z_prod);

  double r_end = std::sqrt(p.vex*p.vex + p.vey*p.vey);
  double z_end = p.vez;
  bool ended_in_tracker = isInTracker(r_end, z_end);
  // Ended in calo means ended outside tracker but not left detector
  bool ended_in_calo = !ended_in_tracker && !statusMask.isSet(G4PARTICLE_SIM_LEFT_DETECTOR);

  // 2. Check hit creation flags (these are set by Sensitive Detectors via Geant4ParticleHandler::mark)
  bool left_hit_in_tracker = reasonMask.isSet(G4PARTICLE_CREATED_TRACKER_HIT);
  bool left_hit_in_calo = reasonMask.isSet(G4PARTICLE_CREATED_CALORIMETER_HIT);

  // 3. Check energy threshold
  bool is_above_global_threshold = reasonMask.isSet(G4PARTICLE_ABOVE_ENERGY_THRESHOLD);

  // 4. Apply truth selection logic - determine if particle should be kept
  bool should_keep = false;
  
  if (created_in_tracker) {
    // Particles born in tracker: keep if they left any hit OR are above energy threshold
    should_keep = (left_hit_in_tracker || left_hit_in_calo || is_above_global_threshold);
    
  } else if (ended_in_calo) {
    // Particles born outside, ended in calo: keep if above threshold AND left calo hit, OR left tracker hit
    should_keep = ((is_above_global_threshold && left_hit_in_calo) || left_hit_in_tracker);
    
  } else {
    // Particles born outside, ended in tracker/outside detector (backscatter): keep if left tracker hit
    should_keep = left_hit_in_tracker;
  }
  
  // Discard particle if it doesn't meet keeping criteria
  if (!should_keep) {
    p.reason = 0;
  }

  // 5. Set standard simulator status bits (using the helper function is good practice)
  setSimulatorStatus(p, created_in_tracker, ended_in_tracker);
}

/// Helper to check if a point is in the tracker region (cylindrical)
bool Geant4FullTruthParticleHandler::isInTracker(double r, double z) const {
  // Handle possibility of symmetric Z definition using only Zmax
  double zMin = (m_zTrackerMin == -1e100) ? -m_zTrackerMax : m_zTrackerMin;
  return (r <= m_rTracker && z >= zMin && z <= m_zTrackerMax);
}

/// Post-event action callback
void Geant4FullTruthParticleHandler::end(const G4Event* /* event */)
{
  // Usually empty for particle handlers, but override is required.
} 