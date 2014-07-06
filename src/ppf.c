#include "TransBitmap.h"
#include "pebble.h"
#include "autoconfig.h"

#include "string.h"
#include "stdlib.h"

static Layer *simple_bg_layer;

static TextLayer *num_layer[12];
static char num_buffer[12][3];
static TextLayer *hour_layer[12];
static char hour_buffer[12][3];

static Layer *hands_layer;
static TransBitmap *background_bitmap;

static Window *window;

// optional stuff
static InverterLayer *inverter_layer;
static BitmapLayer *bluetooth_layer;
static GBitmap *bluetooth_bitmap;
static BitmapLayer *battery_layer;
static GBitmap *battery_bitmap;

static void in_received_handler(DictionaryIterator *iter, void *context) {
  autoconfig_in_received_handler(iter, context);
  layer_mark_dirty(window_get_root_layer(window));
}


static void handle_bluetooth(bool connected)
{
  if(getBluetooth())
  {
    layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer),connected);
    vibes_double_pulse();
  }
}

static void handle_battery(BatteryChargeState battery)
{
  if(getBattery())
  {
    if(battery.is_charging || battery.is_plugged || battery.charge_percent > 19)
      layer_set_hidden(bitmap_layer_get_layer(battery_layer),true);
    else
      layer_set_hidden(bitmap_layer_get_layer(battery_layer),false);
  }
}

static void bg_update_proc(Layer *layer, GContext *ctx) {

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  //graphics_context_set_fill_color(ctx, GColorWhite);
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  const int16_t hourHandLength = bounds.size.w / 2 +20;
  const int16_t minuteHandLength = hourHandLength - 30;

  GPoint minuteHand;
  center.y +=20;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  for (int i=0;i<12;i++)
  {
    int16_t minute_angle = TRIG_MAX_ANGLE * (((60-t->tm_min)+i*5)%60) / 60;
    minuteHand.y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)minuteHandLength / TRIG_MAX_RATIO) + center.y;
    minuteHand.x = (int16_t)(sin_lookup(minute_angle) * (int32_t)minuteHandLength / TRIG_MAX_RATIO) + center.x;

    GRect frame = layer_get_frame(text_layer_get_layer(num_layer[i]));
    frame.origin.x = minuteHand.x-frame.size.w/2;
    frame.origin.y = minuteHand.y-frame.size.h/2;
    layer_set_frame(text_layer_get_layer(num_layer[i]),frame);
    layer_set_hidden(text_layer_get_layer(num_layer[i]),false);
    
    int16_t hour_angle = (TRIG_MAX_ANGLE * ((((24-t->tm_hour+i) % 12) * 6) + ((60-t->tm_min) / 10))) / (12 * 6);
    minuteHand.y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)hourHandLength / TRIG_MAX_RATIO) + center.y;
    minuteHand.x = (int16_t)(sin_lookup(hour_angle) * (int32_t)hourHandLength / TRIG_MAX_RATIO) + center.x;
    
    frame = layer_get_frame(text_layer_get_layer(hour_layer[i]));
    frame.origin.x = minuteHand.x-frame.size.w/2;
    frame.origin.y = minuteHand.y-frame.size.h/2;
    layer_set_frame(text_layer_get_layer(hour_layer[i]),frame);
    layer_set_hidden(text_layer_get_layer(hour_layer[i]),false);

  }
  
  // draw background
  transbitmap_draw_in_rect(background_bitmap, ctx, bounds);
  layer_set_hidden(inverter_layer_get_layer(inverter_layer),!getInvert());
}


static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(window));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // init layers
  simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, simple_bg_layer);
  
  // init bitmaps
  background_bitmap = transbitmap_create_with_resource_prefix(RESOURCE_ID_IMAGE_BACKGROUND);
  bluetooth_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  battery_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  bluetooth_layer = bitmap_layer_create(GRect(1,168-30,30,30));
  bitmap_layer_set_bitmap(bluetooth_layer,bluetooth_bitmap);
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer),true);
  battery_layer = bitmap_layer_create(GRect(144-32,168-20,32,20));
  bitmap_layer_set_bitmap(battery_layer,battery_bitmap);
  layer_set_hidden(bitmap_layer_get_layer(battery_layer),true);
  
  
  // init hands
  hands_layer = layer_create(bounds);
  layer_set_update_proc(hands_layer, hands_update_proc);
  
  GFont hour_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_SUBSET_24));
  GFont minute_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_SUBSET_20));
  for (int i=0;i<12;i++)
  {
    num_layer[i] = text_layer_create(GRect(1,1,40,20));
    snprintf(num_buffer[i],sizeof(num_buffer[i]),"%d",(i)*5);
    text_layer_set_text(num_layer[i],num_buffer[i]);
    text_layer_set_background_color(num_layer[i], GColorClear);
    text_layer_set_text_color(num_layer[i], GColorBlack);
    text_layer_set_text_alignment(num_layer[i],GTextAlignmentCenter);
    text_layer_set_font(num_layer[i], minute_font);
    layer_add_child(window_layer,text_layer_get_layer(num_layer[i]));
    layer_set_hidden(text_layer_get_layer(num_layer[i]),true);
    
    hour_layer[i] = text_layer_create(GRect(1,1,48,24));
    snprintf(hour_buffer[i],sizeof(hour_buffer[i]),"%d",(i+1));
    text_layer_set_text(hour_layer[i],hour_buffer[i]);
    text_layer_set_background_color(hour_layer[i], GColorClear);
    text_layer_set_text_color(hour_layer[i], GColorBlack);
    text_layer_set_font(hour_layer[i], hour_font);
    text_layer_set_text_alignment(hour_layer[i],GTextAlignmentCenter);
    layer_add_child(window_layer,text_layer_get_layer(hour_layer[i]));
    layer_set_hidden(text_layer_get_layer(hour_layer[i]),true);
  }
  
  layer_add_child(window_layer, hands_layer);
  
  // Inverter Layer
  inverter_layer = inverter_layer_create(bounds);
  layer_set_hidden(inverter_layer_get_layer(inverter_layer),!getInvert());
  layer_add_child(window_layer,inverter_layer_get_layer(inverter_layer));
  
  // Bluetooth Stuff
  bluetooth_connection_service_subscribe(handle_bluetooth);
  layer_add_child(window_layer,bitmap_layer_get_layer(bluetooth_layer));

  
  // Battery Stuff
  battery_state_service_subscribe(handle_battery);
  layer_add_child(window_layer,bitmap_layer_get_layer(battery_layer));
}

static void window_unload(Window *window) {
  layer_destroy(simple_bg_layer);
  layer_destroy(hands_layer);
  inverter_layer_destroy(inverter_layer);
  for(int i=0;i<12;i++)
  {
    text_layer_destroy(num_layer[i]);
    text_layer_destroy(hour_layer[i]);
  }
  
  transbitmap_destroy(background_bitmap);
  gbitmap_destroy(bluetooth_bitmap);
  gbitmap_destroy(battery_bitmap);
  bitmap_layer_destroy(bluetooth_layer);
  bitmap_layer_destroy(battery_layer);
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

static void init(void) {
  autoconfig_init();
  app_message_register_inbox_received(in_received_handler);

  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void deinit(void) {


  tick_timer_service_unsubscribe();
  window_destroy(window);
  autoconfig_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
