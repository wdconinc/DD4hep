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

/** \addtogroup Geant4Action
 *
 @{
   \package Geant4FullTruthParticleHandler
 * \brief Keeps all generated particles for full truth record.
 *
 *
@}
 */

#ifndef DD4HEP_DDG4_GEANT4FULLTRUTHPARTICLEHANDLER_H
#define DD4HEP_DDG4_GEANT4FULLTRUTHPARTICLEHANDLER_H

// Framework include files
#include <DD4hep/Primitives.h>
#include <DDG4/Geant4UserParticleHandler.h>

/// Namespace for the AIDA detector description toolkit
namespace dd4hep {

  /// Namespace for the Geant4 based simulation part of the AIDA detector description toolkit
  namespace sim {

    /// Keeps particles based on a custom truth logic flow.
    /** Geant4FullTruthParticleHandler
     *
     *  Implements the logic defined in the user-provided flowchart
     *  to decide which particles to keep in the final MC record.
     *
     * @author  Daniel Murnane
     * @version 1.0
     */
    class Geant4FullTruthParticleHandler : public Geant4UserParticleHandler  {
    private:
      // Tracking volume definition (for determining tracker vs. calorimeter)
      double m_zTrackerMin { -1e100 };
      double m_zTrackerMax {  1e100 };
      double m_rTracker    {  1e100 };
      // Energy threshold for calorimeter-related decisions
      // double m_energyThreshold { 0.0 }; // Removed - Use global G4PARTICLE_ABOVE_ENERGY_THRESHOLD flag

      // Helper methods
      bool isInTracker(double r, double z) const;

    public:
      /// Standard constructor
      Geant4FullTruthParticleHandler(Geant4Context* context, const std::string& nam);

      /// Default destructor
      virtual ~Geant4FullTruthParticleHandler() {}

      /// Post-track action callback
      /** Allow the user to force the particle handling in the post track action
       *  set the reason mask to NULL in order to drop the particle.
       *  The parent's reasoning mask will be or'ed with the particle's mask
       *  to preserve the MC truth for the hit creation.
       *
       *  Note: The particle passed is a temporary and will be copied if kept.
       */
      virtual void end(const G4Track* track, Particle& particle) override;

      /// Post-event action callback: avoid warning (...) was hidden [-Woverloaded-virtual]
      virtual void end(const G4Event* event) override;

    };
  }    // End namespace sim
}      // End namespace dd4hep

#endif // DD4HEP_DDG4_GEANT4FULLTRUTHPARTICLEHANDLER_H 