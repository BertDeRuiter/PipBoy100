//Copyright 2013 Bert de Ruiter (www.bertderuiter.nl/)

#include "pebble.h"

#define PIPEXP 0	
#define SQRT_MAX_STEPS 40


static Window *window; 
static TextLayer *time_layer; 
static TextLayer *date_layer;
static TextLayer *battery_layer;
static TextLayer *xp_layer;
static TextLayer *nextLvl_layer;
static TextLayer *lvl_layer;

static BitmapLayer *image_layer;
static BitmapLayer *connect_layer;
static int xp_counter;
static int xp_needed;
static int xp_multiplier;
static int lvl_counter;
static int fap_detection;
static int fap_timer;
static int x_max;
	
static GBitmap *image;
static GBitmap *connect;

const VibePattern BLUETOOTH_DISCONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100, 85, 100},
  .num_segments = 5
};

const VibePattern BLUETOOTH_CONNECT_VIBE = {
  .durations = (uint32_t []) {100, 85, 100},
  .num_segments = 3
};
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

static int getCurrentLvlFromXP() {
	return (int)((xp_multiplier + my_sqrt(xp_multiplier * xp_multiplier - 4 * xp_multiplier * (-xp_counter) ))/ (2 * xp_multiplier));
}



static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[10];

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "resting");
  } else {
    snprintf(battery_text, sizeof(battery_text), "HP %d/100", charge_state.charge_percent);
  }
  text_layer_set_text(battery_layer, battery_text);
}


static void handle_second_tick(struct tm* tick_time, TimeUnits units_changed) {
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
	
  AccelData accel;
  if(units_changed & SECOND_UNIT){
	strftime(time_text, sizeof(time_text), time_format, tick_time);
	 	text_layer_set_text(time_layer, time_text);
  }
 
	accel_service_peek(&accel);
	static char xp[15];
	static char nextLvl[15];
	static char lvl[10];
	fap_timer++;
	
	if(x_max > 0){
		if(accel.y > x_max || accel.z > x_max){
			fap_detection++;
			x_max = -25;
		}	
	}else{
		if(accel.y < x_max || accel.z < x_max){
			fap_detection++;
			x_max = 25;
		}	
	}
	
	if(fap_detection == 2 && fap_timer <= 10){
		int increase = rand() % 10;
		xp_counter = xp_counter + ((increase+1)*2);
		persist_write_int(PIPEXP, xp_counter);
		fap_timer = 0;
		fap_detection = 0;  
	}else if(fap_timer > 10){
		fap_timer = 0;
		fap_detection = 0;  
	}
	if(xp_counter >= xp_needed) {
		lvl_counter++;
		xp_needed = getXpForNextLvl();
	}
	snprintf(lvl, sizeof(lvl), "Level %i", lvl_counter);	
	text_layer_set_text(lvl_layer, lvl);
	snprintf(xp, sizeof(xp), "XP    %i", xp_counter);	
	text_layer_set_text(xp_layer, xp);
	snprintf(nextLvl, sizeof(nextLvl), "Next %i", xp_needed);	
	text_layer_set_text(nextLvl_layer, nextLvl);
	
	handle_battery(battery_state_service_peek());
	if (units_changed & MONTH_UNIT) {
		static char date_text[20];
		strftime(date_text, sizeof(date_text), date_format, tick_time);
		text_layer_set_text(date_layer, date_text);
	}else if(units_changed & DAY_UNIT){
		static char date_text[20];
		strftime(date_text, sizeof(date_text), date_format, tick_time);
		text_layer_set_text(date_layer, date_text);
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
	gbitmap_destroy(connect);	
	if (connect) {
    	free(connect);
    }   
	connect = gbitmap_create_with_resource(RESOURCE_ID_CONNECTED);	
    bitmap_layer_set_bitmap(connect_layer, connect);   
  } else{
    vibes_enqueue_custom_pattern(BLUETOOTH_DISCONNECT_VIBE);
	gbitmap_destroy(connect);	
	   if (connect) {
    	free(connect);
    } 
	connect = gbitmap_create_with_resource(RESOURCE_ID_DISCONNECTED);	
    bitmap_layer_set_bitmap(connect_layer, connect);   
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
  }else{
	xp_counter = 0;
  }
  lvl_counter = getCurrentLvlFromXP();
  xp_needed = getXpForNextLvl();
  fap_detection = 0;
  fap_timer = 0;
  x_max = 25;

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
	
  connect = gbitmap_create_with_resource(RESOURCE_ID_CONNECTED);	
  connect_layer = bitmap_layer_create(GRect(3, 26, frame.size.w, 100));	
  bitmap_layer_set_bitmap(connect_layer, connect);
  bitmap_layer_set_alignment(connect_layer, GAlignCenter);
  layer_add_child(root_layer, bitmap_layer_get_layer(connect_layer));	
	
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
  handle_second_tick(current_time, SECOND_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, &handle_second_tick);
  battery_state_service_subscribe(&handle_battery);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  accel_data_service_subscribe(0, &handle_accel);
  
  layer_add_child(root_layer, text_layer_get_layer(time_layer));
  layer_add_child(root_layer, text_layer_get_layer(battery_layer));
  layer_add_child(root_layer, text_layer_get_layer(date_layer));
  layer_add_child(root_layer, text_layer_get_layer(xp_layer));
  layer_add_child(root_layer, text_layer_get_layer(nextLvl_layer));
  layer_add_child(root_layer, text_layer_get_layer(lvl_layer));	
 
  update_date_text();
}



static void do_deinit(void) {
  persist_write_int(PIPEXP, xp_counter);
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
  bitmap_layer_destroy(connect_layer);
  gbitmap_destroy(connect);	
	if (connect) {
    	free(connect);
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
