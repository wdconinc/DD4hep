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
//  Simple program to make a "CT-scan" difference between two detector geometries.
//  Produces TH2F histograms of (A - B) or log(A/B) for X0 and lambda per slice.
//  Only material scanning is supported (field diff is not meaningful).
//
//  Author     : W. Deconinck
//
//==========================================================================

#include <TError.h>
#include <TFile.h>
#include <TH2F.h>

// Framework include files
#include <DD4hep/Detector.h>
#include <DD4hep/Printout.h>
#include <DDRec/MaterialManager.h>

#include <iostream>
#include <climits>
#include <cerrno>
#include <cmath>
#include <string>
#include <memory>

#undef NDEBUG
#include <cassert>

using namespace dd4hep;
using namespace dd4hep::rec;

using std::cout;
using std::endl;

/// Compute summed X0/length and lambda/length for a single scan between p0 and p1.
static void accumulate(MaterialManager& mgr, const Vector3D& p0, const Vector3D& p1,
                       double& sum_x0, double& sum_lambda, double& sum_length,
                       bool& found_tessellated)
{
  const MaterialManager::ScanData sd = mgr.entriesBetween(p0, p1);
  const auto& materials = sd.materials;
  const auto& places    = sd.places;
  for ( unsigned i = 0, n = materials.size(); i < n; ++i ) {
    TGeoMaterial* mat = materials[i].first->GetMaterial();
    double length = materials[i].second;
    sum_length += length;
    sum_x0     += length / mat->GetRadLen();
    sum_lambda += length / mat->GetIntLen();
  }
  if ( !found_tessellated ) {
    for ( const auto& p : places ) {
      Volume volume(p.first.volume());
      Solid  shape(volume.solid());
      if ( shape->IsA() == TGeoTessellated::Class() ) {
        found_tessellated = true;
      }
    }
  }
}

