/************************************************************/
/*    NAME: Steve Carter Feujo Nomeny                       */
/*    ORGN: NTNU, MIT                                       */
/*    FILE: ArduBridge.h                                    */
/*    DATE: September 9th, 2024                             */
/************************************************************/

#ifndef ArduBridge_HEADER
#define ArduBridge_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

#include "MOOSGeodesy.h"
#include <cli_arg.h>



#include "WarningSystem.h"
#include "UAV_Model.h"
#include "SetpointManager.h"




class ArduBridge : public AppCastingMOOSApp
{
  public:
    ArduBridge();
    ~ArduBridge();

  protected: // Standard MOOSApp functions to overload  
    bool OnNewMail(MOOSMSG_LIST &NewMail);
    bool Iterate();
    bool OnConnectToServer();
    bool OnStartUp();

  protected: // Standard AppCastingMOOSApp function to overload 
    bool buildReport();

  protected:
    void registerVariables();

    // Notify to DB
    void postTelemetryUpdate(const std::string& prefix );


    // Send command to UAV
    void sendSetpointsToUAV();

  private: // Configuration variables
    std::string  m_uav_prefix;

    static inline std::map<mavsdk::CliArg::Protocol, std::string> protocol2str = {
      {mavsdk::CliArg::Protocol::None, "None"},
      {mavsdk::CliArg::Protocol::Udp, "Udp"},
      {mavsdk::CliArg::Protocol::Tcp, "Tcp"},
      {mavsdk::CliArg::Protocol::Serial, "Serial"}
    };
    mavsdk::CliArg  m_cli_arg;

  
    bool         m_geo_ok;
    CMOOSGeodesy m_geodesy;

  
  private:

    const double MARKER_WIDTH = 14.0;

    const double HEADING_POINT_SIZE = 5;

    void visualizeHomeLocation();
    void visualizeLoiterLocation(const XYPoint& loiter_coord, bool visualize = true);
    void visualizeHeadingWaypoint(const XYPoint& heading_coord, bool visualize = true);

    // bool evaluateBoolFromString(const std::string& str) const { bool b; setBooleanOnString(b,str); return b;}
    bool parseCoordinateString(const std::string& input, double& lat, double& lon, double& x, double& y , std::string& vname) const;
  
  private: // State variables
    // For UAV

    std::string     m_vname;
    
    std::shared_ptr<WarningSystem> m_warning_system_ptr;
    UAV_Model       m_uav_model;
    SetpointManager m_setpoint_manager;
    
    bool  m_do_fly_to_waypoint;
    bool  m_do_takeoff;
    std::pair<bool, double> m_do_change_speed_pair;
    bool  m_do_reset_speed;

    bool  m_do_arm;
    bool  m_do_return_to_launch;
    bool  m_do_loiter;
  





};



#endif 

