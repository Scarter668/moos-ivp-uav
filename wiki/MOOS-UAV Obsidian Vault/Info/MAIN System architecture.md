
# moos-ivp-uav


This repository consist of a submodule for #MAVSDK 


Clone the [repo](https://github.com/Scarter668/moos-ivp-uav.git) (with ssh) and the submodules:

```bash
git clone git@github.com:Scarter668/moos-ivp-uav.git;
cd moos-ivp-uav;
git submodule update --init --recursive
```


### Setup bash aliases

The file `moos-ivp-uav/scripts/setup_bash_aliases_moos.sh` defines and sets up some useful aliases. 
Make the file executable

```bash
chmod +x ~/moos-ivp-uav/scripts/setup_bash_aliases_moos.sh
```

and source it from your bashrc. 

## Install MOOS-IVP (Github)

This was moved from svn trunk found [here](https://oceanai.mit.edu/ivpman/pmwiki/pmwiki.php?n=Lab.ClassSetup#sec_course_software)


Download using`git`:

```bash
git clone https://github.com/moos-ivp/moos-ivp.git
```

Checkout and pull:

```bash
cd moos-ivp;
git pull
```


Then build moos:

```bash
./build.sh 
```
**PS**: Build with flag `-m` if building on vehicle

Verify that it is properly installed with:

```bash
which MOOSDB
which pHelmIvP 
```

It should produce:

	$ which MOOSDB
	/Users/you/moos-ivp/bin/MOOSDB
	$ which pHelmIvP 
	/Users/you/moos-ivp/bin/pHelmIvP

AND/OR run an example mission:
```
cd ./ivp/missions/s1_alpha;
pAntler --MOOSTimeWarp=10 alpha.moos
```

Add path to bin folder in moos-ivp-uav to bashrc: `PATH=$PATH:~/moos-ivp-uav/bin`

**NOTE**: When building moos-ivp, remember to use the `-m` flag to only build nongui apps: `.build.sh -m`


### MOOS IVP Editor for vscode
Installed the moos-ivp editor extension for vscode by following this [guide](https://msis.github.io/2680notes/editors/vscode/10%20-%20Setting%20things%20up/#install-recommended-extensions):

Install the file [moos-ivp-editor-0.2.0.vsix](https://github.com/msis/2680notes/blob/main/pages/editors/moos-ivp-editor-0.2.0.vsix) and run command:

```bash 
code --install-extension moos-ivp-editor-0.2.0.vsix
```

in your folder where you have the project.


## Install MOOS IVP SWARM Toolbox


The codebase is private on github own by pavlab-MIT and needs access to use or clone.

Contact `scnomeny@mit.edu` for access.

**Note:** If you don't hear back with a reasonable time, email `mail@scnomeny.com`

After the code is downloaded, checkout and build the library:
```shell
cd moos-ivp-swarm
./build.sh
```
**PS**: Build with flag `-m` if building on vehicle

*Note:* If bugs appear during the build process, fix them in the code base before retrying the build. More info in  [[Troubleshooting]]

Update your environmental variables, `PATH` and `IVP_BEHAVIOR_DIRS`, in your `.bashrc` file.
```shell
IVP_BEHAVIOR_DIRS=$IVP_BEHAVIOR_DIRS:~/moos-ivp-swarm/lib
export IVP_BEHAVIOR_DIRS

PATH=$PATH:~/moos-ivp-swarm/bin
export PATH
```


Verify that it is properly installed with:

```bash
which pMediator
```

**Output:** /Users/you/moos-ivp-swarm/bin/pMediator


---




# Other installed Repositories:

## ArduPilot (to run arduplane) with MavProxy
- ~/ardupilot/
- [Install guide](https://ardupilot.org/dev/docs/building-setup-linux.html#building-setup-linux)
	- Run `git submodule update --init --recursive`
	- Run `install-prereqs-ubuntu.sh` before switching branch! More in [[Troubleshooting]]
	- **CHECKOUT** version `Plane-4.5.7` (branch)
- [More info](ArduPilot%20&%20MavProx.md)
## ardupilot_gazebo (*gazebo Ionic*)
- Install the [Gazebo Ionic](https://gazebosim.org/docs/ionic/install_ubuntu/) simulator binary:
	- Test that it runs `gz sim -v4 -r shapes.sdf`
- In folder: ~/gz_ws/ardupilot_gazebo/
- Modified installation to match with Ionic version can be found in  [[ardupilot_gazebo plugin]]
- Install the SITL_Models from cloning repo into home
```bash 
git clone git@github.com:ArduPilot/ardupilot_gazebo.git
```
And add to bashrc:
```bash
export GZ_SIM_RESOURCE_PATH=$GZ_SIM_RESOURCE_PATH:\
$HOME/SITL_Models/Gazebo/models:\
$HOME/SITL_Models/Gazebo/worlds:\
$HOME/moos-ivp-uav/GazeboSim/models:\
$HOME/moos-ivp-uav/GazeboSim/worlds:
```
More on [[Gazebo]]


**IMPORTANT:** Before using the plugin, remember to build it by calling cmake in the build folder and then make. 

## MissionPLanner
- ~/Mission_Planner/
- [Install guide](https://ardupilot.org/planner/docs/mission-planner-installation.html)
- [[Mission Planner info]]
## MAVSDK 

**No need to install as it comes as a submodule with moos-ivp-uav)**
- ~/MAVSDK/
- [[Install, Build & Run MAVSDK]]



---
Relevant topics: [[Troubleshooting]]
