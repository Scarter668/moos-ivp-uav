# Instructions to Push MAVSDK Submodule Changes

The tinyxml2 patch fix has been committed to the MAVSDK submodule but needs to be pushed to the fork.

## Steps to Push the Changes:

1. Navigate to the MAVSDK submodule:
   ```bash
   cd MAVSDK
   ```

2. Verify you're on the correct branch:
   ```bash
   git branch
   # Should show: * fix-tinyxml2-patches
   ```

3. Check the commit:
   ```bash
   git log -1
   # Should show the commit: "Fix tinyxml2 patches for version 9.0.0"
   ```

4. Push the branch to your fork:
   ```bash
   git push origin fix-tinyxml2-patches
   ```

5. After pushing, you can either:
   - Create a PR to merge fix-tinyxml2-patches into v2.12.6-Ardupilot branch in your fork
   - Or directly update the v2.12.6-Ardupilot branch:
     ```bash
     git checkout v2.12.6-Ardupilot
     git merge fix-tinyxml2-patches
     git push origin v2.12.6-Ardupilot
     ```

6. After pushing to the MAVSDK fork, come back to the main repository:
   ```bash
   cd ..
   ```

7. Update the submodule reference if needed:
   ```bash
   git add MAVSDK
   git commit -m "Update MAVSDK submodule to include tinyxml2 patch fix"
   git push
   ```

## What Was Fixed:

The tinyxml2 patches (cmake-3.10.2.patch and no-lfs64.patch) were incompatible with tinyxml2 version 9.0.0 because:
- The patches expected `cmake_minimum_required(VERSION 3.5)` but v9.0.0 has `VERSION 3.15`
- Applying patches sequentially caused conflicts

The fix:
- Combined both patches into a single `combined.patch` file
- Updated the patch to work with tinyxml2 9.0.0's structure
- Modified MAVSDK/third_party/tinyxml2/CMakeLists.txt to apply the combined patch

## Current Commit in MAVSDK:
- Branch: fix-tinyxml2-patches
- Commit: c848e73d8d0c3ed39dfca2875b5a8deac6b06aa4
