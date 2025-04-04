/*****************************************************************/
/*    NAME: Steve Nomeny                                         */
/*    ORGN: NTNU, Trondheim                                       */
/*    FILE: GridSearchPlanner_Info.cpp                                */
/*    DATE: Feb 2025                                            */
/*****************************************************************/

#include <cstdlib>
#include <iostream>
#include "GridSearchPlanner_Info.h"
#include "ColorParse.h"
#include "ReleaseInfo.h"

using namespace std;

//----------------------------------------------------------------
// Procedure: showSynopsis

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  The pGridSearchPlanner application  is a module for storing a ");
  blk("  history of vehicle positions in a 2D grid defined over a      ");
  blk("  region of operation and generating a search patter according  ");
  blk("  to the TMSTC* algorithm.                                      "); 
}

//----------------------------------------------------------------
// Procedure: showHelpAndExit

void showHelpAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("Usage: pGridSearchPlanner file.moos [OPTIONS]                          ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("Options:                                                        ");
  mag("  --alias", "=<ProcessName>                                      ");
  blk("      Launch pGridSearchPlanner with the given process name rather     ");
  blk("      than pGridSearchPlanner.                                          ");
  mag("  --example, -e                                                 ");
  blk("      Display example MOOS configuration block.                 ");
  mag("  --help, -h                                                    ");
  blk("      Display this help message.                                ");
  mag("  --interface, -i                                               ");
  blk("      Display MOOS publications and subscriptions.              ");
  mag("  --version,-v                                                  ");
  blk("      Display the release version of pGridSearchPlanner.               ");
  blk("                                                                ");
  blk("Note: If argv[2] does not otherwise match a known option,       ");
  blk("      then it will be interpreted as a run alias. This is       ");
  blk("      to support pAntler launching conventions.                 ");
  blk("                                                                ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showExampleConfigAndExit

void showExampleConfigAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("pGridSearchPlanner Example MOOS Configuration                          ");
  blu("=============================================================== ");
  blk("                                                                ");
  blk("ProcessConfig = pGridSearchPlanner                                     ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blk("  report_deltas = true         // default                       ");
  blk("  grid_var_name = VIEW_GRID    // default                       ");
  blk("  grid_label    = gsv          // default                       ");
  blk("  match_name    = abe                                           ");
  blk("  ignore_name   = ben                                           ");
  blk("                                                                ");
  blk("  search_region = pts={-50,-40: -10,0: 180,0: 180,-150: -50,-150} ");
  blk("}                                                               ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showInterfaceAndExit

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("pGridSearchPlanner INTERFACE                                           ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  NODE_REPORT                                                   ");
  blk("  NODE_REPORT_LOCAL = NAME=alpha,TYPE=UUV,TIME=1252348077.59,   ");
  blk("                      X=51.71,Y=-35.50, LAT=43.824981,          ");
  blk("                      LON=-70.329755,SPD=2.0,HDG=118.8,         ");
  blk("                      YAW=118.8,DEPTH=4.6,LENGTH=3.8,           ");
  blk("                      MODE=MODE@ACTIVE:LOITERING                ");
  blk("  GSV_GRID_RESET    = true                                      ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  VIEW_GRID = pts={-50,-40:-10,0:90,0:90,-90:50,-150:-50,-90},  ");
  blk("              cell_size=5, cell_vars=x:0:y:0:z:0,cell_min=x:0,  ");
  blk("              cell_max=x:50,cell=211:x:50, cell=212:x:50,       ");
  blk("              cell=237:x:50,cell=238:x:50,label=gsv             ");
  blk("                                                                ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showReleaseInfoAndExit

void showReleaseInfoAndExit()
{
  showReleaseInfo("pGridSearchPlanner", "gpl");
  exit(0);
}
