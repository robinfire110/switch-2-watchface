#include "switch-2-watchface.h"
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static void update_time() 
{
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char d_buffer[16];
  static char c_buffer[16];
  
  strftime(s_buffer, sizeof(s_buffer), ((settings.HourMode == 0 && clock_is_24h_style()) || settings.HourMode == 2) ? "%H:%M" : "%I:%M", tick_time);
  if (s_buffer[0] == '0' && (settings.HourMode == 1 || (settings.HourMode == 0 && !clock_is_24h_style())))
  {
    memmove(s_buffer, s_buffer+1, strlen(s_buffer));
  }

  //Date
  switch (settings.DateFormat)
  {
    case 0: strftime(d_buffer, sizeof(d_buffer), "%b %d, %G", tick_time); break;
    case 1: strftime(d_buffer, sizeof(d_buffer), "%d %b, %G", tick_time); break;
    case 2: strftime(d_buffer, sizeof(d_buffer), "%G, %b %d", tick_time); break;
    case 3: strftime(d_buffer, sizeof(d_buffer), "%D", tick_time); break;
    case 4: strftime(d_buffer, sizeof(d_buffer), "%d/%m/%y", tick_time); break;
    case 5: strftime(d_buffer, sizeof(d_buffer), "%y/%m/%d", tick_time); break;
    case 6: strftime(d_buffer, sizeof(d_buffer), " ", tick_time); break;
  }

  //Get Number of days away
  //Create release goal (April 2, 2025 for now)
  struct tm release_time = { 0 };
  release_time.tm_year = 2025 - 1900; //0 = Year 1900, so should subtract to get current year
  release_time.tm_mon = 4 - 1; //Minus 1 because it's base 0
  release_time.tm_mday = 2;
  release_time.tm_hour = release_time.tm_min = release_time.tm_sec = 0;
  release_time.tm_isdst = -1;

  //Get difference
  double dt = difftime(mktime(&release_time), mktime(tick_time));
  int days = round(dt / 86400) + 1;
  if (days > 1) snprintf(c_buffer, sizeof(c_buffer), "%d Days", days);
  else if (days == 1) snprintf(c_buffer, sizeof(c_buffer), "%d Day", days);
  else if (days == 0) snprintf(c_buffer, sizeof(c_buffer), "%s", "Direct Day!");
  else if (days < 0) snprintf(c_buffer, sizeof(c_buffer), "%s", "Direct Out!");  
  
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  text_layer_set_text(s_date_layer, d_buffer);
  text_layer_set_text(s_countdown_layer, c_buffer);
}

// Initialize the default settings
static void prv_default_settings() {
  settings.VibrateOnDisconnect = true;
  settings.HourMode = 0;
  settings.DateFormat = 0;
}

// Read settings from persistent storage
static void prv_load_settings() {
  // Load the default settings
  prv_default_settings();

  // Read settings from persistent storage, if they exist
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));

  //Update
  update_time();
  printf("previoussettings");
}

// Save the settings to persistent storage
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) 
{
  /* GENERAL SETTINGS */
  //Vibrate
  Tuple *vibrate_t = dict_find(iterator, MESSAGE_KEY_VibrateOnDisconnect);
  if (vibrate_t)
  {
    settings.VibrateOnDisconnect = vibrate_t->value->uint32;
  }

  //Hour Mode
  Tuple *hour_t = dict_find(iterator, MESSAGE_KEY_HourMode);
  if (hour_t)
  {
    settings.HourMode = atoi(hour_t->value->cstring);
  }

  //Date Format
  Tuple *data_t = dict_find(iterator, MESSAGE_KEY_DateFormat);
  if (data_t)
  {
    settings.DateFormat = atoi(data_t->value->cstring);
  }

  //Save Settings
  update_time();
  prv_save_settings();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) 
{
  update_time();
}

static void bluetooth_callback(bool connected)
{
  //Set depending on status of bluetooth
  if (connected)
  {
    bitmap_layer_set_bitmap(s_bluetooth_layer, s_bluetooth_on_bitmap);
  }
  else
  {
    bitmap_layer_set_bitmap(s_bluetooth_layer, s_bluetooth_off_bitmap);
    //Vibrate on disconnected
    if (settings.VibrateOnDisconnect)
    {
      vibes_double_pulse();
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void main_window_load(Window *window) 
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  /* Background */
  // Create GBitmap
  PBL_IF_RECT_ELSE(s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND), s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_ROUND));

  // Create BitmapLayer to display the GBitmap
  s_background_layer = bitmap_layer_create(bounds);
  
  // Set the bitmap onto the layer and add to the window
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  /* Bluetooth */
  s_bluetooth_on_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ON);
  s_bluetooth_off_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_OFF);
  s_bluetooth_layer = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(64, 0), PBL_IF_RECT_ELSE(140, 154), bounds.size.w, 30));
  bitmap_layer_set_bitmap(s_bluetooth_layer, s_bluetooth_on_bitmap);
  bitmap_layer_set_compositing_mode(s_bluetooth_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bluetooth_layer));

  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });

  /* Time & Date */
  // Create the TextLayer with specific bounds (x, y)
  s_time_layer = text_layer_create(GRect(1, PBL_IF_RECT_ELSE(-2, 8), bounds.size.w, 100));
  s_date_layer = text_layer_create(GRect(1, PBL_IF_RECT_ELSE(117, 120), bounds.size.w, 50));
  s_countdown_layer = text_layer_create(GRect(1, PBL_IF_RECT_ELSE(140, 140), bounds.size.w, 50));

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_COUTURE_48));
  text_layer_set_font(s_time_layer, s_time_font); 
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_COUTURE_16));
  text_layer_set_font(s_date_layer, s_date_font); 
  text_layer_set_font(s_countdown_layer, s_date_font); 

  // Set Layer Properties
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "12:00");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text(s_date_layer, "Mar 3, 2025");
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_countdown_layer, GColorClear);
  text_layer_set_text_color(s_countdown_layer, GColorWhite);
  text_layer_set_text(s_countdown_layer, "0 Days");
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));
}

static void main_window_unload(Window *window) 
{
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_countdown_layer);

  // Unload GFont
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);

  // Destroy GBitmap
  gbitmap_destroy(s_background_bitmap);
  gbitmap_destroy(s_bluetooth_on_bitmap);
  gbitmap_destroy(s_bluetooth_off_bitmap);

  // Destroy BitmapLayer
  bitmap_layer_destroy(s_background_layer);
  bitmap_layer_destroy(s_bluetooth_layer);
  
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) 
  {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Update bluetooth status
  bluetooth_callback(connection_service_peek_pebble_app_connection());

  // Register MessageKeys callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(128, 128);

  // Make sure the time is displayed from the start
  srand(time(NULL));
  //update_time(); Done in "prv_load_settings()"

  //Load Settings
  printf("init");
  prv_load_settings();
}

static void deinit() 
{
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) 
{
  init();
  app_event_loop();
  deinit();
}