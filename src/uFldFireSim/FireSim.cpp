/*****************************************************************/
/*    NAME: Michael Benjamin, modified by Steve Nomeny           */
/*    FILE: FireSim.cpp                                          */
/*    DATE: Feb 2024                                        */
/*                                                               */
/*****************************************************************/

#include <iterator>
#include <cmath>
#include "FireSim.h"
#include "VarDataPairUtils.h"
#include "MacroUtils.h"
#include "NodeRecordUtils.h"
#include "AngleUtils.h"
#include "GeomUtils.h"
#include "ColorParse.h"
#include "XYCircle.h"
#include "XYMarker.h"
#include "MBUtils.h"
#include "ACTable.h"
#include "FileBuffer.h"
#include "XYFormatUtilsPoly.h"
#include "XYPolyExpander.h"

using namespace std;

//---------------------------------------------------------
// Constructor

FireSim::FireSim()
{
  // Config vars
  m_detect_rng_min = 25;
  m_detect_rng_max = 40;
  m_detect_rng_pd = 0.5;

  m_detect_rng_transparency = 0.1;
  m_detect_rng_show = true;
  m_finish_upon_win = false;
  m_fire_color = "red";

  // State vars
  m_finished = false;
  m_scouts_inplay = false;
  m_total_discoverers = 0;
  m_last_broadcast = 0;
  m_vname_leader = "tie";

  m_known_undiscovered = 0;

  m_ac.setMaxEvents(20);
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool FireSim::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++)
  {
    // p->Trace();
    CMOOSMsg msg = *p;

    string key = msg.GetKey();
    string sval = msg.GetString();
    string comm = msg.GetCommunity();

    bool handled = false;
    string warning;
    if ((key == "NODE_REPORT") || (key == "NODE_REPORT_LOCAL"))
      handled = handleMailNodeReport(sval);
    else if (key == "DISCOVER_REQUEST")
      handled = handleMailDiscoverRequest(sval);
    else if (key == "SCOUT_REQUEST")
      handled = handleMailScoutRequest(sval);
    else if ((key == "XFIRE_ALERT") && (comm == "shoreside"))
    {
      handled = m_fireset.fireAlert(sval, m_curr_time, warning);
      updateFinishStatus();
      postFireMarkers();
    }
    else if ((key == "XFOUND_FIRE") && (comm == "shoreside"))
    {
      string xstr = tokStringParse(sval, "x");
      string ystr = tokStringParse(sval, "y");
      if ((xstr == "") || (ystr == ""))
        handled = false;
      else
      {
        double xval = atof(xstr.c_str());
        double yval = atof(ystr.c_str());
        string fname = m_fireset.getNameClosestFire(xval, yval);
        declareDiscoveredFire("nature", fname);
        postFireMarkers();
        handled = true;
      }
    }

    if (!handled)
    {
      reportRunWarning("Unhandled mail: " + key);
      reportRunWarning(warning);
    }
  }
  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool FireSim::OnConnectToServer()
{
  registerVariables();
  return (true);
}

//------------------------------------------------------------
// Procedure: registerVariables()

void FireSim::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();

  Register("XFIRE_ALERT", 0);
  Register("XFOUND_FIRE", 0);
  Register("NODE_REPORT", 0);
  Register("NODE_REPORT_LOCAL", 0);
  Register("DISCOVER_REQUEST", 0);
  Register("SCOUT_REQUEST", 0);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool FireSim::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if (!m_finished)
  {
    tryDiscovers();
    tryScouts();
  }

  postDetectRngPolys();
  postScoutRngPolys();

  // periodically broadcast fire info to all vehicles
  if ((m_curr_time - m_last_broadcast) > 15)
  {
    broadcastFires();
    applyTMateColors();
    m_last_broadcast = m_curr_time;
  }

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool FireSim::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  if (!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for (p = sParams.begin(); p != sParams.end(); p++)
  {
    string orig = *p;
    string line = *p;
    string param = biteStringX(line, '=');
    string value = line;

    bool handled = false;
    string warning;
    if (param == "fire_file")
    {
      handled = m_fireset.handleFireFile(value, m_curr_time, warning);
      Notify("PLOGGER_CMD", "COPY_FILE_REQUEST=" + value);
      updateFinishStatus();
    }
    else if (param == "show_detect_rng")
      handled = setBooleanOnString(m_detect_rng_show, value);
    else if (param == "detect_rng_min")
      handled = handleConfigDetectRangeMin(value);
    else if (param == "detect_rng_max")
      handled = handleConfigDetectRangeMax(value);
    else if (param == "detect_rng_pd")
      handled = handleConfigDetectRangePd(value);
    else if ((param == "detect_rng_transparency") && isNumber(value))
      handled = setNonNegDoubleOnString(m_detect_rng_transparency, value);
    else if (param == "winner_flag")
      handled = addVarDataPairOnString(m_winner_flags, value);
    else if (param == "leader_flag")
      handled = addVarDataPairOnString(m_leader_flags, value);
    else if (param == "finish_flag")
      handled = addVarDataPairOnString(m_finish_flags, value);
    else if (param == "fire_color")
      handled = setColorOnString(m_fire_color, value);
    else if (param == "finish_upon_win")
      handled = setBooleanOnString(m_finish_upon_win, value);

    if (!handled)
    {
      reportUnhandledConfigWarning(orig);
      if (warning != "")
        reportUnhandledConfigWarning(warning);
    }
  }
  postFireMarkers();

  srand(time(NULL));

  registerVariables();

  return (true);
}