int main_wrapper(int argc, char** argv)   {
  struct Handler  {
    Handler() { SetErrorHandler(Handler::print); }
    static void print(int level, Bool_t abort, const char *location, const char *msg)  {
      if ( level > kInfo || abort ) ::printf("%s: %s\n", location, msg);
    }
    static void usage()  {
      std::cout << " usage: graphicalScanDiff compact_A.xml compact_B.xml axis xMin xMax yMin yMax zMin zMax nSlices nBins nSamples M OutfileName [--abs|--log]" << std::endl
                << " compact_A.xml, compact_B.xml  : two geometry descriptions to compare" << std::endl
                << " axis (X, Y, or Z)             : perpendicular to the slices" << std::endl
                << " xMin xMax yMin yMax zMin zMax : range of scans " << std::endl
                << " nSlices                       : number of slices (equally spaced along chosen axis)" << std::endl
                << " nBins                         : number of bins along each axis of histograms" << std::endl
                << " nSamples                      : the number of times each bin is sampled" << std::endl
                << " M                             : only material scanning is supported for diff mode" << std::endl
                << " OutfileName                   : output root file name" << std::endl
                << " --abs                         : report absolute difference A - B (default)" << std::endl
                << " --log                         : report logarithmic ratio log(A / B)" << std::endl
                << "        -> produces graphical diff scans of material defined in two compact xml descriptions"
                << std::endl;
      exit(1);
    }
  } _handler;

  if ( argc < 15 || argc > 16 ) Handler::usage();

  std::string inFileA = argv[1];
  std::string inFileB = argv[2];
  std::string XYZ     = argv[3];

  TString labx, laby;
  unsigned int index[3] = {99,99,99};
  if ( XYZ=="x" || XYZ=="X" ) {
    index[0]=0; index[1]=2; index[2]=1;
    labx="Z [cm]"; laby="Y [cm]";
  } else if ( XYZ=="y" || XYZ=="Y" ) {
    index[0]=1; index[1]=2; index[2]=0;
    labx="Z [cm]"; laby="X [cm]";
  } else if ( XYZ=="z" || XYZ=="Z" ) {
    index[0]=2; index[1]=0; index[2]=1;
    labx="X [cm]"; laby="Y [cm]";
  } else {
    cout << "invalid XYZ" << endl;
    return -1;
  }

  double x0,y0,z0,x1,y1,z1;
  unsigned int nslice, nbins, mm_count;
  {
    std::stringstream sstr;
    sstr << argv[4] << " " << argv[5] << " " << argv[6] << " "
         << argv[7] << " " << argv[8] << " " << argv[9] << " "
         << argv[10] << " " << argv[11] << " " << argv[12] << "NONE";
    sstr >> x0 >> x1 >> y0 >> y1 >> z0 >> z1 >> nslice >> nbins >> mm_count;
    if ( !sstr.good() ) { Handler::usage(); ::exit(EINVAL); }
  }

  std::string FM = argv[13];
  if ( FM != "m" && FM != "M" ) {
    cout << "graphicalScanDiff only supports material scanning (M). Field diff is not supported." << endl;
    return 1;
  }

  std::string outFileName = argv[14];

  bool log_mode = false;
  if ( argc == 16 ) {
    std::string flag = argv[15];
    if ( flag == "--log" ) {
      log_mode = true;
    } else if ( flag == "--abs" ) {
      log_mode = false;
    } else {
      Handler::usage();
    }
  }

  if ( x0>x1 ) { double t=x0; x0=x1; x1=t; }
  if ( y0>y1 ) { double t=y0; y0=y1; y1=t; }
  if ( z0>z1 ) { double t=z0; z0=z1; z1=t; }

  if ( !(nbins>0 && nbins<USHRT_MAX && nslice>0 && nslice<USHRT_MAX) ) {
    cout << "funny # bins/slices" << endl;
    ::exit(EINVAL);
  }

  setPrintLevel(WARNING);

  std::unique_ptr<Detector> descA = Detector::make_unique("A");
  descA->fromCompact(inFileA);
  std::unique_ptr<Detector> descB = Detector::make_unique("B");
  descB->fromCompact(inFileB);

  MaterialManager matMgrA( descA->world().volume() );
  MaterialManager matMgrB( descB->world().volume() );

  double mmin[3]={x0,y0,z0};
  double mmax[3]={x1,y1,z1};

  const char* diff_label = log_mode ? "log(A/B)" : "A - B";

  bool found_tessellated = false;
  TFile* f = new TFile(outFileName.c_str(),"recreate");
  Vector3D p0, p1;

  for (unsigned int isl=0; isl<nslice; isl++) {

    double sz = nslice > 1 ?
      mmin[index[0]] + isl*( mmax[index[0]] - mmin[index[0]] )/( nslice - 1 ) :
      (mmin[index[0]] + mmax[index[0]])/2. ;

    p0.array()[ index[0] ] = sz;
    p1.array()[ index[0] ] = sz;

    cout << "scanning slice " << isl << " at "+XYZ+" = " << sz << endl;

    TString dirn = "Slice"; dirn+=isl;
    f->mkdir(dirn);
    f->cd(dirn);

    TString hn, hnn;

    hn = "slice"; hn+=isl; hn+="_X0_diff";
    hnn = "X0 diff ("; hnn += diff_label; hnn += ") "; hnn += XYZ; hnn+="="; hnn += Form("%7.3f",sz); hnn+=" [cm]";
    TH2F* h2_x0 = new TH2F( hn, hnn, nbins, mmin[index[1]], mmax[index[1]], nbins, mmin[index[2]], mmax[index[2]] );

    hn = "slice"; hn+=isl; hn+="_lambda_diff";
    hnn = "lambda diff ("; hnn += diff_label; hnn += ") "; hnn += XYZ; hnn+="="; hnn += Form("%7.3f",sz); hnn+=" [cm]";
    TH2F* h2_lambda = new TH2F( hn, hnn, nbins, mmin[index[1]], mmax[index[1]], nbins, mmin[index[2]], mmax[index[2]] );

    for (int ix=1; ix<=h2_x0->GetNbinsX(); ix++) {
      double xmin = h2_x0->GetXaxis()->GetBinLowEdge(ix);
      double xmax = h2_x0->GetXaxis()->GetBinUpEdge(ix);

      for (int iy=1; iy<=h2_x0->GetNbinsY(); iy++) {
        double ymin = h2_x0->GetYaxis()->GetBinLowEdge(iy);
        double ymax = h2_x0->GetYaxis()->GetBinUpEdge(iy);

        double sum_x0_A(0), sum_lambda_A(0), sum_length_A(0);
        double sum_x0_B(0), sum_lambda_B(0), sum_length_B(0);

        for (unsigned int jx=0; jx<2*mm_count; jx++) {
          if ( jx < mm_count ) {
            double xcom = xmin + (1+jx)*( xmax - xmin )/(mm_count+1.);
            p0.array()[index[1]] = xcom;  p0.array()[index[2]] = ymin;
            p1.array()[index[1]] = xcom;  p1.array()[index[2]] = ymax;
          } else {
            double ycom = ymin + (jx-mm_count+1)*( ymax - ymin )/(mm_count+1.);
            p0.array()[index[1]] = xmin;  p0.array()[index[2]] = ycom;
            p1.array()[index[1]] = xmax;  p1.array()[index[2]] = ycom;
          }
          accumulate(matMgrA, p0, p1, sum_x0_A, sum_lambda_A, sum_length_A, found_tessellated);
          accumulate(matMgrB, p0, p1, sum_x0_B, sum_lambda_B, sum_length_B, found_tessellated);
        }

        double val_x0_A     = (sum_length_A > 0) ? sum_x0_A     / sum_length_A : 0.0;
        double val_lambda_A = (sum_length_A > 0) ? sum_lambda_A / sum_length_A : 0.0;
        double val_x0_B     = (sum_length_B > 0) ? sum_x0_B     / sum_length_B : 0.0;
        double val_lambda_B = (sum_length_B > 0) ? sum_lambda_B / sum_length_B : 0.0;

        double diff_x0, diff_lambda;
        if ( log_mode ) {
          diff_x0     = (val_x0_B     > 0 && val_x0_A     > 0) ? std::log(val_x0_A     / val_x0_B)     : 0.0;
          diff_lambda = (val_lambda_B > 0 && val_lambda_A > 0) ? std::log(val_lambda_A / val_lambda_B) : 0.0;
        } else {
          diff_x0     = val_x0_A     - val_x0_B;
          diff_lambda = val_lambda_A - val_lambda_B;
        }

        h2_x0->SetBinContent(ix, iy, diff_x0);
        h2_lambda->SetBinContent(ix, iy, diff_lambda);
      }
    }

    h2_x0->SetOption("zcol");
    h2_x0->GetXaxis()->SetTitle(labx);
    h2_x0->GetYaxis()->SetTitle(laby);
    h2_lambda->SetOption("zcol");
    h2_lambda->GetXaxis()->SetTitle(labx);
    h2_lambda->GetYaxis()->SetTitle(laby);
  }

  if ( found_tessellated )  {
    const char* line = " +------------------------------------------------------------"
      "--------------------------------------------------------------------------------------\n";
    ::printf("%s",line);
    ::printf(" |  WARNING: Tessellated shape were encountered during the volume traversal.\n");
    ::printf(" |  WARNING: The results of the material scan(s) are unreliable!\n");
    ::printf("%s",line);
  }
  f->Write();
  f->Close();
  return 0;
}

/// Main entry point as a program
int main(int argc, char** argv)   {
  try  {
    return main_wrapper(argc, argv);
  }
  catch(const std::exception& e)  {
    std::cout << "Got uncaught exception: " << e.what() << std::endl;
  }
  catch (...)  {
    std::cout << "Got UNKNOWN uncaught exception." << std::endl;
  }
  return EINVAL;
}
