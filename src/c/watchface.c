#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_temparature_layer;
static TextLayer *s_watch_battery_icon_layer;
static TextLayer *s_phone_battery_icon_layer;
static TextLayer *s_watch_battery_charging_layer;
static TextLayer *s_phone_battery_charging_layer;
static Layer *s_temparature_background_layer;
static Layer *s_watch_battery_layer;
static Layer *s_phone_battery_layer;
static int s_battery_level;
static char s_temparature_level_buf[] = "00000000000";
static GBitmap *s_icon_bitmap = NULL;
static BitmapLayer *s_icon_layer;
static uint8_t s_phone_battery_level;
static AppSync s_sync;
static uint8_t s_sync_buffer[128];
static GFont s_icon_font;
enum AppMessageKeys {
	PHONE_BATTERY_PERCENTAGE = 0x0, TEMPERATURE = 0x1, WEATHER_ICON_KEY = 0x2, PHONE_CHARGING_STATUS = 0x3
};

static const uint32_t WEATHER_ICONS[] = { RESOURCE_ID_WEATHER_01D_ICON,
		RESOURCE_ID_WEATHER_01N_ICON, RESOURCE_ID_WEATHER_02D_ICON,
		RESOURCE_ID_WEATHER_02N_ICON, RESOURCE_ID_WEATHER_03D_ICON,
		RESOURCE_ID_WEATHER_03N_ICON, RESOURCE_ID_WEATHER_04D_ICON,
		RESOURCE_ID_WEATHER_04N_ICON, RESOURCE_ID_WEATHER_09D_ICON,
		RESOURCE_ID_WEATHER_09N_ICON, RESOURCE_ID_WEATHER_10D_ICON,
		RESOURCE_ID_WEATHER_10N_ICON, RESOURCE_ID_WEATHER_11D_ICON,
		RESOURCE_ID_WEATHER_11N_ICON, RESOURCE_ID_WEATHER_13D_ICON,
		RESOURCE_ID_WEATHER_13N_ICON, RESOURCE_ID_WEATHER_50D_ICON,
		RESOURCE_ID_WEATHER_50N_ICON };

static void sync_error_callback(DictionaryResult dict_error,
		AppMessageResult app_message_error, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d",
			app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key,
		const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Value %d", new_tuple->value->int8);
	persist_write_int(key, new_tuple->value->int32);
	switch (key) {
	case PHONE_BATTERY_PERCENTAGE:
		s_phone_battery_level = new_tuple->value->uint8;
		layer_mark_dirty(s_phone_battery_layer);
		break;
	case TEMPERATURE:
		if(new_tuple->value->int16 > -274){
			snprintf(s_temparature_level_buf, sizeof(s_temparature_level_buf),
					"%d°", new_tuple->value->int16);
			text_layer_set_text(s_temparature_layer, s_temparature_level_buf);
		}
		break;
	case WEATHER_ICON_KEY:
		if(new_tuple->value->int8 >= 0){
			if (s_icon_bitmap) {
				gbitmap_destroy(s_icon_bitmap);
			}
			s_icon_bitmap = gbitmap_create_with_resource(
					WEATHER_ICONS[new_tuple->value->uint8]);
			bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
			bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
		}
		break;
	case PHONE_CHARGING_STATUS:
		text_layer_set_text(s_phone_battery_charging_layer, (new_tuple->value->uint8)?"c":"");
	}
}

static void battery_callback(BatteryChargeState state) {
	s_battery_level = state.charge_percent;
	text_layer_set_text(s_watch_battery_charging_layer, (state.is_charging)?"c":"");
	layer_mark_dirty(s_watch_battery_layer);
}

