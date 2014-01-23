#pragma once

#define SQRT_MAX_STEPS 40
//Lose 45% of XP when dying
#define XP_LOSS 0.55

#define MAX_CRIPPLED 8

//Every 20 minutes (RESET_TOTAL_MIN+1) reset the total acceleration used to calculate
//the max xp amount given to the player.
//And check if the player lose or heal a limb.
#define RESET_TOTAL_MIN 19

#define ABS(i)( i>0? i: i*-1)

#define DIVERG(a,b) (a>b? a-b: b-a)

//If the user didn't make a mouvement that have a difference of MIN_MOVMNT
//the watchface consider that he can't get XP for that mouvement.
#define MIN_MOVMNT 45

typedef enum {
  PIPEXP = 0,
  PIPE_LAST_XP,
  PIPE_LAST_GAIN,
  PIPE_CURRENT_CRIPPLED,
  PIPE_TOTAL
} PipePersistance;
		
typedef struct  {
	int16_t x ;
	int16_t y ;
	int16_t z ;
	uint8_t total;
	int16_t lastX ;
	int16_t lastY ;
	int16_t lastZ ;
} AccelTotal;
const VibePattern BLUETOOTH_DISCONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100, 85, 100},
  .num_segments = 5
};

const VibePattern BLUETOOTH_CONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100},
  .num_segments = 3
};
