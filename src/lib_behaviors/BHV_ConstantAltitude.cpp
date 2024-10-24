/*****************************************************************/
/*    NAME: Michael Benjamin, Modified by Steve Carter Feujo Nomeny          */
/*    ORGN: Dept of Mechanical Engineering, MIT, Cambridge MA    */
/*    FILE: BHV_ConstantAltitude.cpp                             */
/*****************************************************************/

// #ifdef _WIN32
// #pragma warning(disable : 4786)
// #pragma warning(disable : 4503)
// #endif

#include <iostream>
#include <cmath> 
#include <cstdlib>
#include "BHV_ConstantAltitude.h"
#include "BuildUtils.h"
#include "MBUtils.h"
#include "ZAIC_PEAK.h"

using namespace std;

//-----------------------------------------------------------
// Procedure: Constructor

BHV_ConstantAltitude::BHV_ConstantAltitude(IvPDomain gdomain) : 
  IvPBehavior(gdomain)
{
  this->setParam("descriptor", "bhv for constant altitude hold");


  addInfoVars("NAV_ALTITUDE");

  m_domain = subDomain(m_domain, "altitude");

  m_desired_altitude = 0;

  // Default values changed by HS 110530
  m_peakwidth     = 3;
  m_basewidth     = 100;
  m_summitdelta   = 50;
  m_osd           = 0;

  // The default duration at the IvPBehavior level is "-1", which
  // indicates no duration applied to the behavior by default. By
  // setting to zero here, we force the user to provide a duration
  // value otherwise it will timeout immediately.
  m_duration      = 0;
}

//-----------------------------------------------------------
// Procedure: setParam

bool BHV_ConstantAltitude::setParam(string param, string val) 
{
  if(IvPBehavior::setParam(param, val))
    return(true);

  double dval = atof(val.c_str());

  if((param == "altitude") && isNumber(val)) {
    m_desired_altitude = vclip_min(dval, 0);
    return(true);
  }

  else if((param == "peakwidth") && isNumber(val)) {
    m_peakwidth = vclip_min(dval, 0);
    return(true);
  }
  
  else if((param == "summitdelta") && isNumber(val)) {
    m_summitdelta = vclip(dval, 0, 100);
    return(true);
  }
  
  else if((param == "basewidth") && isNumber(val)) {
    m_basewidth = vclip_min(dval, 0);
    return(true);
  }

  else if((param == "altitude_mismatch_var") && !strContainsWhite(val)) {
    m_altitude_mismatch_var = val;
    return(true);
  }
  
  return(false);
}

//-----------------------------------------------------------
// Procedure: onRunState

IvPFunction *BHV_ConstantAltitude::onRunState() 
{
  updateInfoIn();
  if(!m_domain.hasDomain("altitude")) {
    postEMessage("No 'altitude' variable in the helm domain");
    return(0);
  }

  ZAIC_PEAK zaic(m_domain, "altitude");
  zaic.setSummit(m_desired_altitude);
  zaic.setBaseWidth(m_basewidth);
  zaic.setPeakWidth(m_peakwidth);
  zaic.setSummitDelta(m_summitdelta);

  IvPFunction *ipf = zaic.extractIvPFunction();
  if(ipf)
    ipf->setPWT(m_priority_wt);
  else 
    postEMessage("Unable to generate constant-altitude IvP function");

  string zaic_warnings = zaic.getWarnings();
  if(zaic_warnings != "")
    postWMessage(zaic_warnings);

  return(ipf);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()
//   Purpose: Update relevant to the behavior from the info_buffer.
//            Warning messages may be posted if info is missing.
//   Returns: true if no relevant info is missing from the info_buffer.
//            false otherwise.

bool BHV_ConstantAltitude::updateInfoIn()
{
  bool ok;
  m_osd = getBufferDoubleVal("NAV_ALTITUDE", ok);

  // Should get ownship information from the InfoBuffer
  if(!ok) {
    postWMessage("No ownship ALTITUDE info in info_buffer.");  
    return(false);
  }
  
  double delta = m_osd - m_desired_altitude;
  if(delta < 0)
    delta *= -1; 

  if(m_altitude_mismatch_var != "")
    postMessage(m_altitude_mismatch_var, delta);
  
  return(true);
}