//------------------------------------------------------------
// Procedure: handleConfigDetectRangeMin()

bool FireSim::handleConfigDetectRangeMin(string str)
{
  if (!isNumber(str))
    return (false);

  m_detect_rng_min = atof(str.c_str());

  if (m_detect_rng_min < 0)
    m_detect_rng_min = 0;
  if (m_detect_rng_max <= m_detect_rng_min)
    m_detect_rng_max = m_detect_rng_min + 1;

  return (true);
}

//------------------------------------------------------------
// Procedure: handleConfigDetectRangeMax()

bool FireSim::handleConfigDetectRangeMax(string str)
{
  if (!isNumber(str))
    return (false);

  m_detect_rng_max = atof(str.c_str());

  if (m_detect_rng_max < 0)
    m_detect_rng_max = 0;
  if (m_detect_rng_min >= m_detect_rng_max)
    m_detect_rng_min = m_detect_rng_max * 0.9;

  return (true);
}

//------------------------------------------------------------
// Procedure: handleConfigDetectRangePd()

bool FireSim::handleConfigDetectRangePd(string str)
{
  if (!isNumber(str))
    return (false);

  m_detect_rng_pd = atof(str.c_str());

  if (m_detect_rng_pd < 0)
    m_detect_rng_pd = 0;
  if (m_detect_rng_pd > 1)
    m_detect_rng_pd = 1;

  return (true);
}

//---------------------------------------------------------
// Procedure: handleMailNodeReport()
//   Example: NAME=alpha,TYPE=KAYAK,UTC_TIME=1267294386.51,
//            X=29.66,Y=-23.49,LAT=43.825089, LON=-70.330030,
//            SPD=2.00, HDG=119.06,YAW=119.05677,DEPTH=0.00,
//            LENGTH=4.0,MODE=ENGAGED

bool FireSim::handleMailNodeReport(const string &node_report_str)
{
  NodeRecord new_record = string2NodeRecord(node_report_str);

  if (!new_record.valid())
  {
    Notify("SWM_DEBUG", "Invalid incoming node report");
    reportRunWarning("ERROR: Unhandled node record");
    return (false);
  }

  // In case there is an outstanding RunWarning indicating the lack
  // of a node report for a given vehicle, retract it here. This is
  // mostly a startup timing issue. Sometimes a sensor request is
  // received before a node report. Only a problem if the node report
  // never comes. Once we get one, it's no longer a problem.
  string vname = new_record.getName();
  retractRunWarning("No NODE_REPORT received for " + vname);

  if (m_map_node_records.count(vname) == 0)
    broadcastFires();

  m_map_node_records[vname] = new_record;

  return (true);
}

//---------------------------------------------------------
// Procedure: tryScouts()

void FireSim::tryScouts()
{
  // For each vehicle, check if pending scout actions are to be applied
  map<string, NodeRecord>::iterator p;
  for (p = m_map_node_records.begin(); p != m_map_node_records.end(); p++)
  {
    string vname = p->first;
    tryScoutsVName(vname);
  }
}

//---------------------------------------------------------
// Procedure: tryScoutsVName()

