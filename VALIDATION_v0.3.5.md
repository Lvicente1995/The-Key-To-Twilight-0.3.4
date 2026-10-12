# v0.3.5 validation

Static/source validation performed on the v0.3.4 reviewed baseline:
- version metadata is 0.3.5 in `mod.json`, CMake, README and startup log;
- v0.3.4 plain-text Dusklight description and full README description are preserved;
- all eight universal workflow target names remain present;
- local-player isolation is present in the Kingdom Key and Xion paths;
- remote `onExecute`/`beginFrame` calls no longer reset local replacement state;
- Kingdom Key shadow path guards `mModelNum == 0` and `mModelNum > 38`;
- no runtime assets were modified.

The full native eight-platform GitHub Actions build and live two-player Crests test were not run in this review environment.
