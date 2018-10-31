---
layout: default
---

# Preview

![Example](BadApp.png)

## Why BadApp?
When developing tooling to detect / fix bad application behavior, it can be hard to find applications exposing this bad behavior in a controlled way.

Combining all kinds of common application bugs in a simple application ensures that testing the detections or fixes is as simple as pressing a button.

## Features


* Crashes:
    * Crash by calling a nullptr
    * Crash by reading from a nullptr
    * Crash by writing to a nullptr
    * Crash by causing a stack overflow
* Handle bugs:
    * Call SetEvent on an invalid handle
    * Call SetEvent on a NULL handle
    * WaitForMultipleObjects with no handles
    * Call SetEvent on a Semaphore
* Heap bugs:
    * Modify memory after it has been freed (Use after free)
    * Free memory twice (Double free)
    * Free allocation from another heap as it was allocated from
    * Write more data than was allocated (Buffer overflow)
* Diagnostics:
    * Relaunch BadApp as Admin
    * Reset global FTH ticket state
    * Enable WER for BadApp (Current user, and when possible, all users)
    * Disable WER for BadApp (Current user, and when possible, all users)
* Control the context the bugs will use:
    * Called directly from WndProc (Respond to button click)
    * Called from a message posted to the WndProc
    * Called from a new Win32 thread (CreateThread)
    * Called from a new Native thread (RtlCreateUserThread)
* Heuristics:
    * Detect some known applied mitigations (SHIMS)
    * Detect the current 'ticket' state for the FTH
    * Detect the application WER status (disabled for current user, for all users)
    * Detect ZoneID