void FireSim::tryScoutsVName(string vname)
{
  // If vehicle has not posted a scout request recently, then the
  // scout ability is off for this vehicle.
  double elapsed_req = m_curr_time - m_map_node_last_scout_req[vname];
  if (elapsed_req > 5)
    return;
  double elapsed_try = m_curr_time - m_map_node_last_scout_try[vname];
  if (elapsed_try < 1.5)
    return;
  if (m_map_node_vroles[vname] != "scout")
    return;

  m_map_node_last_scout_try[vname] = m_curr_time;
  m_map_node_scout_tries[vname]++;

  set<string> fire_names = m_fireset.getFireNames();
  set<string>::iterator p;
  for (p = fire_names.begin(); p != fire_names.end(); p++)
  {
    string fname = *p;
    tryScoutsVNameFire(vname, fname);
  }
}

//---------------------------------------------------------
// Procedure: tryScoutsVNameFire()

void FireSim::tryScoutsVNameFire(string vname, string fname)
{
  Fire fire = m_fireset.getFire(fname);
  if (fire.getState() == "discovered")
    return;

  bool result = rollDice(vname, fname, "scout");
  if (result)
    declareScoutedFire(vname, fname);
}

//---------------------------------------------------------
// Procedure: tryDiscovers()

void FireSim::tryDiscovers()
{
  map<string, NodeRecord>::iterator p;
  for (p = m_map_node_records.begin(); p != m_map_node_records.end(); p++)
  {
    string vname = p->first;
    tryDiscoversVName(vname);
  }
}

//---------------------------------------------------------
// Procedure: tryDiscoversVName()

void FireSim::tryDiscoversVName(string vname)
{
  // If vehicle has has not posted a discover request recently, then the
  // discover ability is off for this vehicle.
  double elapsed_req = m_curr_time - m_map_node_last_discover_req[vname];
  if (elapsed_req > 5)
    return;
  double elapsed_try = m_curr_time - m_map_node_last_discover_try[vname];
  if (elapsed_try < 1)
    return;
  if (m_map_node_vroles[vname] != "discover")
    return;

  m_map_node_last_discover_try[vname] = m_curr_time;
  m_map_node_discover_tries[vname]++;

  set<string> fire_names = m_fireset.getFireNames();
  set<string>::iterator p;
  for (p = fire_names.begin(); p != fire_names.end(); p++)
  {
    string fname = *p;
    tryDiscoversVNameFire(vname, fname);
  }
}

//---------------------------------------------------------
// Procedure: tryDiscoversVNameFire()

void FireSim::tryDiscoversVNameFire(string vname, string fname)
{
  Fire fire = m_fireset.getFire(fname);
  if (fire.getState() == "discovered")
    return;

  bool detect_result = rollDice(vname, fname, "discover"); // TODO: modify discover
  if (detect_result)
    declareDiscoveredFire(vname, fname);
}

//------------------------------------------------------------
// Procedure: updateLeaderStatus()

void FireSim::updateLeaderStatus()
{
  // Part 1: Note prev leader to detect a lead change
  string prev_leader = m_vname_leader;

  // Part 2: Calc highest number of discoveries over any vehicle
  unsigned int highest_discover_count = 0;
  map<string, unsigned int>::iterator p;
  for (p = m_map_node_discovers.begin(); p != m_map_node_discovers.end(); p++)
  {
    unsigned int discovers = p->second;
    if (discovers > highest_discover_count)
      highest_discover_count = discovers;
  }

  // Part 3: Calc vector of vnames having highest discover count
  vector<string> leader_vnames;
  for (p = m_map_node_discovers.begin(); p != m_map_node_discovers.end(); p++)
  {
    string vname = p->first;
    unsigned int discovers = p->second;
    if (discovers == highest_discover_count)
      leader_vnames.push_back(vname);
  }

  // Part 4: Set the new leader or update leader to tie status
  if (leader_vnames.size() == 1)
    m_vname_leader = leader_vnames[0];
  else
    m_vname_leader = "tie";

  // Part 5: If no change, we're done. Otherwise make postings
  if (m_vname_leader == prev_leader)
    return;

  Notify("UFRM_LEADER", m_vname_leader);
  postFlags(m_leader_flags);
}

//------------------------------------------------------------
// Procedure: updateWinnerStatus()

