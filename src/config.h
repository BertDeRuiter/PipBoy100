#pragma once

#define SQRT_MAX_STEPS 40
//Lose 45% of XP
#define XP_LOSS 0.55

#define MAX_CRIPPLED 8

//Every 10 minute (9+1) reset the total acceleration used to calculate
//the max xp amount given to the player.
#define RESET_TOTAL_MIN 9

#define ABS(i)( i>0? i: i*-1)

#define DIVERG(a,b) (a>b? a-b: b-a)

typedef enum {
  PIPEXP = 0,
  PIPE_LAST_XP,
  PIPE_LAST_GAIN,
  PIPE_CURRENT_CRIPPLED
} PipePersistance;
		
const VibePattern BLUETOOTH_DISCONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100, 85, 100},
  .num_segments = 5
};

const VibePattern BLUETOOTH_CONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100},
  .num_segments = 3
};
