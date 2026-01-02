#pragma once

#include "Record.h"

#ifdef _WIN64
#include "RecordWindows.h"
#else
#include "RecordLinux.h"
#endif

inline Record *CreateRecorder() {
#ifdef _WIN64
  return new RecordWindows();
#else
  return new RecordLinux();
#endif
}