void FireSim::updateWinnerStatus(bool finished)
{
  // Once a winner always a winner
  if (m_vname_winner != "")
    return;

  // Part 2: Determine the threshold for winning
  unsigned int known_fire_cnt = m_fireset.getKnownFireCnt();
  double win_thresh = (double)(known_fire_cnt) / 2;

  // Part 3: Calc vector of vnames having reached the win threshold
  // Possibly >1 winner for now. Will handle tie-breaker afterwards.
  vector<string> winner_vnames;
  map<string, unsigned int>::iterator p;
  for (p = m_map_node_discovers.begin(); p != m_map_node_discovers.end(); p++)
  {
    string vname = p->first;
    unsigned int discovers = p->second;
    if (discovers >= win_thresh)
      winner_vnames.push_back(vname);
  }

  // Part 4: If no winners then done for now
  if (winner_vnames.size() == 0)
  {
    Notify("UFRM_WINNER", "pending");
    return;
  }

  string would_be_winner;

  // Part 5: If one winner, set the winner
  if (winner_vnames.size() == 1)
    would_be_winner = winner_vnames[0];

  // Part 6: If multiple vnames meeting win threshold, do tiebreaker
  if (winner_vnames.size() > 1)
  {
    string first_winner;
    double first_winner_utc = 0;
    map<string, double>::iterator q;
    for (q = m_map_node_last_discover_utc.begin();
         q != m_map_node_last_discover_utc.end(); q++)
    {
      string vname = q->first;
      double utc = q->second;
      if ((first_winner == "") || (utc < first_winner_utc))
      {
        first_winner = vname;
        first_winner_utc = utc;
      }
    }
    would_be_winner = first_winner;
  }

  // If this competition has scout vehicles, and we're not yet
  // finished, then hold off on declaring a winner.
  if (m_scouts_inplay && !finished && !m_finish_upon_win)
    return;

  m_vname_winner = would_be_winner;
  Notify("UFRM_WINNER", m_vname_winner);
  postFlags(m_winner_flags);
}

//------------------------------------------------------------
// Procedure: updateFinishStatus()
//   Purpose: Completion is when all KNOWN fires have been
//            discovered. A fire is known if either (a) it is
//            a registered fire known at the outset, or (b)
//            it has been scouted.
//      Note: It is possible that after completion, further
//            vehicles could become scouted, but scouting will
//            be disabled, once the complete state has been
//            reached.

void FireSim::updateFinishStatus()
{
  // Once we are finished, we are always finished
  if (m_finished)
    return;

  set<string> fnames = m_fireset.getFireNames();
  if (fnames.size() == 0)
    return;

  // Assume finished is true until we find undiscovered fire
  m_known_undiscovered = 0;
  set<string>::iterator p;
  for (p = fnames.begin(); p != fnames.end(); p++)
  {
    string fname = *p;
    Fire fire = m_fireset.getFire(fname);
    bool fire_is_known = false;

    if (fire.getType() == "reg")
      fire_is_known = true;
    else
    {
      if (fire.hasBeenScouted())
        fire_is_known = true;
    }

    if (fire_is_known && (fire.getState() != "discovered"))
      m_known_undiscovered++;
  }

  bool finished = false;
  // First and most general criteria for finishing is when all
  // known fires have been discovered
  if (m_known_undiscovered == 0)
    finished = true;

  // Second criteria if no scouts in play, and a majority has been
  // discovered by one team (winner declared), AND finish_upon_win is
  // set to true, then we can finish.
  if (m_finish_upon_win && (m_vname_winner != "") && !m_scouts_inplay)
    finished = true;
  if (!finished)
    return;

  m_finished = true;
  Notify("UFRM_FINISHED", boolToString(m_finished));
  postFlags(m_finish_flags);

  updateWinnerStatus(true);
}

//------------------------------------------------------------
// Procedure: rollDice()
//
//
// 1.0 ^       sensor_rng_min       sensor_rng_max
//     |
//     |            |                 |
// Pd  |------------o                 |
//     |            |  \              |
//     |            |     \           |
//     |            |        \        |
//     |            |           \     |
//     |            |              \  |
//     o------------------------------o----------------------------->
//         range from fire to ownship
//

bool FireSim::rollDice(string vname, string fname, string dtype)
{
  // Part 1: Sanity checking
  if (!m_fireset.hasFire(fname))
    return (false);
  if (m_map_node_records.count(vname) == 0)
    return (false);

  Fire fire = m_fireset.getFire(fname);

  // Part 2: Calculated the range to fire
  double vx = m_map_node_records[vname].getX();
  double vy = m_map_node_records[vname].getY();
  double fx = fire.getCurrX();
  double fy = fire.getCurrY();
  double range_to_fire = hypot((vx - fx), (vy - fy));

  // Part 3: Calculate Pd threshold modified by range to fire
  int rand_int = rand() % 10000;
  double dice_roll = (double)(rand_int) / 10000;

  double pd = m_detect_rng_pd;
  if (range_to_fire >= m_detect_rng_max)
    pd = 0;
  else if (range_to_fire >= m_detect_rng_min)
  {
    double pct = m_detect_rng_max - range_to_fire;
    pct = pct / (m_detect_rng_max - m_detect_rng_min);
    pd = pct * pd;
  }

  if (range_to_fire <= m_detect_rng_max)
  {
    if (dtype == "discover")
      fire.incDiscoverTries();
    else if (dtype == "scout")
      fire.incScoutTries();
  }
  m_fireset.modFire(fire);

  // Apply the dice role to the Pd
  if (dice_roll >= pd)
    return (false);

  return (true);
}

