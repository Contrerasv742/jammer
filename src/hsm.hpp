#pragma once
#include <cstdint>

// Forward declaration so the header stays lightweight.
class Display;

enum class State : uint8_t {
  /* top-level state */
  IDLE = 0,

  /* Scanning */
  SCANNING, // superstate
  MOVE,
  SCAN,
  COMPARE,

  /* Jamming */
  JAMMING, // superstate
  JAM,

  /* Signal Processing */
  SIGNAL_PROCESSING, // superstate
  FILTER_NOISE,
  DEMODULATE,
  DECODE,

  COUNT,      // number of real states (keep last of the reals)
  NONE = 0xFF // sentinel for "no parent / no substate"
};

/* @brief Bind the state machine to your Display instance.
 * @param display
 * */
void hsm_init(Display &display);

/* @brief Run exactly one leaf: draw it, wait, then advance to the
 * next state. Call this repeatedly from your main loop.
 * */
void hsm_run();
