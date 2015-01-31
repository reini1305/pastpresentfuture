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
static TextLayer *date_layer;
static char date_buffer[3];
static bool draw_date = false;
static AppTimer *date_timer;

static Layer *hands_layer;
static TransBitmap *background_bitmap;

static Window *window;

// optional stuff
static InverterLayer *inverter_layer;
static BitmapLayer *bluetooth_layer;
static GBitmap *bluetooth_bitmap;
static BitmapLayer *battery_layer;
static GBitmap *battery_bitmap;


static void handle_battery(BatteryChargeState battery)
{
  if(getBattery())
  {
    if(battery.is_charging || battery.is_plugged || battery.charge_percent > 30)
    layer_set_hidden(bitmap_layer_get_layer(battery_layer),true);
    else
    layer_set_hidden(bitmap_layer_get_layer(battery_layer),false);
  }
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  autoconfig_in_received_handler(iter, context);
  layer_set_hidden(inverter_layer_get_layer(inverter_layer),!getInvert());
  layer_mark_dirty(window_get_root_layer(window));
  handle_battery(battery_state_service_peek());
}


static void handle_bluetooth(bool connected)
{
  if(getBluetooth())
  {
    layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer),connected);
    if(!connected)
      vibes_double_pulse();
  }
}

static void disable_date(void *data)
{
  draw_date=false;
  layer_mark_dirty(window_get_root_layer(window));
}

