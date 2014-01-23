//Copyright 2013 Bert de Ruiter (www.bertderuiter.nl/)

#include "pebble.h"
#include "config.h"



static Window *window; 
static TextLayer *time_layer; 
static TextLayer *date_layer;
static TextLayer *battery_layer;
static TextLayer *xp_layer;
static TextLayer *nextLvl_layer;
static TextLayer *lvl_layer;

static BitmapLayer *image_layer;
static BitmapLayer *vaultBoy_layer;
static uint32_t xp_counter = 0;
static uint32_t xp_needed;
static uint8_t xp_multiplier;
static uint32_t lvl_counter;
static uint32_t lastXp = 0;
static uint32_t lastGain = 0;

static AccelTotal totalAccel = {0,0,0,0,0,0,0};
	
static GBitmap *image;
static GBitmap *vaultBoy;
static uint8_t currentVaultBoy = RESOURCE_ID_VAULT_BOY;

static uint8_t loadedImage = 0;
static bool dead = false;


//X = MULT * L * L - MULT * L
static int getXpForNextLvl() {
	int nextLvl = lvl_counter + 1;
	return xp_multiplier * nextLvl * nextLvl  - xp_multiplier * nextLvl;
}

float my_sqrt(float num) {
  float a, p, e = 0.001, b;
  a = num;
  p = a * a;
  int nb = 0;
  while ((p - num >= e) && (nb++ < SQRT_MAX_STEPS)) {
    b = (a + (num / a)) / 2;
    a = b;
    p = a * a;
  }
  return a;
}
static bool canGainXP(AccelData *accel) {
	int16_t lastX = totalAccel.lastX;
	int16_t lastY = totalAccel.lastY;
	int16_t lastZ = totalAccel.lastZ;
	totalAccel.lastX = accel->x;
	totalAccel.lastY = accel->y;
	totalAccel.lastZ = accel->z;
	totalAccel.total++;
	APP_LOG(APP_LOG_LEVEL_DEBUG,"Last Accel : (%i,%i,%i)",lastX,lastY,lastZ);
	if(DIVERG(accel->x,lastX) >= MIN_MOVMNT || 
	   DIVERG(accel->y,lastY) >= MIN_MOVMNT || 
	   DIVERG(accel->z,lastZ) >= MIN_MOVMNT) {
		totalAccel.x += accel->x;
		totalAccel.y += accel->y;
		totalAccel.z += accel->z;
		return true;
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG,"No XP");
	return false;

}
static int getCurrentLvlFromXP() {
	return (int)((xp_multiplier + my_sqrt(xp_multiplier * xp_multiplier - 4 * xp_multiplier * (-xp_counter) ))/ (2 * xp_multiplier));
}

static void updateXpLayer() {
	static char xp[15];
	snprintf(xp, sizeof(xp), "XP    %lu", xp_counter);	
	text_layer_set_text(xp_layer, xp);
}
static void updateLvlNextLayers() {	
	static char nextLvl[15];
	static char lvl[10];
	
	snprintf(lvl, sizeof(lvl), "Level %lu", lvl_counter);	
	text_layer_set_text(lvl_layer, lvl);
	snprintf(nextLvl, sizeof(nextLvl), "Next %lu", xp_needed);	
	text_layer_set_text(nextLvl_layer, nextLvl);
}

static uint16_t getModulo(AccelData *data) {
	uint16_t smallest;
	uint8_t nbAccel = totalAccel.total;
	
	uint16_t divergX = DIVERG(totalAccel.x/nbAccel,data->x);
	uint16_t divergY = DIVERG(totalAccel.y/nbAccel,data->y);
	uint16_t divergZ = DIVERG(totalAccel.z/nbAccel,data->z);
	
	smallest = divergX;
	
	if(divergY < smallest) {
		smallest = divergY;
	}
	if(divergZ < smallest) {
		smallest = divergZ;
	}
	
	APP_LOG(APP_LOG_LEVEL_DEBUG,"DIV : (%i,%i,%i)",divergX,divergY,divergZ);
	return smallest;
}
static void loadVaultBoyState(uint8_t ressource) {
	APP_LOG(APP_LOG_LEVEL_DEBUG,"Load image %i",ressource);
	if (vaultBoy) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Pointer instancied");
			if(ressource == loadedImage) {
				APP_LOG(APP_LOG_LEVEL_DEBUG, "Same image %i", loadedImage);
				return;
			}
		APP_LOG(APP_LOG_LEVEL_DEBUG,"Unload image %i",loadedImage);
		gbitmap_destroy(vaultBoy);	
    	free(vaultBoy);
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Load image to layer");   
	vaultBoy = gbitmap_create_with_resource(ressource);	
    bitmap_layer_set_bitmap(vaultBoy_layer, vaultBoy); 
    loadedImage = ressource;  
}

static void killVaultBoy() {
	dead = true;
	loadVaultBoyState(RESOURCE_ID_DEAD);
	currentVaultBoy = RESOURCE_ID_VAULT_BOY;
	xp_counter *= XP_LOSS;
	lvl_counter = getCurrentLvlFromXP();
	xp_needed = getXpForNextLvl();
	updateXpLayer();
	updateLvlNextLayers();
	vibes_long_pulse();
	
}

