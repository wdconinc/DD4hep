// $Id:$
//====================================================================
//  AIDA Detector description implementation for LCD
//--------------------------------------------------------------------
//
//  Author     : M.Frank
//
//====================================================================
#include "DD4hep/DetFactoryHelper.h"

using namespace std;
using namespace DD4hep;
using namespace DD4hep::Geometry;

static Ref_t create_element(LCDD& lcdd, xml_h e, Ref_t)  {
  xml_det_t   x_det = e;
  string      name  = x_det.nameStr();
  xml_comp_t  box    (x_det.child(_U(box)));
  xml_dim_t   pos    (x_det.child(_U(position)));
  xml_dim_t   rot    (x_det.child(_U(rotation)));
  Material    mat    (lcdd.material(x_det.materialStr()));
  DetElement  det    (name,x_det.id());
  Volume      det_vol(name+"_vol",Box(box.x(),box.y(),box.z()), mat);
  Volume      mother = lcdd.pickMotherVolume(det);

  det_vol.setVisAttributes(lcdd, x_det.visStr());
  PlacedVolume phv = mother.placeVolume(det_vol,Position(pos.x(),pos.y(),pos.z()),
					Rotation(rot.z(),rot.y(),rot.x()));
  phv.addPhysVolID("id",x_det.id());
  det.setPlacement(phv);
  return det;
}

// first argument is the type from the xml file
DECLARE_DETELEMENT(BoxSegment,create_element)
