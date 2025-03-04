#include <pebble.h>

#define SETTINGS_KEY 1

static Window* s_main_window;
static TextLayer* s_time_layer;
static TextLayer* s_date_layer;
static TextLayer* s_countdown_layer;

static BitmapLayer* s_background_layer;
static BitmapLayer* s_bluetooth_layer;

static GBitmap* s_background_bitmap;
static GBitmap* s_bluetooth_on_bitmap;
static GBitmap* s_bluetooth_off_bitmap;

static GFont s_time_font;
static GFont s_date_font;

/* Define Settings */
typedef struct ClaySettings {
  bool VibrateOnDisconnect;
  int HourMode;
  int DateFormat;
  bool HideCountdown;
} ClaySettings;
static ClaySettings settings;

/* DEFINE */