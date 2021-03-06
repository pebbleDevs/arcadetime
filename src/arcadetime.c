/*
   A few things complicate the implementation of this watch:

   a) The largest size of the Nevis font which the Pebble handles
      seems to be ~47 units (points or pixels?). But the size of
      characters we want is ~100 points.

      This requires us to generate and use images instead of
      fonts--which complicates things greatly.

   b) When I started it wasn't possible to load all the images into
      RAM at once--this means we have to load/unload each image when
      we need it. The images are slightly smaller now than they were
      but I figured it would still be pushing it to load them all at
      once, even if they just fit, so I've stuck with the load/unload
      approach.
 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"

#define BOTTOM_LABEL_VALUE "PEBBLE"

#define MY_UUID {0x78, 0x2B, 0xEA, 0x0C, 0xEA, 0x53, 0x4F, 0x55, 0xB9, 0xAA, 0xF7, 0xF5, 0x0B, 0x41, 0xC4, 0xD2}
PBL_APP_INFO(MY_UUID, "Arcade Time", "mapps", 0x5, 0x0, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);

Window window;

TextLayer text_extra_layer;

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0 1
//     2 3
//
#define TOTAL_IMAGE_SLOTS 4

#define NUMBER_OF_IMAGES 10

// These images are 72 x 84 pixels (i.e. a quarter of the display),
// black and white with the digit character centered in the image.
// (As generated by the `fonttools/font2png.py` script.)
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

BmpContainer image_containers[TOTAL_IMAGE_SLOTS];

#define EMPTY_SLOT -1

// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.
int image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};


void load_digit_image_into_slot(int slot_number, int digit_value) {
  /*

     Loads the digit image from the application's resources and
     displays it on-screen in the correct location.

     Each slot is a quarter of the screen.

   */

  // TODO: Signal these error(s)?

  if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
    return;
  }

  if ((digit_value < 0) || (digit_value > 9)) {
    return;
  }

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }

  image_slot_state[slot_number] = digit_value;
  bmp_init_container(IMAGE_RESOURCE_IDS[digit_value], &image_containers[slot_number]);
  image_containers[slot_number].layer.layer.frame.origin.x = (slot_number % 2) * 72;
  image_containers[slot_number].layer.layer.frame.origin.y = (slot_number / 2) * 84;
  layer_add_child(&window.layer, &image_containers[slot_number].layer.layer);

}


void unload_digit_image_from_slot(int slot_number) {
  /*

     Removes the digit from the display and unloads the image resource
     to free up RAM.

     Can handle being called on an already empty slot.

   */

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    layer_remove_from_parent(&image_containers[slot_number].layer.layer);
    bmp_deinit_container(&image_containers[slot_number]);
    image_slot_state[slot_number] = EMPTY_SLOT;
  }

}


void display_value(unsigned short value, unsigned short row_number, bool show_first_leading_zero) {
  /*

     Displays a numeric value between 0 and 99 on screen.

     Rows are ordered on screen as:

       Row 0
       Row 1

     Includes optional blanking of first leading zero,
     i.e. displays ' 0' rather than '00'.

   */
  value = value % 100; // Maximum of two digits per row.

  // Column order is: | Column 0 | Column 1 |
  // (We process the columns in reverse order because that makes
  // extracting the digits from the value easier.)
  for (int column_number = 1; column_number >= 0; column_number--) {
    int slot_number = (row_number * 2) + column_number;
    unload_digit_image_from_slot(slot_number);
    if (!((value == 0) && (column_number == 0) && !show_first_leading_zero)) {
      load_digit_image_into_slot(slot_number, value % 10);
    }
    value = value / 10;
  }
}


unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}


void display_time(PblTm *tick_time) {

  // TODO: Use `units_changed` and more intelligence to reduce
  //       redundant digit unload/load?
// in below function call, true mean its gonna show leading zero
  display_value(get_display_hour(tick_time->tm_hour), 0, false);
  display_value(tick_time->tm_min, 1, true);
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)t;
  (void)ctx;
  static char bottom_text[] = "PEBBLE";  
// Title text on the watch NEVER Changes
  text_layer_set_text(&text_extra_layer, bottom_text);

  display_time(t->tick_time);
}


void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "Arcade Time");
  window_stack_push(&window, true);
  // if gColor is White, its gonna pick *w.png files  if black *b.png
  window_set_background_color(&window, GColorBlack);

//Resource Map initialization
  resource_init_current_app(&APP_RESOURCES);
  
//Extra text Layer
  text_layer_init(&text_extra_layer, window.layer.frame);
  text_layer_set_text_color(&text_extra_layer, GColorWhite);
  text_layer_set_background_color(&text_extra_layer, GColorBlack);
  layer_set_frame(&text_extra_layer.layer, GRect(25, 140, 90, 168));
  text_layer_set_font(&text_extra_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_C_20)));
  layer_add_child(&window.layer, &text_extra_layer.layer);  

  // Avoids a blank screen on watch start.
  PblTm tick_time;

  get_time(&tick_time);
  display_time(&tick_time);
}


void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
    unload_digit_image_from_slot(i);
  }

}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
