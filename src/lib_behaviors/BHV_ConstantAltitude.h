/*****************************************************************/
/*    NAME: Michael Benjamin, modified by Steve Carter Feujo Nomeny */
/*    ORGN: Dept of Mechanical Engineering, MIT, Cambridge MA    */
/*    FILE: BHV_ConstantAltitude.h                                  */
/*****************************************************************/

#pragma once

#include "IvPBehavior.h"

class BHV_ConstantAltitude : public IvPBehavior {
public:
  BHV_ConstantAltitude(IvPDomain);
  ~BHV_ConstantAltitude() {}
  
  void         onIdleState() {updateInfoIn();}
  IvPFunction* onRunState();
  bool         setParam(std::string, std::string);

 protected:
  bool         updateInfoIn();

 protected: // Configuration variables
  double      m_desired_altitude;
  double      m_peakwidth;
  double      m_basewidth;
  double      m_summitdelta;
  std::string m_altitude_mismatch_var;

 protected: // State variables
  double      m_osd;
};


#ifdef WIN32
	// Windows needs to explicitly specify functions to export from a dll
   #define IVP_EXPORT_FUNCTION __declspec(dllexport) 
#else
   #define IVP_EXPORT_FUNCTION
#endif

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain) 
  {return new BHV_ConstantAltitude(domain);}
}