static void update_time(struct tm *tick_time) {

	// Write the current hours and minutes into a buffer
	static char s_buffer[8];
	strftime(s_buffer, sizeof(s_buffer),
			clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

	// Display this time on the TextLayer
	text_layer_set_text(s_time_layer, s_buffer);
}

static void update_day(struct tm *tick_time) {
	static char s_buffer[20];
	strftime(s_buffer, sizeof(s_buffer), "%a %b %e, %Y", tick_time);
	text_layer_set_text(s_date_layer, s_buffer);
}

static void get_weather() {
	// Begin dictionary
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	// Send the message!
	app_message_outbox_send();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	if( (units_changed & MINUTE_UNIT) != 0 ) {
		update_time(tick_time);
	}

	if( (units_changed & DAY_UNIT) != 0 ) {
		update_day(tick_time);
	}

	if (tick_time->tm_min % 10 == 0) {
		get_weather();
	}
}

static void draw_bar(GContext *ctx, GRect bar_bounds, GColor8 color, int percentage) {
	int sectWidth = (bar_bounds.size.w - 9) / 10;
	bar_bounds.size.w = sectWidth * 10 + 9;
	graphics_context_set_fill_color(ctx, GColorLightGray);
	graphics_fill_rect(ctx, bar_bounds, 0, GCornerNone);
	graphics_context_set_fill_color(ctx, color);
	int offset = 0;
	while (percentage > 10) {
		graphics_fill_rect(ctx,
				GRect(bar_bounds.origin.x + offset, bar_bounds.origin.y,
						sectWidth, bar_bounds.size.h), 0, GCornerNone);
		percentage -= 10;
		offset += sectWidth + 1;
	}
	graphics_fill_rect(ctx,
			GRect(bar_bounds.origin.x + offset, bar_bounds.origin.y,
					sectWidth * percentage / 10, bar_bounds.size.h), 0,
			GCornerNone);
	graphics_context_set_stroke_width(ctx, 1);
	graphics_context_set_stroke_color(ctx, GColorBlack);
	for (size_t i = 1; i < 10; i++) {
		graphics_draw_line(ctx,
				GPoint(bar_bounds.origin.x + sectWidth * i + (i - 1),
						bar_bounds.origin.y),
				GPoint(bar_bounds.origin.x + sectWidth * i + (i - 1),
						bar_bounds.origin.y + bar_bounds.size.h));
	}

}

static void watch_battery_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	draw_bar(ctx, bounds, GColorBlue, s_battery_level);
}

static void phone_battery_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	draw_bar(ctx, bounds, GColorIslamicGreen, s_phone_battery_level);
}

static void temparature_background_layer_update_proc(Layer *layer,
		GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	graphics_context_set_fill_color(ctx, GColorMidnightGreen);
	graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}


static void init_text_layer_font(TextLayer *layer, GColor8 text_color, const char* inital_text, GFont font, GTextAlignment text_alignment) {
	text_layer_set_background_color(layer, GColorClear);
	text_layer_set_text_color(layer, text_color);
	text_layer_set_text(layer, inital_text);
	text_layer_set_font(layer,font);
	text_layer_set_text_alignment(layer, text_alignment);
}

static void init_text_layer(TextLayer *layer, GColor8 text_color, const char* inital_text, const char* font_key, GTextAlignment text_alignment) {
	init_text_layer_font(layer,text_color,inital_text, fonts_get_system_font(font_key),text_alignment);
}


