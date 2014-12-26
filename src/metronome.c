#include <pebble.h>

/*
 * Metronome
 *
 * Simple metronome, with time detection.
 *
 * Copyright 2013 Frank Taylor
 */

#define MAX_CLICKS 5
#define MAX_TEMPO 500
#define MIN_TEMPO 10

#ifdef DO_LOGGING
#define LOG_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif
#define LOG_DEBUG1(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

static Window *window;
static TextLayer *metro_bpm_layer;
static Layer *metro_tick_layer;

static char metro_bpm_msg[20];
static int metro_running = 1;
static int metro_bpm = 60;
static int metro_tock = 0;

static Window *tempo_window;
static TextLayer *tempo_msg_layer;
static int tempo_clicks = 0;
static uint32_t tempo_times[MAX_CLICKS];
static int tempo_bpm = 0;
static char tempo_msg[20];

// Tempo ------------------------------------------------------------

static uint16_t add_click(uint32_t t) {
    if (MAX_CLICKS == tempo_clicks) {
	// Shuffle all the clicks up.
	for (int i = 0; i < MAX_CLICKS - 1; i++) {
	    tempo_times[i] = tempo_times[i+1];
	}
    } else {
	// Take the next empty slot.
	tempo_clicks++;
    }

    tempo_times[tempo_clicks - 1] = t;

    if (tempo_clicks < 2) {
	// Not enough clicks to create a diff.
	return 0;
    }

    // Determine the average difference.
    return (t - tempo_times[0]) / (tempo_clicks - 1);
}

static void tempo_single_click_handler(ClickRecognizerRef recognizer, void *ctx) {
    time_t seconds;
    uint16_t millis;
    uint32_t normal_millis;
    uint16_t diff;

    time_ms(&seconds, &millis);
    normal_millis = (seconds * 1000) + millis;

    diff = add_click(normal_millis);

    if (diff == 0) {
	// Not enough clicks... nothing more to do.
	return;
    }

    tempo_bpm = 60000 / diff;

    snprintf(tempo_msg, 20, "%d", tempo_bpm);
    text_layer_set_text(tempo_msg_layer, tempo_msg);
}

static void handle_tempo_disappear(Window *window) {
    // Reset the tempo times... ready for the next time we need it.
    tempo_clicks = 0;

    if (tempo_bpm > 0) {
	// We have a valid bpm... use it.
	metro_bpm = tempo_bpm;
    }

    text_layer_set_text(tempo_msg_layer, "Beat time");
}

static void tempo_config_provider(Window *window) {
    // single click / repeat-on-hold config:
    window_single_click_subscribe(BUTTON_ID_DOWN, &tempo_single_click_handler);
}

// Metro ------------------------------------------------------------

static void schedule_tick();

static void metro_tick_dirty() {
    layer_mark_dirty(metro_tick_layer);
}

static void handle_tick_timer(void *data) {
    metro_tock = metro_tock == 0 ? 1 : 0;
    metro_tick_dirty();
    
    if (metro_running) {
	schedule_tick();
    }
}

static void schedule_tick() {
    app_timer_register(60000 / metro_bpm, &handle_tick_timer, NULL);
}

static void metro_bpm_update() {
    snprintf(metro_bpm_msg, sizeof(metro_bpm_msg), "%d", metro_bpm);
    text_layer_set_text(metro_bpm_layer, metro_bpm_msg);
}

static void tmp_message(char *msg) {
    snprintf(metro_bpm_msg, sizeof(metro_bpm_msg), "%s", msg);
    text_layer_set_text(metro_bpm_layer, metro_bpm_msg);
}

static void metro_select_single_click_handler(ClickRecognizerRef recognizer, void *ctx) {
    metro_running = 0;
    window_stack_push(tempo_window, true);
}

static void metro_up_single_click_handler(ClickRecognizerRef recognizer, void *ctx) {
    if (metro_bpm < MAX_TEMPO) {
	metro_bpm++;
	metro_bpm_update();
    }
}

static void metro_down_single_click_handler(ClickRecognizerRef recognizer, void *ctx) {
    if (metro_bpm > MIN_TEMPO) {
	metro_bpm--;
	metro_bpm_update();
    }
}

static void metro_config_provider(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_SELECT, &metro_select_single_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, &metro_up_single_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, &metro_down_single_click_handler);
}

static void metro_tick_layer_update_callback(Layer *me, GContext *ctx) {
    if (metro_running) {
	graphics_fill_circle(ctx, (GPoint) { 35 + (metro_tock * 72), 40 }, 20);
    }
}

static void handle_metro_appear(Window *window) {
    metro_bpm_update();
    metro_running = 1;
    schedule_tick();
}

static void init(void) {
    window = window_create();
    window_set_click_config_provider(window, &metro_config_provider);

    Layer *window_layer = window_get_root_layer(window);

    // Tick layer - top half of the screen
    metro_tick_layer = layer_create((GRect) { .origin = { 0, 0 }, .size = { 144, 168 } });
    layer_set_update_proc(metro_tick_layer, &metro_tick_layer_update_callback);
    layer_add_child(window_layer, metro_tick_layer);
    
    window_set_window_handlers(window, (WindowHandlers) {
	    .appear = &handle_metro_appear,
	});

    // BPM layer - bottom half of the screen
    metro_bpm_layer = text_layer_create((GRect) { .origin = { 0, 91 }, .size = { 144, 80 } });
    text_layer_set_text_alignment(metro_bpm_layer, GTextAlignmentCenter);
    text_layer_set_text(metro_bpm_layer, "");
    text_layer_set_font(metro_bpm_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    layer_add_child(window_layer, (Layer *) metro_bpm_layer);

    // Tempo window...
    tempo_window = window_create();
    Layer *tempo_window_layer = window_get_root_layer(tempo_window);

    window_set_click_config_provider(tempo_window,
				     (ClickConfigProvider) &tempo_config_provider);
    window_set_window_handlers(tempo_window, (WindowHandlers) {
	    .disappear = &handle_tempo_disappear,
	});
	
    tempo_msg_layer = text_layer_create((GRect) { .origin = { 0, 65 }, .size = { 144, 30 } });
    text_layer_set_text_alignment(tempo_msg_layer, GTextAlignmentCenter);
    text_layer_set_text(tempo_msg_layer, "Beat time");
    text_layer_set_font(tempo_msg_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    layer_add_child(tempo_window_layer, (Layer *) tempo_msg_layer);

    const bool animated = true;
    window_stack_push(window, animated);

    // Register timer.
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  LOG_DEBUG("Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