//---------------------------------------------------------
// Procedure: handleMailDiscoverRequest()
//   Example: vname=alpha

bool FireSim::handleMailDiscoverRequest(string request)
{
  string vname = tokStringParse(request, "vname");

  if (vname == "")
    return (reportRunWarning("Discover request with no vname"));

  // Ensure this vname has not previously been a scout vehicle
  if (m_map_node_vroles.count(vname))
  {
    if (m_map_node_vroles[vname] != "discover")
      return (reportRunWarning(vname + " is discover double-agent"));
  }
  else
  {
    m_map_node_vroles[vname] = "discover";
    m_total_discoverers++;
  }

  m_map_node_discover_reqs[vname]++;
  m_map_node_last_discover_req[vname] = MOOSTime();
  return (true);
}

//---------------------------------------------------------
// Procedure: handleMailScoutRequest()
//   Example: vname=cal, tmate=abe

bool FireSim::handleMailScoutRequest(string request)
{
  string vname = tokStringParse(request, "vname");
  string tmate = tokStringParse(request, "tmate");

  // Sanity Check 1: check for empty vname or tname
  if (vname == "")
    return (reportRunWarning("Scout request with no vname"));
  if (tmate == "")
    return (reportRunWarning("Scout request with no teammate name"));

  m_scouts_inplay = true;
  m_finish_upon_win = true;

  // Sanity Check 2: check vname has not before been a discover vehicle
  if (m_map_node_vroles.count(vname))
  {
    if (m_map_node_vroles[vname] != "scout")
      return (reportRunWarning(vname + " is scout double-agent"));
  }
  else
    m_map_node_vroles[vname] = "scout";

  // Sanity Check 3: check vname has had different teammate before
  if (m_map_node_tmate.count(vname))
  {
    if (m_map_node_tmate[vname] != tmate)
      return (reportRunWarning(vname + " is disloyal scout"));
  }
  else
    m_map_node_tmate[vname] = tmate;

  m_map_node_scout_reqs[vname]++;
  m_map_node_last_scout_req[vname] = MOOSTime();
  return (true);
}

//------------------------------------------------------------
// Procedure: declareDiscoveredFire()
//     Notes: Example postings:
//            DISCOVERED_FIRE = id=f1, finder=abe
//            FOUND_FIRE = id=f1, finder=abe   (deprecated)

void FireSim::declareDiscoveredFire(string vname, string fname)
{
  // Part 1: Sanity check
  if (!m_fireset.hasFire(fname))
    return;

  // Part 2: Update the notables data structures to support calc
  // of leader differentials
  addNotable(vname, fname);

  // Part 3: Update the fire status, mark the discoverer. Note the
  // check for fire being not yet discovered was done earlier
  Fire fire = m_fireset.getFire(fname);
  fire.setState("discovered");
  fire.setDiscoverer(vname);
  m_fireset.modFire(fire);

  // Part 4: Update the discover stats for this vehicle
  m_map_node_discovers[vname]++;
  m_map_node_last_discover_utc[vname] = m_curr_time;

  // Part 5: Update the leader, winner and finish status
  updateLeaderStatus();
  updateWinnerStatus();
  updateFinishStatus();

  // Part 6: Generate postings, visuals and events
  reportEvent("Fire " + fname + " has been discovered by " + vname + "!");

  postFireMarkers();

  string idstr = m_fireset.getFire(fname).getID();
  idstr = findReplace(idstr, "id", "");
  string msg = "id=" + idstr + ", finder=" + vname;
  Notify("DISCOVERED_FIRE", msg);
  Notify("FOUND_FIRE", msg); // (deprecated)
}

//------------------------------------------------------------
// Procedure: declareScoutedFire()
//     Notes: Example postings:
//            SCOUTED_FIRE_AB = id=f1, x=23, y=34