static void prv_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	s_watch_battery_layer = layer_create(GRect(15, 10, bounds.size.w - 20, 10));
	s_phone_battery_layer = layer_create(GRect(15, 25, bounds.size.w - 20, 10));

	layer_set_update_proc(s_watch_battery_layer, watch_battery_update_proc);
	layer_set_update_proc(s_phone_battery_layer, phone_battery_update_proc);

	s_temparature_background_layer = layer_create(
			GRect(0, bounds.size.h - 40, bounds.size.w, 50));
	layer_set_update_proc(s_temparature_background_layer,
			temparature_background_layer_update_proc);
	layer_mark_dirty(s_temparature_background_layer);
	s_temparature_layer = text_layer_create(
			GRect(10, bounds.size.h - 40, bounds.size.w - 10, 50));
	init_text_layer(s_temparature_layer, GColorWhite, "", FONT_KEY_LECO_28_LIGHT_NUMBERS, GTextAlignmentLeft);

	s_time_layer = text_layer_create(
			GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
	init_text_layer(s_time_layer, GColorWhite, "", FONT_KEY_BITHAM_42_BOLD, GTextAlignmentCenter);

	s_date_layer = text_layer_create(
			GRect(0, PBL_IF_ROUND_ELSE(108, 102), bounds.size.w, 30));
	init_text_layer(s_date_layer, GColorWhite, "", FONT_KEY_GOTHIC_14_BOLD, GTextAlignmentCenter);

	s_watch_battery_icon_layer = text_layer_create(GRect(2,10,20,10));
	init_text_layer_font(s_watch_battery_icon_layer, GColorWhite,"b" , s_icon_font, GTextAlignmentLeft);

	s_phone_battery_icon_layer = text_layer_create(GRect(2,25,20,10));
	init_text_layer_font(s_phone_battery_icon_layer, GColorWhite,"a" , s_icon_font, GTextAlignmentLeft);

	s_watch_battery_charging_layer = text_layer_create(GRect(bounds.size.w-20,10,10,10));
	init_text_layer_font(s_watch_battery_charging_layer, GColorBlack,"c" , s_icon_font, GTextAlignmentRight);

	s_phone_battery_charging_layer = text_layer_create(GRect(bounds.size.w-20,25,10,10));
	init_text_layer_font(s_phone_battery_charging_layer, GColorBlack,"c" , s_icon_font, GTextAlignmentRight);

	s_icon_layer = bitmap_layer_create(
			GRect(bounds.size.w - 50, bounds.size.h - 45, 50, 50));

	// Weather layers -- Bottom bar
	layer_add_child(window_layer, s_temparature_background_layer);
	layer_add_child(window_layer, text_layer_get_layer(s_temparature_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));

	// Date and Time layers -- Middle
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

	// Battery Bar layers -- Top bars
	layer_add_child(window_layer, s_watch_battery_layer);
	layer_add_child(window_layer, s_phone_battery_layer);
	layer_add_child(window_layer, text_layer_get_layer(s_watch_battery_icon_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_phone_battery_icon_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_watch_battery_charging_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_phone_battery_charging_layer));

	int32_t default_temp = -300;
	int32_t default_icon_key = -1;

	if(persist_exists(TEMPERATURE)){
		default_temp = persist_read_int(TEMPERATURE);
	}

	if(persist_exists(WEATHER_ICON_KEY)){
		default_icon_key = persist_read_int(WEATHER_ICON_KEY);
	}

	Tuplet initial_values[] = {
			TupletInteger(PHONE_BATTERY_PERCENTAGE,(uint8_t ) 0),
			TupletInteger(TEMPERATURE, default_temp),
			TupletInteger(WEATHER_ICON_KEY, default_icon_key),
			TupletInteger(PHONE_CHARGING_STATUS,(uint8_t ) 0)
	};
	app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), initial_values,
			ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
			sync_error_callback, NULL);
}

static void prv_window_unload(Window *window) {
	layer_destroy(s_watch_battery_layer);
	layer_destroy(s_phone_battery_layer);
	layer_destroy(s_temparature_background_layer);
	bitmap_layer_destroy(s_icon_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_temparature_layer);
	text_layer_destroy(s_date_layer);
	text_layer_destroy(s_phone_battery_icon_layer);
	text_layer_destroy(s_watch_battery_icon_layer);
	text_layer_destroy(s_watch_battery_charging_layer);
	text_layer_destroy(s_phone_battery_charging_layer);
}

static void prv_init(void) {
	s_window = window_create();

	s_icon_font = fonts_load_custom_font(
	                          resource_get_handle(RESOURCE_ID_ICON_FONT_10));
	window_set_background_color(s_window, GColorBlack);
	window_set_window_handlers(s_window, (WindowHandlers ) { .load =
					prv_window_load, .unload = prv_window_unload, });
	const bool animated = true;
	tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, tick_handler);
	window_stack_push(s_window, animated);
	time_t temp = time(NULL);
	struct tm *tick_time = localtime(&temp);
	update_time(tick_time);
	update_day(tick_time);
	battery_callback(battery_state_service_peek());
	app_message_open(64, 64);
	get_weather();
}

static void prv_deinit(void) {
	window_destroy(s_window);
}

int main(void) {
	prv_init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p",
			s_window);

	app_event_loop();
	prv_deinit();
}