static void enable_date(void)
{
  draw_date=true;
  if(!app_timer_reschedule(date_timer,2000))
    date_timer = app_timer_register(2000,disable_date,NULL);
  layer_mark_dirty(window_get_root_layer(window));
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
    int16_t minute_angle = TRIG_MAX_ANGLE * (((59-t->tm_min)+i*5)%60) / 60;
    minuteHand.y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)minuteHandLength / TRIG_MAX_RATIO) + center.y;
    minuteHand.x = (int16_t)(sin_lookup(minute_angle) * (int32_t)minuteHandLength / TRIG_MAX_RATIO) + center.x;

    GRect frame = layer_get_frame(text_layer_get_layer(num_layer[i]));
    frame.origin.x = minuteHand.x-frame.size.w/2;
    frame.origin.y = minuteHand.y-frame.size.h/2;
    layer_set_frame(text_layer_get_layer(num_layer[i]),frame);
    layer_set_hidden(text_layer_get_layer(num_layer[i]),false);
    
    int16_t hour_angle = (TRIG_MAX_ANGLE * (((24-t->tm_hour+i) % 12) * 6) +
                         ((TRIG_MAX_ANGLE * (60-t->tm_min)) / 10))     / (12 * 6);
    minuteHand.y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)hourHandLength / TRIG_MAX_RATIO) + center.y;
    minuteHand.x = (int16_t)(sin_lookup(hour_angle) * (int32_t)hourHandLength / TRIG_MAX_RATIO) + center.x;
    
    frame = layer_get_frame(text_layer_get_layer(hour_layer[i]));
    frame.origin.x = minuteHand.x-frame.size.w/2;
    frame.origin.y = minuteHand.y-frame.size.h/2;
    layer_set_frame(text_layer_get_layer(hour_layer[i]),frame);
    layer_set_hidden(text_layer_get_layer(hour_layer[i]),false);

  }
  
  // draw minute line
  if(getDrawline())
  {
    if(getInvert())
      graphics_context_set_stroke_color(ctx,GColorWhite);
    else
      graphics_context_set_stroke_color(ctx,GColorBlack);
    graphics_draw_line(ctx,GPoint(144/2,0),GPoint(144/2,167));
  }
  // draw background
  transbitmap_draw_in_rect(background_bitmap, ctx, bounds);
  
  // draw date
  if(draw_date)
  {
    graphics_context_set_fill_color(ctx,GColorBlack);
    graphics_fill_circle(ctx,GPoint(120,120),16);
    layer_set_hidden(text_layer_get_layer(date_layer),false);
  }
  else
    layer_set_hidden(text_layer_get_layer(date_layer),true);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (clock_is_24h_style()) // we need to modify the labels
  {
    if(tick_time->tm_hour>=2 && tick_time->tm_hour<11) // all labels to lower
    {
      for (int i=0;i<12;i++){
        snprintf(hour_buffer[i],sizeof(hour_buffer[i]),"%d",(i+1));
      }
    }
    else if(tick_time->tm_hour>=11 && tick_time->tm_hour<13) // 11, 12 lower, rest higher
    {
      for (int i=0;i<10;i++){
        snprintf(hour_buffer[i],sizeof(hour_buffer[i]),"%d",(i+13));
      }
      snprintf(hour_buffer[10],sizeof(hour_buffer[10]),"11");
      snprintf(hour_buffer[11],sizeof(hour_buffer[11]),"12");
    }
    else if(tick_time->tm_hour>=13 && tick_time->tm_hour<23) // all labels to higher, 12<=0
    {
      for (int i=0;i<11;i++){
        snprintf(hour_buffer[i],sizeof(hour_buffer[i]),"%d",(i+13));
      }
      snprintf(hour_buffer[11],sizeof(hour_buffer[11]),"0");
    }
    else  // 1,2 to lower, rest higher
    {
      for (int i=2;i<12;i++){
        snprintf(hour_buffer[i],sizeof(hour_buffer[i]),"%d",(i+13));
      }
      snprintf(hour_buffer[11],sizeof(hour_buffer[11]),"0");
      snprintf(hour_buffer[0],sizeof(hour_buffer[0]),"1");
      snprintf(hour_buffer[1],sizeof(hour_buffer[1]),"2");
    }
  }
  snprintf(date_buffer, sizeof(date_buffer), "%d",tick_time->tm_mday);
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
  battery_layer = bitmap_layer_create(GRect(143-32,167-20,32,20));
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
  
  
  // Inverter Layer
  inverter_layer = inverter_layer_create(bounds);
  layer_set_hidden(inverter_layer_get_layer(inverter_layer),!getInvert());
  layer_add_child(window_layer,inverter_layer_get_layer(inverter_layer));
  
  layer_add_child(window_layer, hands_layer);
  
  // Bluetooth Stuff
  bluetooth_connection_service_subscribe(handle_bluetooth);
  layer_add_child(window_layer,bitmap_layer_get_layer(bluetooth_layer));
  handle_bluetooth(bluetooth_connection_service_peek());
  
  // Battery Stuff
  battery_state_service_subscribe(handle_battery);
  layer_add_child(window_layer,bitmap_layer_get_layer(battery_layer));
  handle_battery(battery_state_service_peek());
  
  
  // Date Layer
  date_layer = text_layer_create(GRect(104,105,32,32));
  text_layer_set_text(date_layer,date_buffer);
  text_layer_set_background_color(date_layer,GColorClear);
  text_layer_set_text_color(date_layer,GColorWhite);
  text_layer_set_font(date_layer,hour_font);
  text_layer_set_text_alignment(date_layer,GTextAlignmentCenter);
  layer_add_child(window_layer,text_layer_get_layer(date_layer));
  layer_set_hidden(text_layer_get_layer(date_layer),true);
  
  // force update
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  snprintf(date_buffer, sizeof(date_buffer), "%d",t->tm_mday);
  handle_tick(t, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  // draw date if enabled
  enable_date();
}


static void window_unload(Window *window) {
  /*layer_destroy(simple_bg_layer);
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
  bitmap_layer_destroy(battery_layer);*/
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

static void init(void) {
  autoconfig_init();
  app_message_register_inbox_received(in_received_handler);
  accel_tap_service_subscribe(&accel_tap_handler);

  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);

}

static void deinit(void) {
  accel_tap_service_unsubscribe();
  window_destroy(window);
  autoconfig_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
