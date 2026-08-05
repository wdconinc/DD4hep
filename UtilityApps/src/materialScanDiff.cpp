//==========================================================================
//  AIDA Detector description implementation 
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
//==========================================================================
//
//  Simple program to print the difference in materials between two detectors
//  on a straight line between two given points
//
//  Author     : W. Deconinck
//
//==========================================================================

#include <TError.h>

// Framework include files
#include <DD4hep/Detector.h>
#include <DD4hep/Printout.h>
#include <DD4hep/DD4hepUnits.h>
#include <DDRec/MaterialScan.h>
#include "main.h"

#include <cmath>
#include <iostream>

using namespace dd4hep;
using namespace dd4hep::rec;

int main_wrapper(int argc, char** argv)   {
  struct Handler  {
    Handler() { SetErrorHandler(Handler::print); }
    static void print(int level, Bool_t abort, const char *location, const char *msg)  {
      if ( level > kInfo || abort ) ::printf("%s: %s\n", location, msg);
    }
    static void usage()  {
      std::cout << " usage: materialScanDiff compact_A.xml compact_B.xml x0 y0 z0 x1 y1 z1 [--abs|--log]" << std::endl
                << "        -> prints the difference (A - B) in materials on a straight line between the two given points" << std::endl
                << "        --abs   report absolute difference A - B (default)" << std::endl
                << "        --log   report logarithmic ratio log(A / B)" << std::endl
                << " NOTE:  ALL lengths in units of [cm]"
                << std::endl;
      exit(EINVAL);
    }
  } _handler;

  if ( argc < 9 ) Handler::usage();

  bool log_mode = false;
  if ( argc >= 10 ) {
    std::string flag = argv[9];
    if ( flag == "--log" ) {
      log_mode = true;
    } else if ( flag == "--abs" ) {
      log_mode = false;
    } else {
      Handler::usage();
    }
  }

  std::string inFileA = argv[1];
  std::string inFileB = argv[2];

  double x0, y0, z0, x1, y1, z1;
  {
    std::stringstream sstr;
    sstr << argv[3] << " " << argv[4] << " " << argv[5] << " "
         << argv[6] << " " << argv[7] << " " << argv[8] << " " << "NONE";
    sstr >> x0 >> y0 >> z0 >> x1 >> y1 >> z1;
    if ( !sstr.good() ) Handler::usage();
  }

  setPrintLevel(WARNING);

  // Compute material totals for geometry A
  double sum_x0_A = 0, sum_lambda_A = 0;
  {
    std::unique_ptr<Detector> descA = Detector::make_unique("A");
    descA->fromXML(inFileA);
    MaterialScan scanA(*descA);
    const MaterialVec& matsA = scanA.scan(x0*dd4hep::cm, y0*dd4hep::cm, z0*dd4hep::cm,
                                          x1*dd4hep::cm, y1*dd4hep::cm, z1*dd4hep::cm);
    for ( const auto& entry : matsA ) {
      TGeoMaterial* mat = entry.first->GetMaterial();
      double length = entry.second;
      sum_x0_A     += length / mat->GetRadLen();
      sum_lambda_A += length / mat->GetIntLen();
    }
  }

  // Compute material totals for geometry B
  double sum_x0_B = 0, sum_lambda_B = 0;
  {
    std::unique_ptr<Detector> descB = Detector::make_unique("B");
    descB->fromXML(inFileB);
    MaterialScan scanB(*descB);
    const MaterialVec& matsB = scanB.scan(x0*dd4hep::cm, y0*dd4hep::cm, z0*dd4hep::cm,
                                          x1*dd4hep::cm, y1*dd4hep::cm, z1*dd4hep::cm);
    for ( const auto& entry : matsB ) {
      TGeoMaterial* mat = entry.first->GetMaterial();
      double length = entry.second;
      sum_x0_B     += length / mat->GetRadLen();
      sum_lambda_B += length / mat->GetIntLen();
    }
  }

  // Compute and report the difference
  double diff_x0, diff_lambda;
  const char* mode_label;
  if ( log_mode ) {
    mode_label = "log(A/B)";
    diff_x0     = (sum_x0_B     > 0) ? std::log(sum_x0_A     / sum_x0_B)     : 0.0;
    diff_lambda = (sum_lambda_B > 0) ? std::log(sum_lambda_A / sum_lambda_B) : 0.0;
  } else {
    mode_label = "A - B";
    diff_x0     = sum_x0_A     - sum_x0_B;
    diff_lambda = sum_lambda_A - sum_lambda_B;
  }

  const char* line = "+----------------------------------+------------------+------------------+------------------+\n";
  ::printf("\n");
  ::printf("%s", line);
  ::printf("| %-32s | %16s | %16s | %16s |\n", "Quantity", "Geometry A", "Geometry B", mode_label);
  ::printf("%s", line);
  ::printf("| %-32s | %16.6f | %16.6f | %16.6f |\n", "Total X0 [rad. lengths]",    sum_x0_A,     sum_x0_B,     diff_x0);
  ::printf("| %-32s | %16.6f | %16.6f | %16.6f |\n", "Total Lambda [int. lengths]", sum_lambda_A, sum_lambda_B, diff_lambda);
  ::printf("%s", line);
  ::printf("\n");

  return 0;
}
