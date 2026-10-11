# BadApp

[![Build status](https://github.com/learn-more/BadApp/actions/workflows/msbuild.yml/badge.svg?branch=master)](https://github.com/learn-more/BadApp/actions/workflows/msbuild.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://spdx.org/licenses/MIT)
![GitHub release (latest SemVer including pre-releases)](https://img.shields.io/github/v/release/learn-more/BadApp?include_prereleases&sort=semver)
![Works on Windows XP](https://img.shields.io/badge/Works%20on-Windows%20XP-blue)


## Test application exposing bad application behavior.

When developing tooling to detect / fix bad application behavior, it can be hard to find applications exposing this bad behavior in a controlled way.

Combining all kinds of common application bugs in a simple application ensures that testing the detections or fixes is as simple as pressing a button.

See https://learn-more.github.io/BadApp/ for an overview of the features.

![Layout](docs/BadApp.png)

## Features

* Crashes: call / read / write a nullptr, stack overflow, assert abort
* Critical sections: double init / release, free without delete, delete a held section, use without init, VirtualFree, private PEB lock
* Handles: SetEvent on invalid / NULL / wrong handles, WaitForMultipleObjects with no handles
* Heap: use after free, double free, free from the wrong heap, buffer overflow, leak
* Threads: handle leaks, TerminateThread, abandoned mutex, self deadlock, lock order inversion, crash in a worker thread
* Diagnostics: relaunch as admin, reset FTH state, enable/disable WER, OS version, list modules
* Heuristics: SHIMS, FTH tickets, WER status, ZoneID, GlobalFlag

Every bug can also be triggered in a different context via the dropdown at the bottom right
(direct call, posted message, CreateThread, RtlCreateUserThread, dialog init).

## Building

* Visual Studio 2017 with the Windows XP toolset (`v141_xp`) / Windows 5.1 SDK, or Visual Studio 2022 with the
  `Microsoft.VisualStudio.Component.WinXP` component (that is what the GitHub Actions workflows install).
* Open `BadApp.sln` in Visual Studio, or run:
  `msbuild BadApp.sln /p:Configuration=Release /p:Platform=x86`
* `GenVersion.bat` runs as a pre-build event and generates `generated_git_version.h` from `git describe --tags`.
  It falls back to version `0.0.0.0` when there is no git or no tags (e.g. building from a source zip), so the
  build still succeeds.