void FireSim::declareScoutedFire(string vname, string fname)
{
  if (!m_fireset.hasFire(fname))
    return;

  Fire fire = m_fireset.getFire(fname);
  if (fire.getType() != "unreg")
    return;
  if (fire.hasBeenScouted(vname))
    return;

  fire.addScouted(vname);
  m_fireset.modFire(fire);

  m_map_node_scouts[vname]++;
  reportEvent("Fire " + fname + " has been scouted by " + vname + "!");

  postFireMarkers();

  string idstr = m_fireset.getFire(fname).getID();
  idstr = findReplace(idstr, "id", "");

  string msg = "id=" + idstr;
  msg += ", x=" + doubleToString(fire.getCurrX(), 2);
  msg += ", y=" + doubleToString(fire.getCurrY(), 2);
  Notify("SCOUTED_FIRE_" + toupper(vname), msg);
}

//------------------------------------------------------------
// Procedure: postDetectRngPolys()

void FireSim::postDetectRngPolys()
{
  if (!m_detect_rng_show)
    return;

  map<string, double>::iterator p;
  for (p = m_map_node_last_discover_req.begin();
       p != m_map_node_last_discover_req.end(); p++)
  {

    string vname = p->first;
    double last_req = p->second;
    double elapsed = m_curr_time - last_req;
    if (elapsed < 3)
      postRangePolys(vname, "discover", true);
    else
      postRangePolys(vname, "discover", false);
  }
}

//------------------------------------------------------------
// Procedure: postScoutRngPolys()

void FireSim::postScoutRngPolys()
{
  if (!m_detect_rng_show)
    return;

  map<string, double>::iterator p;
  for (p = m_map_node_last_scout_req.begin();
       p != m_map_node_last_scout_req.end(); p++)
  {

    string vname = p->first;
    double last_req = p->second;
    double elapsed = m_curr_time - last_req;
    if (elapsed < 3)
      postRangePolys(vname, "scout", true);
    else
      postRangePolys(vname, "scout", false);
  }
}

//------------------------------------------------------------
// Procedure: postRangePolys()

void FireSim::postRangePolys(string vname, string tag, bool active)
{
  if (m_map_node_records.count(vname) == 0)
    return;

  double x = m_map_node_records[vname].getX();
  double y = m_map_node_records[vname].getY();

  XYCircle circ(x, y, m_detect_rng_max);
  circ.set_label("sensor_max_" + tag + "_" + vname);
  circ.set_active(active);
  circ.set_vertex_color("off");
  circ.set_label_color("off");
  circ.set_edge_color("off");
  circ.set_color("fill", "white");
  circ.set_transparency(m_detect_rng_transparency);

  string spec1 = circ.get_spec();
  Notify("VIEW_CIRCLE", spec1);

  circ.set_label("sensor_min_" + tag + "_" + vname);
  circ.setRad(m_detect_rng_min);
  string spec2 = circ.get_spec();
  Notify("VIEW_CIRCLE", spec2);
}

//------------------------------------------------------------
// Procedure: broadcastFires()
//   Example: FIRE_ALERT = x=34, y=85, id=21

void FireSim::broadcastFires()
{
  map<string, NodeRecord>::const_iterator p;
  for (p = m_map_node_records.begin(); p != m_map_node_records.end(); p++)
  {
    string vname = p->first;
    string var = "FIRE_ALERT_" + toupper(vname);

    set<string> fires = m_fireset.getFireNames();
    set<string>::const_iterator q;
    for (q = fires.begin(); q != fires.end(); q++)
    {
      string fname = *q;
      Fire fire = m_fireset.getFire(fname);
      string ftype = fire.getType();
      if (ftype == "reg")
      {
        string id_str = fire.getID();
        id_str = findReplace(id_str, "id", ""); // convert "id23" ot "23"
        string msg = "x=" + doubleToStringX(fire.getCurrX(), 1);
        msg += ", y=" + doubleToStringX(fire.getCurrY(), 1);
        msg += ", id=" + id_str;
        Notify(var, msg);
      }
    }
  }
}

//------------------------------------------------------------
// Procedure: applyTMateColors()

void FireSim::applyTMateColors()
{
  map<string, string>::iterator p;
  for (p = m_map_node_tmate.begin(); p != m_map_node_tmate.end(); p++)
  {
    string scout_vname = p->first;
    string discover_vname = p->second;

    if (m_map_node_records.count(discover_vname))
    {
      string discover_color = m_map_node_records[discover_vname].getColor();
      Notify("NODE_COLOR_CHANGE_" + toupper(scout_vname), discover_color);
    }
  }
}

//------------------------------------------------------------
// Procedure: postFireMarkers()

