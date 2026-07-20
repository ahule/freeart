#pragma once

#define FREEART_VERSION_MAJOR 1
#define FREEART_VERSION_MINOR 0
#define FREEART_VERSION_PATCH 0

#define FREEART_VERSION_STRING "1.00"

#define FREEART_MAKE_VERSION(major, minor, patch)                              \
  ((major) * 10000 + (minor) * 100 + (patch))

#define FREEART_VERSION                                                        \
  FREEART_MAKE_VERSION(FREEART_VERSION_MAJOR, FREEART_VERSION_MINOR,           \
                       FREEART_VERSION_PATCH)