static void vaultBoy_status() {
	
	if(battery_state_service_peek().is_charging  && currentVaultBoy > RESOURCE_ID_VAULT_BOY) {
		loadVaultBoyState(--currentVaultBoy);
		persist_write_int(PIPE_CURRENT_CRIPPLED,currentVaultBoy);
		return;
	}
	
	uint64_t currentGain = xp_counter - lastXp;
	if(currentGain <= lastGain) {
		currentVaultBoy++;
		if(currentVaultBoy == (MAX_CRIPPLED + 1)) {
			killVaultBoy();
		} else {
			loadVaultBoyState(currentVaultBoy);
		}
	} else if(currentVaultBoy > RESOURCE_ID_VAULT_BOY) {
		loadVaultBoyState(--currentVaultBoy);
	}
	lastGain = currentGain;
	lastXp = xp_counter;
	persist_write_int(PIPE_LAST_GAIN,lastGain);
	persist_write_int(PIPE_LAST_XP,lastXp);
	persist_write_int(PIPE_CURRENT_CRIPPLED,currentVaultBoy);
}

static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[15];

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "resting");
  } else {
    snprintf(battery_text, sizeof(battery_text), "HP %d/100", charge_state.charge_percent);
  }
  text_layer_set_text(battery_layer, battery_text);
}
static void setTimeLayers(struct tm* tick_time, TimeUnits units_changed) {
	static char time_text[6]; 
	char *time_format;
	char *date_format;
	
	if (clock_is_24h_style()) {
    	time_format = "%R";
		date_format = "%d-%m-%Y";
    } else {
    	time_format = "%I:%M";
		date_format = "%m-%d-%Y";
    }
	
	strftime(time_text, sizeof(time_text), time_format, tick_time);
	text_layer_set_text(time_layer, time_text);

	
  	if (units_changed & DAY_UNIT) {
		static char date_text[20];
		strftime(date_text, sizeof(date_text), date_format, tick_time);
		text_layer_set_text(date_layer, date_text);
	}
}
static void handle_second_tick(struct tm* tick_time, TimeUnits units_changed) {

	if(!(units_changed & MINUTE_UNIT)) {
		return;
	}
	
	setTimeLayers(tick_time,units_changed);
	
	if(dead) {
		dead = rand() %2;
		if(!dead) {
			loadVaultBoyState(currentVaultBoy);
		} else {
			return;
		}
	}
		
	AccelData accel;
	accel_service_peek(&accel);
	
	APP_LOG(APP_LOG_LEVEL_DEBUG,"Accel : (%i,%i,%i)",accel.x,accel.y,accel.z);
	
	if(!canGainXP(&accel)) {
		return;
	}

		
	if(totalAccel.total > 1){
		uint16_t modulo = getModulo(&accel) + 10;
		uint16_t increase = (rand() % modulo) + 1;
		APP_LOG(APP_LOG_LEVEL_DEBUG,"Add xp %i%%%i", increase,modulo);
		xp_counter += increase;
		persist_write_int(PIPEXP, xp_counter);
		updateXpLayer();
	}
	
	bool levelUp = false;
	while(xp_counter >= xp_needed) {
		lvl_counter++;
		xp_needed = getXpForNextLvl();
		levelUp = true;
	}
	
	if(levelUp) {
		updateLvlNextLayers();
	}
	

	
	if(totalAccel.total == RESET_TOTAL_MIN) {
		totalAccel = (AccelTotal){0,0,0,0,0,0,0};
		vaultBoy_status();		
	}

}

void update_date_text(){
	time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
	static char date_text[20];
	char *date_format;
	 if (clock_is_24h_style()) {
    	date_format = "%d-%m-%Y";
    } else {
    	date_format = "%m-%d-%Y";
    }
	strftime(date_text, sizeof(date_text), date_format, current_time);
  	text_layer_set_text(date_layer, date_text);
}


static void handle_bluetooth(bool connected) {
   if (connected) {
    vibes_enqueue_custom_pattern(BLUETOOTH_CONNECT_VIBE);
  } else{
    vibes_enqueue_custom_pattern(BLUETOOTH_DISCONNECT_VIBE);
  }
}


static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
  //doing nothing here
}


