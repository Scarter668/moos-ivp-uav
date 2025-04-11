/*****************************************************************/
/*    NAME: Michael Benjamin                                     */
/*    ORGN: Dept of Mechanical Engineering, MIT, Cambridge MA    */
/*    FILE: Proxonoi.cpp                                         */
/*    DATE: December 25th, 2019                                  */
/*                                                               */
/* This is unreleased BETA code. No permission is granted or     */
/* implied to use, copy, modify, and distribute this software    */
/* except by the author(s), or those designated by the author.   */
/*****************************************************************/

#include <iterator>
#include <unistd.h>
#include "MBUtils.h"
#include "ACTable.h"
#include "Proxonoi.h"
#include "VoronoiUtils.h"
#include "XYFormatUtilsPoly.h"
#include "NodeRecordUtils.h"
#include "NodeMessage.h"

#include "NodeMessageUtils.h"

#include "VoronoiSetPtMethods.h"

#include <cmath>
#include "AngleUtils.h"
#include "GeomUtils.h"
#include "Logger.h"

//---------------------------------------------------------
// Constructor

Proxonoi::Proxonoi()
{
  // Config variables
  m_reject_range = 10000;
  m_contact_local_coords = "verbatim";
  m_use_geodesy = false;

  m_post_region = false;
  m_post_poly = false;

  m_region_up_var = "PROX_UP_REGION";
  m_ignore_list_up_var = "PROX_SET_IGNORE_LIST";

  // State Variables
  m_osx = 0;
  m_osy = 0;
  m_osx_tstamp = false;
  m_osy_tstamp = false;
  m_poly_erase_pending = false;
  m_last_posted_spec = "";
  m_skip_count = 0;

  m_vcolor = "white";

  m_node_record_stale_treshold = 10; // 10s

  m_voronoi_field = VoronoiField();
  m_setpt_method = "center";
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool Proxonoi::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  static bool got_osx = false;
  static bool got_osy = false;

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++)
  {
    CMOOSMsg &msg = *p;
    std::string key = msg.GetKey();
    double dval = msg.GetDouble();
    std::string sval = msg.GetString();
    double mtime = msg.GetTime();
    std::string comm = msg.GetCommunity();

#if 0 // Keep these around just for template
    std::string msrc  = msg.GetSource();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    bool handled = false;

    if (key == "NAV_X")
    {
      m_osx = dval;
      m_osx_tstamp = mtime;
      got_osx = true;
      handled = true;
    }
    else if (key == "NAV_Y")
    {
      m_osy = dval;
      m_osy_tstamp = mtime;
      got_osy = true;
      handled = true;
    }
    else if (key == "PROX_CLEAR")
      handled = handleMailProxClear();
    else if (key == "NODE_REPORT")
    {
      handleMailNodeReport(sval);
      handled = true;
    }
    else if (key == "PROX_POLY_VIEW")
      handled = handleMailProxPolyView(sval);
    else if (key == m_ignore_list_up_var)
      handled = handleMailProxSetIgnoreList(sval);
    else if (key == m_region_up_var)
    {
      handleMailProxClear();
      handled = handleConfigOpRegion(sval);
    }
    else if (key == "PROX_SETPT_METHOD")
      handled = handleStringSetPointMethod(sval);

    else if (key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);

    if (!handled)
      reportRunWarning("Unhandled Mail: " + key);
  }

  if (got_osx && got_osy)
  {
    XYPoint os_point(m_osx, m_osy);
    double tstamp = MOOSTime();
    os_point.set_time(tstamp);
    m_voronoi_field.modProxPoint(m_ownship, os_point);

    got_osx = false;
    got_osy = false;
  }

  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool Proxonoi::OnConnectToServer()
{
  // registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool Proxonoi::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // //===========================================================
  // // Part 1: Update the split line based on the information of
  // // nearby contacts.
  // //===========================================================
  // updateSplitLines();

  //===========================================================
  // Part 1: Update the non stale vehicles
  //===========================================================
  checkRemoveVehicleStaleness();

  //===========================================================
  // Part 2: Using the split lines, carve down the voronoi poly
  //===========================================================
  updateVoronoiPoly();

  //===========================================================
  // Update the AreaBalanceSetpoint
  //===========================================================

  XYPoint center_reg = m_voronoi_field.getRegionPoly().get_center_pt();
  center_reg.set_label("center_reg");
  center_reg.set_label_color("off");
  center_reg.set_color("vertex", "red");
  center_reg.set_vertex_size(10);
  Notify("VIEW_POINT", center_reg.get_spec());
  
  XYPoint centroid_reg = m_voronoi_field.getRegionPoly().get_centroid_pt();
  centroid_reg.set_label("centroid_reg");
  centroid_reg.set_label_color("off");
  centroid_reg.set_color("vertex", "yellow");
  centroid_reg.set_vertex_size(10);
  Notify("VIEW_POINT", centroid_reg.get_spec());
  

  auto setpt_area = updateViewAreaBalanceSetpoint();
  
  auto setpt_churn = updateViewChurnSetpoint();
  
  auto setpt_grid = updateViewGridSearchSetpoint();
  
  XYPoint setpt;
  if (m_setpt_method=="gridsearch")
      setpt = setpt_grid;
  
  if(setpt.valid())
  {
    Notify("PROX_SETPT", setpt.get_spec());
  }

  postCentroidSetpoint();

  //===========================================================

  // Part 3.  Post it;
  if (m_prox_poly.is_convex())
  {
    std::string spec = m_prox_poly.get_spec();
    bool new_spec = (spec != m_last_posted_spec);

    if (m_post_poly && !m_poly_erase_pending && new_spec)
    {
      Notify("VIEW_POLYGON", spec);
      m_last_posted_spec = spec;
    }
    Notify("PROXONOI_POLY", spec);
  }
  else
  {
    m_prox_poly.set_active(false);
    std::string spec = m_prox_poly.get_spec();
    bool new_spec = (spec != m_last_posted_spec);
    if (m_post_poly && new_spec)
    {
      Notify("VIEW_POLYGON", spec);
      m_last_posted_spec = spec;
    }

    Notify("PROXONOI_POLY", spec);
    m_poly_erase_pending = false;
  }

  if (m_poly_erase_pending)
  {
    m_prox_poly.set_active(false);
    std::string spec = m_prox_poly.get_spec();
    Notify("VIEW_POLYGON", spec);
    m_poly_erase_pending = false;
  }

  if ((m_skip_count % 400) == 0)
  {
    auto region = m_voronoi_field.getRegionPoly();
    if (region.is_convex())
    {
      std::string spec = region.get_spec();
      bool new_spec = (spec != m_last_posted_spec);
      if (m_post_region && new_spec)
      {
        Notify("VIEW_POLYGON", spec);
        m_last_posted_spec = spec;
      }
      Notify("PROXONOI_REGION", spec);
    }
    m_skip_count = 0;
  }
  else
    m_skip_count += 1;

  // if ((m_iteration % 20) == 0)
  // {
  //   shareProxPolyArea();
  //   shareProxPoly();
  // }

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Proxonoi::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  Notify("PROXONOI_PID", getpid());

  m_ownship = m_host_community;
  if (m_ownship == "")
  {
    std::cout << "Vehicle Name (MOOS community) not provided" << std::endl;
    return (false);
  }

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if (!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for (p = sParams.begin(); p != sParams.end(); p++)
  {
    std::string orig = *p;
    std::string line = *p;
    std::string param = tolower(biteStringX(line, '='));
    std::string value = line;

    bool handled = false;
    if (param == "region")
      handled = handleConfigOpRegion(value);
    else if (param == "post_poly")
      handled = setBooleanOnString(m_post_poly, value);
    else if (param == "post_region")
      handled = setBooleanOnString(m_post_region, value);
    else if ((param == "reject_range") && (value == "nolimit"))
    {
      m_reject_range = -1;
      handled = true;
    }
    else if (param == "ignore_name")
    {
      m_name_reject.insert(tolower(value));
      handled = true;
    }
    else if (param == "always_ignore_name")
    {
      m_name_always_reject.insert(tolower(value));
      m_name_reject.insert(tolower(value));
      handled = true;
    }
    else if (param == "reject_range")
      handled = setDoubleOnString(m_reject_range, value);
    else if (param == "neighbor_stale_treshold")
      handled = setNonNegDoubleOnString(m_node_record_stale_treshold, value);

    else if (param == "region_update_var")
    {
      if (value != "")
      {
        m_region_up_var = toupper(value);
        handled = true;
      }
    }
    else if (param == "ignore_list_update_var")
    {
      if (value != "")
      {
        m_ignore_list_up_var = toupper(value);
        handled = true;
      }
    }
    else if (param == "vcolor")
    {
      m_vcolor = value;
      handled = true;
    }
    else if (param == "setpt_method")
      handled = handleStringSetPointMethod(value);

    if (!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: registerVariables

void Proxonoi::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NODE_REPORT", 0);
  Register("PROX_POLY_VIEW", 0);
  Register("PROX_CLEAR", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register(m_ignore_list_up_var, 0);
  Register(m_region_up_var, 0);

  Register("PROX_SETPT_METHOD", 0);
}

//---------------------------------------------------------
// Procedure: handleConfigOpRegion

bool Proxonoi::handleConfigOpRegion(std::string opstr)
{
  XYPolygon op_region = string2Poly(opstr);
  op_region.set_label("prox_opregion");

  if (!op_region.is_convex())
    return (false);

  // If all good, then clear the states
  handleMailProxClear();

  m_voronoi_field.setRegionPoly(op_region);

  return (true);
}

bool Proxonoi::handleStringSetPointMethod(std::string method)
{
  method = tolower(method);
  if ((method == "area_balance") ||
      (method == "churn") ||
      (method == "gridsearch") ||
      (method == "centroid") ||
      (method == "center"))
    m_setpt_method = method;
  else
    return (false);

  return (true);
}

//---------------------------------------------------------
// Procedure: handleMailNodeMessage

// bool Proxonoi::handleMailNodeMessage(std::string msg)
// {
//   NodeMessage node_msg = string2NodeMessage(msg);
//   std::string vname = tolower(node_msg.getSourceNode());
//   if (vname == m_ownship)
//     return (true);

//   return (true);
// }

//---------------------------------------------------------
// Procedure: handleMailNodeReport
//   Example: NAME=alpha,TYPE=KAYAK,UTC_TIME=1267294386.51,
//            X=29.66,Y=-23.49, LAT=43.825089,LON=-70.330030,
//            SPD=2.00,HDG=119.06,YAW=119.05677,DEPTH=0.00,
//            LENGTH=4.0,MODE=DRIVE

void Proxonoi::handleMailNodeReport(std::string report)
{
  NodeRecord new_node_record = string2NodeRecord(report);

  // Part 1: Decide if we want to override X/Y with Lat/Lon based on
  // user configuration and state of the node record.
  bool override_xy_with_latlon = true;
  if (m_contact_local_coords == "verbatim")
    override_xy_with_latlon = false;
  if (!m_use_geodesy)
    override_xy_with_latlon = false;
  if (m_contact_local_coords == "lazy_lat_lon")
  {
    if (new_node_record.isSetX() && new_node_record.isSetY())
      override_xy_with_latlon = false;
  }

  if (!new_node_record.isSetLatitude() || !new_node_record.isSetLongitude())
    override_xy_with_latlon = false;

  // Part 2: If we can override xy with latlon and configured to do so
  // then find the X/Y from MOOSGeodesy and Lat/Lon and replace.
  if (override_xy_with_latlon)
  {
    double nav_x, nav_y;
    double lat = new_node_record.getLat();
    double lon = new_node_record.getLon();

#ifdef USE_UTM
    m_geodesy.LatLong2LocalUTM(lat, lon, nav_y, nav_x);
#else
    m_geodesy.LatLong2LocalGrid(lat, lon, nav_y, nav_x);
#endif
    new_node_record.setX(nav_x);
    new_node_record.setY(nav_y);
  }

  std::string vname = new_node_record.getName();

  // If incoming node name matches ownship, just ignore the node report
  // but don't return false which would indicate an error.

  if (vname == m_ownship || (m_name_reject.count(tolower(vname)) > 0))
    return;

  bool newly_known_vehicle = false;
  if (m_map_node_records.count(vname) == 0)
    newly_known_vehicle = true;

  // If we are (a) not currently tracking the given vehicle, and (b)
  // a reject_range is enabled, and (c) the contact is outside the
  // reject_range, then ignore this contact.
  double cnx = new_node_record.getX();
  double cny = new_node_record.getY();
  double range = hypot(m_osx - cnx, m_osy - cny);
  if (newly_known_vehicle && (m_reject_range > 0))
  {
    if (range > m_reject_range)
      return;
  }

  //  if(!new_node_record.valid()) {
  //  Notify("PROXONOI_WARNING", "Bad Node Report Received");
  //  reportRunWarning("Bad Node Report Received");
  //  return;
  //}

  m_map_node_records[vname] = new_node_record;
  m_map_ranges[vname] = range;

  XYPoint pt(cnx, cny);
  pt.set_time(m_curr_time);

  if (newly_known_vehicle)
    m_voronoi_field.addProxPoint(vname, pt);
  else
    m_voronoi_field.modProxPoint(vname, pt);
}

//---------------------------------------------------------
// Procedure: handleMailProxPolyView()

bool Proxonoi::handleMailProxPolyView(std::string msg)
{
  msg = tolower(stripBlankEnds(msg));
  if (msg == "toggle")
    m_post_poly = !m_post_poly;
  else if ((msg == "false") || (msg == "off"))
    m_post_poly = false;
  else if ((msg == "true") || (msg == "on"))
    m_post_poly = true;
  else
    return (false);

  if (m_post_poly == false)
    m_poly_erase_pending = true;

  return (true);
}

//---------------------------------------------------------
// Procedure: handleMailProxClear()

bool Proxonoi::handleMailProxClear()
{
  // ========================================================
  // Part 1: Reset the prox poly to the entire region. But we
  // want to retain the label of the prox_poly on the new one
  // ========================================================

  m_voronoi_field.clear();
  XYPoint pt(m_osx, m_osy);
  pt.set_time(m_curr_time);
  m_voronoi_field.addProxPoint(m_ownship, pt);
  m_voronoi_field.updateProxPolys();

  m_prox_poly = m_voronoi_field.getVPoly(m_ownship);
  m_prox_poly.set_label("vpoly_" + m_ownship);

  // ========================================================
  // Part 2: Reset all the data structures
  // ========================================================

  m_map_node_records.clear();

  // ========================================================
  // Part 3: Mark the prox poly as needing to be erased
  // ========================================================
  m_poly_erase_pending = true;

  return (true);
}

//------------------------------------------------------------
// Procedure: shareProxPolyArea()

void Proxonoi::shareProxPolyArea()
{
  if (!m_prox_poly.valid())
    return;

  NodeMessage msg(m_ownship, "all", "PROX_POLY_AREA");
  msg.setDoubleVal(m_prox_poly.area());
  msg.setColor("off");

  Notify("NODE_MESSAGE_LOCAL", msg.getSpec());
}

//------------------------------------------------------------
// Procedure: shareProxPoly()

void Proxonoi::shareProxPoly()
{
  if (!m_prox_poly.valid())
    return;

  NodeMessage msg(m_ownship, "all", "PROX_POLY_NEIGHBOR");

  // Logger::info("Sharing Prox Poly (3spec): " + m_prox_poly.get_spec(3));

  msg.setStringVal(m_prox_poly.get_spec(3));
  msg.setColor("off");

  Notify("NODE_MESSAGE_LOCAL", msg.getSpec(3));
}

//------------------------------------------------------------
// Procedure: updateVoronoiPoly()

bool Proxonoi::updateVoronoiPoly()
{
  m_prox_poly = XYPolygon(); // Init to a null poly

  // Sanity check 2: if no ownship position return false
  if ((m_osx_tstamp == false) || (m_osy_tstamp == false))
    return (false);

  // Sanity check 3: if ownship not in op_region, return false
  if (!m_voronoi_field.containsPoint(m_osx, m_osy))
  {
    if (m_os_in_prox_region)
      m_poly_erase_pending = true;
    m_os_in_prox_region = false;
    return (false);
  }
  else
    m_os_in_prox_region = true;

  m_voronoi_field.updateProxPolys();
  m_prox_poly = m_voronoi_field.getVPoly(m_ownship);

  m_prox_poly.set_label("vpoly_" + m_ownship);

  // Possibly combine very close vertices. In this case, vertices
  // within one meter of each other.
  bool can_simplify = true;
  while (can_simplify)
    can_simplify = m_prox_poly.simplify(1);

  // Set the viewable components of the prox poly
  m_prox_poly.set_label("vpoly_" + m_ownship);
  m_prox_poly.set_color("edge", "white");
  m_prox_poly.set_color("vertex", "blue");
  m_prox_poly.set_color("fill", "pink");
  m_prox_poly.set_transparency(0.15);

  return (true);
}

//-----------------------------------------------------
// Procedure: handleMailProxSetIgnoreList(std::string);
bool Proxonoi::handleMailProxSetIgnoreList(std::string msg)
{

  // First clear the states
  handleMailProxClear();

  // Make a new set which will replace the old set.
  std::set<std::string> new_name_reject;
  // parse message
  std::vector<std::string> svector = parseString(msg, ',');
  for (unsigned int i = 0; i < svector.size(); i++)
  {
    new_name_reject.insert(svector[i]);
  }

  // Add in the names we allways ignore
  std::set<std::string>::iterator it;
  for (it = m_name_always_reject.begin(); it != m_name_always_reject.end(); it++)
  {
    new_name_reject.insert(*it);
  }

  m_name_reject = new_name_reject;

  return (true);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool Proxonoi::buildReport()
{
  //=================================================================
  // Part 1: Header Content
  //=================================================================
  std::string reject_range = "off";
  if (m_reject_range > 0)
    reject_range = doubleToStringX(m_reject_range, 2);

  std::string in_region = boolToString(m_os_in_prox_region);
  std::string erase_pending = boolToString(m_poly_erase_pending);

  m_msgs << "Reject Range:   " << reject_range << std::endl;
  m_msgs << "In Prox Region: " << in_region << std::endl;
  m_msgs << "Erase Pending:  " << erase_pending << std::endl;
  m_msgs << "Treshold Value: " << m_node_record_stale_treshold << std::endl;

  double area = m_voronoi_field.getVArea(m_ownship);
  if (area > 10000)
    area /= 1000;

  m_msgs << "Ownship Area:       " << doubleToStringX(area, 0) << std::endl;
  m_msgs << "Ownship Position:   (" << m_osx << ", " << m_osy << ")" << std::endl;
  m_msgs << "Setpoint Method:   " << m_setpt_method << std::endl;

  m_msgs << "\n\n";
  //=================================================================
  // Part 4: Contact Status Summary
  //=================================================================
  m_msgs << "Contact Status Summary:" << std::endl;
  m_msgs << "-----------------------" << std::endl;

  ACTable actab(5, 2);
  actab.setColumnJustify(1, "right");
  actab.setColumnJustify(2, "right");
  actab.setColumnJustify(3, "right");
  actab.setColumnJustify(4, "right");
  actab << "Contact | Range | Area | TimeSinceRec | ValidPoly";
  actab.addHeaderLines();

  std::map<std::string, NodeRecord>::iterator q;
  for (q = m_map_node_records.begin(); q != m_map_node_records.end(); q++)
  {
    std::string vname = q->first;
    std::string range = doubleToString(m_map_ranges[vname], 1);

    std::string area_str = "-";
    std::string time_str = "-";
    std::string valid_poly_str = "-";

    bool valid_poly = m_voronoi_field.getVPoly(vname).is_convex();
    valid_poly_str = boolToString(valid_poly);

    double area = m_voronoi_field.getVArea(vname);
    if (area > 10000)
      area /= 1000;
    area_str = doubleToStringX(area, 0);

    double time_to_treshold = (MOOSTime() - m_voronoi_field.getVPoint(vname).get_time());
    time_str = doubleToStringX(time_to_treshold, 1);

    actab << vname << range << area_str << time_str << valid_poly_str;
  }
  m_msgs << actab.getFormattedString();

  return (true);
}

XYPoint Proxonoi::updateViewAreaBalanceSetpoint()
{

  XYPoint nullpt;
  XYPoint areaSetPt = getSetPtAreaBalance(m_voronoi_field, m_ownship);

  if (!areaSetPt.valid())
  {
    Logger::warning("Area Setpoint not valid");
    return nullpt;
  }

  // std::string label = "areaSetPt_" + m_ownship;
  std::string label = "a_" + m_ownship;
  areaSetPt.set_label(label);
  // areaSetPt.set_label_color("off");
  areaSetPt.set_color("vertex", m_vcolor);
  areaSetPt.set_vertex_size(12);
  // areaSetPt.set_duration(5);
  Notify("VIEW_POINT", areaSetPt.get_spec());

  return areaSetPt;
}

XYPoint Proxonoi::updateViewChurnSetpoint()
{
  XYPoint nullpt;
  XYPoint churnSetPt = getSetPtChurn(m_voronoi_field, m_ownship);

  if (!churnSetPt.valid())
  {
    Logger::warning("Area Setpoint not valid");
    return nullpt;
  }

  // std::string label = "churnSetPt_" + m_ownship;
  std::string label = "c_" + m_ownship;
  churnSetPt.set_label(label);
  // churnSetPt.set_label_color("off");
  churnSetPt.set_color("vertex", m_vcolor);
  churnSetPt.set_vertex_size(10);
  // churnSetPt.set_duration(5);
  Notify("VIEW_POINT", churnSetPt.get_spec());

  return churnSetPt;
}


XYPoint Proxonoi::updateViewGridSearchSetpoint()
{
  XYPoint nullpt;
  XYPoint gridSearchSetPt = calculateGridSearchSetpoint();

  if (!gridSearchSetPt.valid())
  {
    Logger::warning("Area Setpoint not valid");
    return nullpt;
  }

  // std::string label = "gridSearchSetPt_" + m_ownship;
  std::string label = "g_" + m_ownship;
  gridSearchSetPt.set_label(label);
  // gridSearchSetPt.set_label_color("off");
  gridSearchSetPt.set_color("vertex", m_vcolor);
  gridSearchSetPt.set_vertex_size(10);
  // gridSearchSetPt.set_duration(5);
  Notify("VIEW_POINT", gridSearchSetPt.get_spec());

  return gridSearchSetPt;
}

void Proxonoi::postCentroidSetpoint()
{

  XYPoint centroidSetPt = m_prox_poly.get_centroid_pt();
  if (!centroidSetPt.valid())
  {
    Logger::warning("Centroid Setpoint not valid");
    return;
  }

  double area = m_voronoi_field.getVArea(m_ownship); // O(1)
  if (area > 10000)
    area /= 1000;
  unsigned int uint_area = (unsigned int)(area);

  std::string label = "centroid_" + m_ownship + " (" + uintToString(uint_area) + ")";
  centroidSetPt.set_label("centroidSetPt_" + m_ownship);
  centroidSetPt.set_label_color("white");
  centroidSetPt.set_msg(label);
  centroidSetPt.set_color("vertex", "white");
  centroidSetPt.set_vertex_size(10);
  // centroidSetPt.set_duration(5);
  Notify("VIEW_POINT", centroidSetPt.get_spec());
}

void Proxonoi::checkRemoveVehicleStaleness()
{

  double curr_time = MOOSTime();

  std::vector<std::string> vnames = m_voronoi_field.getStaleKeys(curr_time, m_node_record_stale_treshold);

  for (const auto &vname : vnames)
  {

    if (vname == m_ownship)
      continue;

    m_voronoi_field.removePoint(vname);
    auto time = m_voronoi_field.getVPoint(vname).get_time();

    Logger::info("Checking Poly Staleness: Erased " + vname + " time: " + doubleToStringX(time, 2) + " curr_time: " + doubleToStringX(curr_time, 2) +
                 " treshold: " + doubleToStringX(m_node_record_stale_treshold, 2) +
                 " diff: " + doubleToStringX(curr_time - time, 2));
  }
}



//-----------------------------------------------------------
// Procedure: getSetPtChurn()

XYPoint Proxonoi::calculateGridSearchSetpoint() const
{
  // ==============================================
  // Part 1: Sanity Checks
  // ==============================================
  XYPoint null_pt;
  if(!m_voronoi_field.hasKey(m_ownship))
    return(null_pt);

  
  if(!m_prox_poly.valid())
    return(null_pt);
  
  XYPoint centroid_pt = m_prox_poly.get_centroid_pt();
  // XYPoint areabl_pt = getSetPtAreaBalance(m_voronoi_field, m_ownship);



  auto ref_pt = centroid_pt;

  if(!ref_pt.valid()){
    Logger::warning("Centroid Setpoint not valid");
    return(null_pt);
  }


  // Logger::info("Centroid Setpoint: " + ref_pt.get_spec());
  
  XYPoint reg_center_pt = m_voronoi_field.getRegionPoly().get_center_pt();
  
  double rel_ang = relAng(reg_center_pt, ref_pt);
  
  // Logger::info("Relative angle calculated: " + doubleToString(rel_ang, 2));
  
  static XYPoint target_pt;
  
  double heading = rel_ang-90;
  double default_dist = 200;

  XYPoint final_pt = projectPoint(heading, default_dist, ref_pt);
  
  XYPoint cpt = m_voronoi_field.getVPoint(m_ownship);
  double dist_to_target = 0;
  // Logger::info("Current Point: " + cpt.get_spec());
  // Logger::info("Final Point: " + final_pt.get_spec());
  // Logger::info("Target Point: " + target_pt.get_spec());
  if(target_pt.valid()){
    dist_to_target = distPointToPoint(cpt, target_pt);
    // Logger::info("Distance to target calculated: " + doubleToString(dist_to_target, 2));
  }
  else{
    dist_to_target = distPointToPoint(cpt, final_pt);
    // Logger::info("Distance to final calculated: " + doubleToString(dist_to_target, 2));
  }
  
  // Logger::info("Distance to target calculated: " + doubleToString(dist_to_target, 2));
  // create a function that goes to 1000 as dist goes to 30
  // double mag = 1000 * (1 - (dist / 30));
  double mag = 150 * (1 - (dist_to_target / 150));
  if (mag < 0)
    mag = 0;
  else if (mag > 1000) 
    mag = 1000;

  // Logger::info("Magnitude calculated: " + doubleToString(mag, 2));


  final_pt = projectPoint(heading, mag+default_dist, ref_pt);
  // Logger::info("Final point calculated: " + final_pt.get_spec());

  target_pt = final_pt;
  return(final_pt);  
}


//------------------------------------------------------------
// UTILITY FUNCTIONS
//  These are not part of the Proxonoi class, but are
//  used by the Proxonoi class.
//-------------------------------------------------------------

//-----------------------------------------------------------
// Procedure: getSetPtAreaBalance()

// XYPoint getSetPtAreaBalance(const XYPolygon op_region, std::string key, const std::map<std::string, XYPolygon> &map_vpolygons, const std::map<std::string, double> &map_vAreas)
// {
//   XYPoint null_pt; // invalid by default
//   if (map_vpolygons.find(key) == map_vpolygons.end()){
//     Logger::warning("Key not found in map_vpolygons: " + key);
//     return (null_pt);
//   }

//   XYPoint cpt = map_vpolygons.at(key).get_centroid_pt();
//   if (!cpt.valid())
//   {
//     Logger::warning("Centroid not valid for key: " + key);
//     return (null_pt);
//   }

//   // If the centroid is not in the op_region, return the centroid
//   // ==============================================
//   // Part 1: Sanity Checks
//   // ==============================================

//   if (!op_region.is_convex())
//     return (cpt);

//   // Find the neighbors of the current polygon
//   std::vector<std::string> neighbors;
//   XYPolygon kpoly = map_vpolygons.at(key);
//   for (auto p = map_vpolygons.begin(); p != map_vpolygons.end(); p++)
//   {
//     std::string pkey = p->first;
//     if (pkey != key)
//     {
//       double dist = kpoly.dist_to_poly(p->second);
//       if (dist < 1)
//         neighbors.push_back(pkey);
//     }
//   }

//   if (neighbors.size() == 0)
//     return (cpt);

//   // ==============================================
//   // Part 2: Calculate component vectors, one for each
//   //         neighbor and add all vectors, keeping track
//   //         of the total magnitud along the way
//   // ==============================================
//   double pa = map_vAreas.at(key);
//   double total_abs_mag = 0;

//   // The vector point (vpt) is sum of all hdg/mag vectors.
//   // It starts at the current center point of the cell.
//   XYPoint vpt = cpt;
//   for (unsigned int j = 0; j < neighbors.size(); j++)
//   {
//     std::string jkey = neighbors[j];
//     XYPoint jpt = map_vpolygons.at(jkey).get_center_pt();
//     double paj = map_vAreas.at(jkey);

//     double hdg = relAng(cpt, jpt);
//     double mag = 0;
//     double dist = distPointToPoint(cpt, jpt);
//     if ((pa + paj) > 0)
//     {
//       double nfactor = paj - pa;
//       double dfactor = (pa + paj) / 2;
//       mag = (nfactor / dfactor) * (dist / 2);
//     }

//     // Add the new hdg/mag vector to the running sum
//     vpt = projectPoint(hdg, mag, vpt);
//     total_abs_mag += std::abs(mag);
//   }

//   // ==============================================
//   // Part 3: Define the final point, having the heading of the sum
//   //         of vectors, but magnitude is avg of all magnitudes
//   // ==============================================
//   double avg_abs_mag = total_abs_mag / (double)(neighbors.size());

//   double hdg = relAng(cpt, vpt);
//   double dist = avg_abs_mag;

//   XYPoint final_pt = projectPoint(hdg, dist, cpt);

//   // ==============================================
//   // Part 4: Sanity check that point is within op_region
//   // ==============================================
//   if (!op_region.contains(final_pt.x(), final_pt.y()))
//     final_pt = op_region.closest_point_on_poly(final_pt);

//   return (final_pt);
// }