void FireSim::postFireMarkers()
{
  set<string> fire_names = m_fireset.getFireNames();
  set<string>::iterator p;
  for (p = fire_names.begin(); p != fire_names.end(); p++)
  {
    string fname = *p;
    postFireMarker(fname);
  }

  XYPolygon poly = m_fireset.getSearchRegion();
  if (poly.is_convex())
  {
    Notify("VIEW_POLYGON", poly.get_spec());
    Notify("SEARCH_REGION", poly.get_spec());
  }
}

//------------------------------------------------------------
// Procedure: postFireMarker()

void FireSim::postFireMarker(string fname)
{
  if (!m_fireset.hasFire(fname))
    return;

  Fire fire = m_fireset.getFire(fname);
  string ftype = fire.getType();
  string discoverer = fire.getDiscoverer();

  bool notable = isNotable(fname);

  XYMarker marker;
  marker.set_label(fname);
  marker.set_type("triangle");
  marker.set_vx(fire.getCurrX());
  marker.set_vy(fire.getCurrY());
  marker.set_width(3);
  marker.set_edge_color("green");
  marker.set_transparency(0.3);

  if ((ftype == "reg") || (discoverer != ""))
  {
    string marker_color = m_fire_color;
    string discoverer = fire.getDiscoverer();
    if ((discoverer != "") && (discoverer != "nature"))
    {
      marker_color = m_map_node_records[discoverer].getColor();
      if (!notable)
        marker.set_transparency(0.7);
      else
        marker.set_transparency(0.1);
    }
    if (discoverer == "nature")
      marker.set_color("primary_color", "gray50");
    else
      marker.set_color("primary_color", marker_color);
  }
  else
  {
    string color1 = "gray50";
    string color2 = "gray50";
    set<string> scouts = fire.getScoutSet();
    if ((scouts.size() == 1) || (scouts.size() == 2))
    {
      set<string>::iterator p;
      for (p = scouts.begin(); p != scouts.end(); p++)
      {
        string scout = *p;
        string scout_tmate = m_map_node_tmate[scout];
        string color = m_map_node_records[scout_tmate].getColor();
        if (isColor(color) && (color1 == "gray50"))
          color1 = color;
        else if (isColor(color) && (color2 == "gray50"))
          color2 = color;
      }
    }
    marker.set_type("efield");
    marker.set_color("primary_color", color1);
    marker.set_color("secondary_color", color2);

#if 0
    marker.set_type("circle");
    marker.set_color("primary_color", "gray50");
#endif
  }

  Notify("VIEW_MARKER", marker.get_spec());
}

//------------------------------------------------------------
// Procedure: postFlags()

void FireSim::postFlags(const vector<VarDataPair> &flags)
{
  for (unsigned int i = 0; i < flags.size(); i++)
  {
    VarDataPair pair = flags[i];
    string moosvar = pair.get_var();

    // If posting is a double, just post. No macro expansion
    if (!pair.is_string())
    {
      double dval = pair.get_ddata();
      Notify(moosvar, dval);
    }
    // Otherwise if string posting, handle macro expansion
    else
    {
      string sval = pair.get_sdata();
      sval = macroExpand(sval, "LEADER", m_vname_leader);
      sval = macroExpand(sval, "WINNER", m_vname_winner);

      Notify(moosvar, sval);
    }
  }
}

//------------------------------------------------------------
// Procedure: addNotable()
//   Purpose: The notables map is where we keep track of (a)
//            the most recent fires discovered for each vehicle
//            When we have data for all vehicles, we use this
//            map to pop-off equal amounts of fires for each
//            vehicle until some vehicle is an empty list. This
//            way the remaining fires are the "notable" once
//            since they represent the most recent fires that
//            provide the leading vehicle with the lead.

void FireSim::addNotable(string vname, string fname)
{
  if (vname == "nature")
    return;

#if 0
  string ftype = m_fireset.getFire(fname).getType();
  if(m_fireset.getFire(fname).getType() == "person") 
    m_map_notables[vname].push_front(fname);
  else
    return;
#endif
#if 1
  m_map_notables[vname].push_front(fname);
#endif

  bool some_empty = false;
  map<string, list<string>>::iterator p;
  for (p = m_map_notables.begin(); p != m_map_notables.end(); p++)
  {
    if (p->second.size() == 0)
      some_empty = true;
  }

  if (some_empty)
    return;
  if (m_map_notables.size() < m_total_discoverers)
    return;
  if (m_map_notables.size() == 1)
    return;

  for (p = m_map_notables.begin(); p != m_map_notables.end(); p++)
    p->second.pop_back();
}