static void do_init(void) {
  srand(time(NULL));
  xp_multiplier = 28;
  if(persist_exists(PIPEXP)){
	xp_counter = persist_read_int(PIPEXP);
  }
  
  if(persist_exists(PIPE_LAST_XP)) {
	  lastXp = persist_read_int(PIPE_LAST_XP);
  }
  
  if(persist_exists(PIPE_LAST_GAIN)) {
	  lastGain = persist_read_int(PIPE_LAST_GAIN);
  }
  
  if(persist_exists(PIPE_CURRENT_CRIPPLED)) {
	  currentVaultBoy = persist_read_int(PIPE_CURRENT_CRIPPLED);
	  dead = (currentVaultBoy == RESOURCE_ID_DEAD);
  }
  
  if(persist_exists(PIPE_TOTAL)) {
	 persist_read_data(PIPE_TOTAL, &totalAccel, sizeof(totalAccel));
	 APP_LOG(APP_LOG_LEVEL_DEBUG,"totalAccel : (%i,%i,%i) -- %i",totalAccel.x,totalAccel.y,totalAccel.z,totalAccel.total);
  }
  
  
  lvl_counter = getCurrentLvlFromXP();
  xp_needed = getXpForNextLvl();

  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

  Layer *root_layer = window_get_root_layer(window);
  GRect frame = layer_get_frame(root_layer);
  
  image = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND);	
  image_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(image_layer, image);
  bitmap_layer_set_alignment(image_layer, GAlignCenter);
  layer_add_child(root_layer, bitmap_layer_get_layer(image_layer));
	
  vaultBoy_layer = bitmap_layer_create(GRect(3, 26, frame.size.w, 100));
  loadVaultBoyState(currentVaultBoy);		
  bitmap_layer_set_bitmap(vaultBoy_layer, vaultBoy);
  bitmap_layer_set_alignment(vaultBoy_layer, GAlignCenter);
  layer_add_child(root_layer, bitmap_layer_get_layer(vaultBoy_layer));	
	
  time_layer = text_layer_create(GRect(-8, 133, frame.size.w , 34));
  text_layer_set_text_color(time_layer, GColorWhite);
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(time_layer, GTextAlignmentRight);

  battery_layer = text_layer_create(GRect(-8, 4, frame.size.w, 34));
  text_layer_set_text_color(battery_layer, GColorWhite);
  text_layer_set_background_color(battery_layer, GColorClear);
  text_layer_set_font(battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(battery_layer, GTextAlignmentRight);
  text_layer_set_text(battery_layer, "HP 100/100");
	
  date_layer = text_layer_create(GRect(8, 4, frame.size.w, 34));
  text_layer_set_text_color(date_layer, GColorWhite);
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);
  text_layer_set_text(date_layer, "1-1-2013");
	
  xp_layer = text_layer_create(GRect(8, 133, frame.size.w, 34));
  text_layer_set_text_color(xp_layer, GColorWhite);
  text_layer_set_background_color(xp_layer, GColorClear);
  text_layer_set_font(xp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(xp_layer, GTextAlignmentLeft);
  text_layer_set_text(xp_layer, "XP");
  
  
  nextLvl_layer = text_layer_create(GRect(8, 146, frame.size.w, 34));
  text_layer_set_text_color(nextLvl_layer, GColorWhite);
  text_layer_set_background_color(nextLvl_layer, GColorClear);
  text_layer_set_font(nextLvl_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(nextLvl_layer, GTextAlignmentLeft);
  text_layer_set_text(nextLvl_layer, "Next");		
	
  lvl_layer = text_layer_create(GRect(0, 121, frame.size.w, 34));
  text_layer_set_text_color(lvl_layer, GColorWhite);
  text_layer_set_background_color(lvl_layer, GColorClear);
  text_layer_set_font(lvl_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(lvl_layer, GTextAlignmentCenter);
  text_layer_set_text(lvl_layer, "Level 1");	
	
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_battery(battery_state_service_peek());
  accel_data_service_subscribe(0, &handle_accel);
  setTimeLayers(current_time,SECOND_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, &handle_second_tick);
  battery_state_service_subscribe(&handle_battery);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  
  layer_add_child(root_layer, text_layer_get_layer(time_layer));
  layer_add_child(root_layer, text_layer_get_layer(battery_layer));
  layer_add_child(root_layer, text_layer_get_layer(date_layer));
  layer_add_child(root_layer, text_layer_get_layer(xp_layer));
  layer_add_child(root_layer, text_layer_get_layer(nextLvl_layer));
  layer_add_child(root_layer, text_layer_get_layer(lvl_layer));	
 
  update_date_text();
  updateLvlNextLayers();
  updateXpLayer();
}



static void do_deinit(void) {
  persist_write_int(PIPEXP, xp_counter);
  persist_write_int(PIPE_LAST_XP, lastXp);
  persist_write_int(PIPE_LAST_GAIN,lastGain);
  persist_write_int(PIPE_CURRENT_CRIPPLED,currentVaultBoy);
  persist_write_data(PIPE_TOTAL, &totalAccel, sizeof(totalAccel));
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  
  accel_data_service_unsubscribe();
  text_layer_destroy(time_layer);
  text_layer_destroy(battery_layer);
  text_layer_destroy(date_layer);
  text_layer_destroy(xp_layer);
  text_layer_destroy(lvl_layer);
  text_layer_destroy(nextLvl_layer);
  
  bitmap_layer_destroy(image_layer);
  gbitmap_destroy(image);
  bitmap_layer_destroy(vaultBoy_layer);
  gbitmap_destroy(vaultBoy);	
	if (vaultBoy) {
    	free(vaultBoy);
    } 
	if (image_layer) {
    	free(image_layer);
    } 
  window_destroy(window);
}


int main(void) {
  do_init();
  app_event_loop();
  do_deinit();
}
