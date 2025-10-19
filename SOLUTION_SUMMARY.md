# TinyXML2 Build Error Fix - Complete Solution

## Problem
The build was failing with the following error:
```
error: patch failed: CMakeLists.txt:1
error: CMakeLists.txt: patch does not apply
error: patch failed: cmake/tinyxml2-config.cmake:1
error: cmake/tinyxml2-config.cmake: patch does not apply
```

This occurred when building the tinyxml2 dependency in the MAVSDK submodule.

## Root Cause
The MAVSDK fork was using two separate patch files (`cmake-3.10.2.patch` and `no-lfs64.patch`) that were created for an older version of tinyxml2. These patches expected tinyxml2 with `cmake_minimum_required(VERSION 3.5)`, but version 9.0.0 uses `cmake_minimum_required(VERSION 3.15)`.

Additionally, applying the patches sequentially caused conflicts because the second patch (`no-lfs64.patch`) tried to modify CMakeLists.txt which had already been changed by the first patch.

## Solution
Created a single combined patch file (`combined.patch`) that:
1. Works correctly with tinyxml2 version 9.0.0
2. Includes all changes from both original patches
3. Can be applied in a single step without conflicts

### Changes Made

**In MAVSDK submodule (`MAVSDK/third_party/tinyxml2/`):**
1. Deleted: `cmake-3.10.2.patch` and `no-lfs64.patch`
2. Created: `combined.patch` with all necessary modifications for tinyxml2 9.0.0
3. Updated: `CMakeLists.txt` to apply only the combined patch

**Patch Contents:**
The combined patch includes:
- Downgrade `cmake_minimum_required` from VERSION 3.15 to VERSION 3.5 (for Ubuntu 18.04 support)
- Add conditional logic for `NAMELINK_COMPONENT` (requires CMake >= 3.12)
- Add conditional logic for `TYPE INCLUDE` (requires CMake > 3.13.5)
- Add conditional logic for pkg-config generation (requires CMake > 3.14.7)
- Add `_FILE_OFFSET_BITS=64` definition (except for Android builds)
- Remove LFS64 functions (`fseeko64`/`ftello64`) for x86_64 Unix systems (fixes Musl compatibility)

## Repository Status

### Main Repository (moos-ivp-uav)
- Branch: `copilot/fix-build-error-tinyxml2`
- Submodule reference updated to point to commit `c848e73d` in MAVSDK

### MAVSDK Submodule
- Branch: `fix-tinyxml2-patches` (new branch created for this fix)
- Commit: `c848e73d8d0c3ed39dfca2875b5a8deac6b06aa4`
- Status: **Committed locally but NOT YET PUSHED to GitHub**

## Required Actions

### 1. Push MAVSDK Changes to Fork
You need to manually push the MAVSDK changes to your fork:

```bash
cd MAVSDK
git push origin fix-tinyxml2-patches
```

Then either:
- Create a PR to merge `fix-tinyxml2-patches` into `v2.12.6-Ardupilot`
- Or directly merge and push:
  ```bash
  git checkout v2.12.6-Ardupilot
  git merge fix-tinyxml2-patches
  git push origin v2.12.6-Ardupilot
  ```

### 2. Update Main Repository (if needed)
After pushing the MAVSDK changes, the main repository already has the updated submodule reference. If you merged into `v2.12.6-Ardupilot`, you might want to update the submodule reference:

```bash
cd ..  # back to main repository
cd MAVSDK
git checkout v2.12.6-Ardupilot
git pull
cd ..
git add MAVSDK
git commit -m "Update MAVSDK submodule to merged tinyxml2 fix"
```

### 3. Test the Build
After pushing the changes, test the build:

```bash
./build.sh
```

Or if you have specific build commands:
```bash
mkdir -p build
cd build
cmake ..
make
```

## Verification
To verify the patch works, you can test it manually:

```bash
cd /tmp
git clone https://github.com/leethomason/tinyxml2
cd tinyxml2
git checkout 9.0.0
git apply /path/to/MAVSDK/third_party/tinyxml2/combined.patch
# Should apply successfully with no errors
```

## Files Changed

### In MAVSDK Submodule:
- `third_party/tinyxml2/CMakeLists.txt` - Updated patch command
- `third_party/tinyxml2/combined.patch` - New combined patch file
- `third_party/tinyxml2/cmake-3.10.2.patch` - Deleted
- `third_party/tinyxml2/no-lfs64.patch` - Deleted

### In Main Repository:
- `MAVSDK` - Submodule reference updated to `c848e73d`
- `PUSH_MAVSDK_INSTRUCTIONS.md` - Instructions for pushing submodule changes
- `SOLUTION_SUMMARY.md` - This file

## Technical Details

The combined patch makes tinyxml2 9.0.0 compatible with older CMake versions (3.5+) by:

1. **CMake Version Compatibility**: Uses conditional logic based on CMake version to use features only when available
2. **LFS64 Removal**: Removes deprecated LFS64 interfaces that are no longer supported in modern Musl libc
3. **Build Configuration**: Adds `_FILE_OFFSET_BITS=64` to enable large file support without LFS64 functions

This ensures the code works on:
- Ubuntu 18.04 (CMake 3.10.2)
- Alpine Linux and other Musl-based systems
- Android (which requires LFS64, so it's excluded from the LFS64 removal)
- Modern systems with newer CMake versions