//------------------------------------------------------------
// Procedure: isNotable()

bool FireSim::isNotable(string fname)
{
  map<string, list<string>>::iterator p;
  for (p = m_map_notables.begin(); p != m_map_notables.end(); p++)
  {
    if (listContains(p->second, fname))
      return (true);
  }

  return (false);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool FireSim::buildReport()
{
  string str_rng_min = doubleToStringX(m_detect_rng_min, 1);
  string str_rng_max = doubleToStringX(m_detect_rng_max, 1);
  string str_rng_pd = doubleToStringX(m_detect_rng_pd, 2);
  string str_trans = doubleToString(m_detect_rng_transparency, 2);

  m_msgs << "======================================" << endl;
  m_msgs << "FireSim Configuration " << endl;
  m_msgs << "======================================" << endl;
  m_msgs << "detect_rng_min: " << str_rng_min << endl;
  m_msgs << "detect_rng_max: " << str_rng_max << endl;
  m_msgs << "detect_rng_pd:  " << str_rng_pd << endl;
  m_msgs << "detect_rng_show: " << boolToString(m_detect_rng_show) << endl;
  m_msgs << "transparency:   " << str_trans << endl;
  m_msgs << "fire_file:      " << m_fireset.getFireFile() << endl;
  m_msgs << endl;

  m_msgs << "======================================" << endl;
  m_msgs << "Vehicle Discover Summary " << endl;
  m_msgs << "======================================" << endl;

  ACTable actab = ACTable(7);
  actab << "Vehi | Discover | Discover | Fires       | Scout | Scout | Fires";
  actab << "Name | Reqs     | Tries    | Discovered  | Reqs  | Tries | Scouted ";
  actab.addHeaderLines();

  string finished_str = boolToString(m_finished);
  finished_str += " (" + uintToString(m_known_undiscovered) + " remaining)";

  m_msgs << "Total vehicles: " << m_map_node_records.size() << endl;
  m_msgs << "Leader vehicle: " << m_vname_leader << endl;
  m_msgs << "Winner vehicle: " << m_vname_winner << endl;
  m_msgs << "Mission Finished: " << finished_str << endl;
  m_msgs << endl;

  map<string, NodeRecord>::iterator q;
  for (q = m_map_node_records.begin(); q != m_map_node_records.end(); q++)
  {
    string vname = q->first;
    string ds_reqs = uintToString(m_map_node_discover_reqs[vname]);
    string ds_tries = uintToString(m_map_node_discover_tries[vname]);
    string discovers = uintToString(m_map_node_discovers[vname]);
    string sc_reqs = uintToString(m_map_node_scout_reqs[vname]);
    string sc_tries = uintToString(m_map_node_scout_tries[vname]);
    string scouts = uintToString(m_map_node_scouts[vname]);
    actab << vname << ds_reqs << ds_tries << discovers;
    actab << sc_reqs << sc_tries << scouts;
  }
  m_msgs << actab.getFormattedString();

  m_msgs << endl
         << endl;
  m_msgs << "======================================" << endl;
  m_msgs << "Fire Summary " << endl;
  m_msgs << "======================================" << endl;
  actab = ACTable(9);
  actab << "Name | ID | Type | Pos| State | Discoverer| Tries | Scouts | Time ";
  actab.addHeaderLines();

  set<string> fire_names = m_fireset.getFireNames();

  set<string>::iterator p;
  for (p = fire_names.begin(); p != fire_names.end(); p++)
  {
    string fname = *p;
    Fire fire = m_fireset.getFire(fname);
    string id = fire.getID();
    string ftype = fire.getType();
    string state = fire.getState();
    string tries = uintToString(fire.getDiscoverTries());

    double xpos = fire.getCurrX();
    double ypos = fire.getCurrY();
    string pos = doubleToStringX(xpos, 0) + "," + doubleToStringX(ypos, 0);

    string discoverer = "-";
    double duration = m_curr_time - fire.getTimeEnter();
    if (state == "discovered")
    {
      duration = fire.getTimeDiscovered();
      discoverer = fire.getDiscoverer();
    }

    set<string> scout_set = fire.getScoutSet();
    string scouts = stringSetToString(scout_set);

    string dur_str = doubleToStringX(duration, 1);

    actab << fname << id << ftype << pos << state;
    actab << discoverer << tries << scouts << dur_str;
  }
  m_msgs << actab.getFormattedString();
  m_msgs << endl
         << endl;

  return (true);
}
